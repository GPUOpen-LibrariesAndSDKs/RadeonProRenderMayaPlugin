#include "FireRenderIpr.h"
#include <tbb/atomic.h>
#include <maya/MRenderView.h>
#include <maya/MViewport2Renderer.h>
#include <maya/MGlobal.h>
#include "FireRenderThread.h"
#include "AutoLock.h"
#include <thread>
#include <mutex>

#include "FireRenderUtils.h"
#include "maya/MItSelectionList.h"

using namespace std;
using namespace std::chrono;
using namespace RPR;
using namespace FireMaya;

// Life Cycle
// -----------------------------------------------------------------------------
FireRenderIpr::FireRenderIpr() :
	m_width(0),
	m_height(0),
	m_isRunning(false),
	m_isPaused(false),
	m_isRegion(false),
	m_renderStarted(false),
	m_needsContextRefresh(false),
	m_previousSelectionList()
{
	m_renderViewUpdateScheduled = false;
	m_context.m_RenderType = RenderType::IPR;
}

// -----------------------------------------------------------------------------
FireRenderIpr::~FireRenderIpr()
{
	stop();
	m_context.cleanScene();
	m_previousSelectionList.clear();
}


// Public Methods
// -----------------------------------------------------------------------------
bool FireRenderIpr::isRunning()
{
	return m_isRunning;
}

// -----------------------------------------------------------------------------
bool FireRenderIpr::isError()
{
	return m_error.check();
}

// -----------------------------------------------------------------------------
void FireRenderIpr::updateRegion()
{
	// We need to sync this function with readFrameBuffer since they operate on
	// same data (m_region and m_pixels).
	// Without this syncronization race condition may occur for m_pixels that will
	// lead to crash in readFrameBuffer function
	std::lock_guard<std::mutex> guard(m_regionUpdateMutex);

	// Get the render view region if any.
	MRenderView::getRenderRegion(
		m_region.left, m_region.right,
		m_region.bottom, m_region.top);

	// Check that a region was specified
	// and it is not the full render area.
	m_isRegion =
		!m_region.isZeroArea() &&
		!(m_region.getWidth() == m_width && m_region.getHeight() == m_height);

	// Default to the full render view if the region is invalid.
	if (!m_isRegion)
		m_region = RenderRegion(0, m_width - 1, m_height - 1, 0);

	// Ensure the pixel buffer is the correct size.
	m_pixels.resize(m_region.getArea());

	// Instruct the context to render to a region if required.
	if (m_isRunning)
	{
		FireRenderThread::RunOnceProcAndWait([this]()
		{
			m_context.setUseRegion(m_isRegion);

			if (m_isRegion)
				m_context.setRenderRegion(m_region);

			// Invalidate the context to restart the render.
			m_context.setDirty();
		});
	}

	// Restart the Maya render if currently running.
	if (m_isRunning)
		startMayaRender();
}

// -----------------------------------------------------------------------------
void FireRenderIpr::setResolution(unsigned int w, unsigned int h)
{
	m_width = w;
	m_height = h;
}

// -----------------------------------------------------------------------------
void FireRenderIpr::setCamera(MDagPath& camera)
{
	MAIN_THREAD_ONLY;
	m_camera = camera;
	MRenderView::setCurrentCamera(camera);
}

// -----------------------------------------------------------------------------
bool FireRenderIpr::start()
{
	bool showWarningDialog = false;

	MStatus status = MGlobal::getActiveSelectionList(m_previousSelectionList);
	CHECK_MSTATUS(status);

	auto ret = FireRenderThread::RunOnceAndWait<bool>([this, &showWarningDialog]()
	{
		// Stop before restarting if already running.
		if (m_isRunning)
			stop();

		// Check dimensions are valid.
		if (m_width == 0 || m_height == 0)
			return false;

		//enable AOV-COLOR and Variance so that it can be resolved and used properly
		m_context.enableAOV(RPR_AOV_COLOR);
		m_context.enableAOV(RPR_AOV_VARIANCE);

		// Enable SC related AOVs if they was turned on
		FireRenderGlobalsData globals;
		globals.readFromCurrentScene();

		if (globals.aovs.getAOV(RPR_AOV_OPACITY).active)
			m_context.enableAOV(RPR_AOV_OPACITY);
		if (globals.aovs.getAOV(RPR_AOV_BACKGROUND).active)
			m_context.enableAOV(RPR_AOV_BACKGROUND);
		if (globals.aovs.getAOV(RPR_AOV_SHADOW_CATCHER).active)
			m_context.enableAOV(RPR_AOV_SHADOW_CATCHER);
		
		if (!m_context.buildScene(false, false, false, false))
		{
			return false;
		}

		SetupOOC(globals);

		m_needsContextRefresh = true;
		m_context.setResolution(m_width, m_height, true);
		m_context.setCamera(m_camera, true);
		m_context.setStartedRendering();
		m_context.setUseRegion(m_isRegion);

		if (m_context.isFirstIterationAndShadersNOTCached())
			showWarningDialog = true;	//first iteration and shaders are _NOT_ cached

		if (m_isRegion)
			m_context.setRenderRegion(m_region);

		return true;
	});

	if (ret)
	{
		if (showWarningDialog)
		{
			rcWarningDialog.show();
		}

		// Start the Maya render view render.
		startMayaRender();

		// Ensure the scheduled update flag is clear initially.
		m_renderViewUpdateScheduled = false;

		m_isRunning = true;

		// Start the render
		FireRenderThread::KeepRunning([this]()
		{
			try
			{
				m_isRunning = RunOnViewportThread();
			}
			catch (...)
			{
				m_isRunning = false;
				m_error.set(current_exception());
			}

			return m_isRunning;
		});
	}

	return ret;
}

// -----------------------------------------------------------------------------
void FireRenderIpr::SetupOOC(FireRenderGlobalsData& globals)
{
	frw::Context context = m_context.GetContext();
	rpr_context frcontext = context.Handle();

	rpr_int frstatus = RPR_SUCCESS;

	if (globals.enableOOC)
	{
		rprContextSetParameter1u(frcontext, "ooctexcache", globals.oocTexCache);
	}
	else
	{
		rprContextSetParameter1u(frcontext, "ooctexcache", 0);
	}
}

// -----------------------------------------------------------------------------
bool FireRenderIpr::pause(bool value)
{
	m_isPaused = value;

	if (m_isPaused)
	{
		m_context.state = FireRenderContext::StatePaused;
	}
	else
	{
		m_context.state = FireRenderContext::StateRendering;

		m_needsContextRefresh = true;
	}

	return true;
}

// -----------------------------------------------------------------------------
bool FireRenderIpr::stop()
{
	if (m_isRunning)
	{
		m_context.state = FireRenderContext::StateExiting;

		stopMayaRender();

		while (m_isRunning)
		{
			//Run items queued for main thread
			FireRenderThread::RunItemsQueuedForTheMainThread();
			this_thread::sleep_for(1ms);
		}
	}

	return true;
}

// -----------------------------------------------------------------------------
bool FireRenderIpr::RunOnViewportThread()
{
	RPR_THREAD_ONLY;

	switch (m_context.state)
	{
		// The context is exiting.
	case FireRenderContext::StateExiting:
		return false;


		// The context is rendering.
	case FireRenderContext::StateRendering:
	{
		if (m_needsContextRefresh || m_context.isDirty())
		{
			FireRenderThread::RunProcOnMainThread([this]() {refreshContext(); });
			
			m_needsContextRefresh = false;
		}

		// Check if a render is required.
		if (m_context.needsRedraw() ||
			m_context.cameraAttributeChanged() ||
			m_context.keepRenderRunning())
		{
			try
			{
				// Render.
				AutoMutexLock contextLock(m_contextLock);
				m_context.render(false);

				// Read the frame buffer.
				AutoMutexLock pixelsLock(m_pixelsLock);
				readFrameBuffer();

				// Schedule a Maya render view on the main thread.
				scheduleRenderViewUpdate();
			}
			catch (...)
			{
				scheduleRenderViewUpdate();
				throw;
			}
		}

		return true;
	}

	case FireRenderContext::StatePaused:
	case FireRenderContext::StateUpdating:
	default:
		return true;
	}
}

void FireRenderIpr::CheckSelection()
{
	// render selected is not enabled => back off
	if (!m_context.renderSelectedObjectsOnly())
		return;

	MSelectionList currSelectionList;
	MStatus status = MGlobal::getActiveSelectionList(currSelectionList);
	CHECK_MSTATUS(status);

	bool selectionChanged = false;

	if (currSelectionList.length() != m_previousSelectionList.length())
	{
		// if number of selected objects has changed than selection have been changed
		selectionChanged = true;
	}
	else
	{
		// compare objects in prev and curr selections
		for (MItSelectionList it(currSelectionList); !it.isDone(); it.next())
		{
			MDagPath item;
			MObject component;
			MStatus status = it.getDagPath(item, component);

			CHECK_MSTATUS(status);
			if (!m_previousSelectionList.hasItem(item))
			{
				selectionChanged = true;
				break;
			}
		}
	}

	// selection changed => change rendered item(s)
	if (selectionChanged)
	{
		FireRenderThread::RunOnceProcAndWait([&]()
		{
			AutoMutexLock contextLock(m_contextLock);

			for (MItSelectionList it(currSelectionList); !it.isDone(); it.next())
			{
				MDagPath item;
				MObject component;
				MStatus status = it.getDagPath(item, component);
				item.extendToShape();
				FireRenderObject* pRObj = m_context.getRenderObject(item);
				if (pRObj)
				{
					pRObj->setDirty();
				}
			}

			for (MItSelectionList it2(m_previousSelectionList); !it2.isDone(); it2.next())
			{
				MDagPath item;
				MObject component;
				MStatus status = it2.getDagPath(item, component);
				item.extendToShape();
				FireRenderObject* pRObj = m_context.getRenderObject(item);
				if (pRObj)
				{
					pRObj->setDirty();
				}
			}
		});

		m_previousSelectionList.clear();
		m_previousSelectionList = currSelectionList;
	}
}

// -----------------------------------------------------------------------------
void FireRenderIpr::updateRenderView()
{
	// Clear the scheduled flag.
	m_renderViewUpdateScheduled = false;

	CheckSelection();

	// Check that rendering is still active.
	if (m_context.state != FireRenderContext::StateRendering)
		return;

	{
		// Acquire the pixels lock.
		AutoMutexLock pixelsLock(m_pixelsLock);

		// Prepare to update the Maya view.
		beginMayaUpdate();

		// Update the render view pixels.
		MRenderView::updatePixels(
			m_region.left, m_region.right,
			m_region.bottom, m_region.top,
			m_pixels.data(), true);

		// Refresh the render view.
		MRenderView::refresh(
			m_region.left, m_region.right,
			m_region.bottom, m_region.top);

		// Complete the Maya view update.
		endMayaUpdate();

		if (rcWarningDialog.shown) {
			rcWarningDialog.close();
		}
	}

	// Refresh the context if required.
	m_needsContextRefresh = true;
}

// -----------------------------------------------------------------------------
void FireRenderIpr::scheduleRenderViewUpdate()
{
	// Only schedule one update at a time.
	if (m_renderViewUpdateScheduled)
		return;

	// Schedule the update.
	MGlobal::executeCommandOnIdle("fireRender -ipr -updateRenderView", false);

	// Flag as scheduled.
	m_renderViewUpdateScheduled = true;
}


// Private Methods
// -----------------------------------------------------------------------------
void FireRenderIpr::startMayaRender()
{
	// Stop any active render.
	stopMayaRender();

	// Start a region render if required.
	if (m_isRegion)
		MRenderView::startRegionRender(m_width, m_height,
			m_region.left, m_region.right,
			m_region.bottom, m_region.top, true, true);
	// Otherwise, start a full render.
	else
		MRenderView::startRender(m_width, m_height, true, true);

	// Flag as started.
	m_renderStarted = true;
	pause(false);
}

// -----------------------------------------------------------------------------
void FireRenderIpr::stopMayaRender()
{
	// Check that a render has started.
	if (!m_renderStarted)
		return;

	// Stop the render and clear the started flag.
	MRenderView::endRender();
	m_renderStarted = false;
}

// -----------------------------------------------------------------------------
void FireRenderIpr::beginMayaUpdate()
{
	// No action is required for Maya versions higher than 2016.
	if (MGlobal::apiVersion() >= 201650)
		return;

#ifndef MAYA2015
	// Action is only required for the core profile renderer.
	MHWRender::MRenderer* renderer = MHWRender::MRenderer::theRenderer();
	if (renderer->drawAPI() != MHWRender::kOpenGLCoreProfile)
		return;

	// No action is required if the render has already been started.
	if (m_renderStarted)
		return;

	// Start the render.
	startMayaRender();
#endif
}

// -----------------------------------------------------------------------------
void FireRenderIpr::endMayaUpdate()
{
	// No action is required for Maya versions higher than 2016.
	if (MGlobal::apiVersion() >= 201650)
		return;

#ifndef MAYA2015

	// Action is only required for the core profile renderer.
	MHWRender::MRenderer* renderer = MHWRender::MRenderer::theRenderer();
	if (renderer->drawAPI() != MHWRender::kOpenGLCoreProfile)
		return;

	// Stop the render. This forces a render view
	// refresh that is not correctly performed by the
	// MRenderView::refresh in Maya 2016 when using the
	// Core OpenGL rendering engine. This also prevents
	// a memory leak in Maya.
	stopMayaRender();
#endif
}

// -----------------------------------------------------------------------------
void FireRenderIpr::readFrameBuffer()
{
	RPR_THREAD_ONLY;

	std::lock_guard<std::mutex> guard(m_regionUpdateMutex);

	// We need to made SC merge here, but we don't want to do opacity merge 
	// since we render in interactive mode
	m_context.readFrameBuffer(m_pixels.data(), RPR_AOV_COLOR, m_context.width(),
		m_context.height(), m_region, true, false, true);
}

// -----------------------------------------------------------------------------
void FireRenderIpr::refreshContext()
{
	if (!m_context.isDirty())
		return;

	FireRenderThread::RunOnceProcAndWait([this]()
	{
		AutoMutexLock contextLock(m_contextLock);
		m_context.Freshen(false);
	});
}
