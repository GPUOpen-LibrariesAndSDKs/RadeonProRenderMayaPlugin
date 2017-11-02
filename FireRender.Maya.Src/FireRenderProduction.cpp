#include "FireRenderProduction.h"
#include <tbb/atomic.h>
#include <maya/MRenderView.h>
#include <maya/MViewport2Renderer.h>
#include <maya/MGlobal.h>
#include "FireRenderThread.h"
#include "AutoLock.h"
#include <thread>
#include <maya/MCommonRenderSettingsData.h>
#include <maya/MRenderUtil.h>

#include "FireRenderGlobals.h"
#include "FireRenderUtils.h"

using namespace std;
using namespace std::chrono;
using namespace RPR;
using namespace FireMaya;

// Life Cycle
// -----------------------------------------------------------------------------
FireRenderProduction::FireRenderProduction() :
	m_width(0),
	m_height(0),
	m_isRunning(false),
	m_isPaused(false),
	m_isRegion(false),
	m_renderStarted(false),
	m_needsContextRefresh(false),
	m_progressBars()
{
	m_renderViewUpdateScheduled = false;
}

// -----------------------------------------------------------------------------
FireRenderProduction::~FireRenderProduction()
{
	if (m_isRunning)
		stop();
}


// Public Methods
// -----------------------------------------------------------------------------
bool FireRenderProduction::isRunning()
{
	return m_isRunning;
}

// -----------------------------------------------------------------------------
bool FireRenderProduction::isError()
{
	return m_error.check();
}

// -----------------------------------------------------------------------------
void FireRenderProduction::updateRegion()
{
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
			// was causing deadlock: AutoMutexLock contextLock(m_contextLock);
			m_context->setUseRegion(m_isRegion);

			if (m_isRegion)
				m_context->setRenderRegion(m_region);

			// Invalidate the context to restart the render.
			m_context->setDirty();
		});
	}

	// Restart the Maya render if currently running.
	if (m_isRunning)
		startMayaRender();
}

// -----------------------------------------------------------------------------
void FireRenderProduction::setResolution(unsigned int w, unsigned int h)
{
	m_width = w;
	m_height = h;
}

void FireRenderProduction::setRenderRegion(RenderRegion & value)
{
	m_region = value;
}

// -----------------------------------------------------------------------------
void FireRenderProduction::setCamera(MDagPath& camera)
{
	m_camera = camera;
	MRenderView::setCurrentCamera(camera);
}

// -----------------------------------------------------------------------------
bool FireRenderProduction::start()
{
	MStatus status;

	bool showWarningDialog = false;

	// Read common render settings.
	MRenderUtil::getCommonRenderSettings(m_settings);

	// Read RPR globals.
	m_globals.readFromCurrentScene();
	MString renderStamp;
	if (m_globals.useRenderStamp)
		renderStamp = m_globals.renderStampText;

	m_aovs = &m_globals.aovs;

	auto ret = FireRenderThread::RunOnceAndWait<bool>([this, &showWarningDialog]()
	{
		{
			AutoMutexLock contextCreationLock(m_contextCreationLock);

			m_context = make_unique<FireRenderContext>();
		}

		m_aovs->applyToContext(*m_context);

		// Stop before restarting if already running.
		if (m_isRunning)
			stop();

		// Check dimensions are valid.
		if (m_width == 0 || m_height == 0)
			return false;

		m_context->setCallbackCreationDisabled(true);
		m_context->buildScene(false, false, false, false);
		m_needsContextRefresh = true;
		m_context->setResolution(m_width, m_height, true);
		m_context->setCamera(m_camera, true);
		m_context->setStartedRendering();
		m_context->setUseRegion(m_isRegion);

		if (m_context->isFirstIterationAndShadersNOTCached())
			showWarningDialog = true;	//first iteration and shaders are _NOT_ cached

		if (m_isRegion)
			m_context->setRenderRegion(m_region);

		return true;
	});

	// Tell the Maya view that rendering has started.
	if (m_isRegion)
		MRenderView::startRegionRender(m_width, m_height,
			m_region.left, m_region.right, m_region.bottom, m_region.top,
			false, true);
	else
		MRenderView::startRender(m_width, m_height, false, true);

	if (ret)
	{
		// Initialize the render progress bar UI.
		m_progressBars = make_unique<RenderProgressBars>(m_context->isUnlimited());

		FireRenderThread::KeepRunningOnMainThread([this]() -> bool { return mainThreadPump(); });

		// Allocate space for region pixels. This is equivalent to
		// the size of the entire frame if a region isn't specified.

		// Setup render stamp
		m_aovs->setRenderStamp(renderStamp);

		// Allocate memory for active AOV pixels and get
		// the AOV that will be displayed in the render view.
		m_aovs->setRegion(m_region, m_width, m_height);
		m_aovs->allocatePixels();
		m_renderViewAOV = &m_aovs->getRenderViewAOV();

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
bool FireRenderProduction::pause(bool value)
{
	m_isPaused = value;

	if (m_isPaused)
	{
		m_context->state = FireRenderContext::StatePaused;
	}
	else
	{
		m_context->state = FireRenderContext::StateRendering;

		m_needsContextRefresh = true;
	}

	return true;
}

// -----------------------------------------------------------------------------
bool FireRenderProduction::stop()
{
	if (m_isRunning)
	{
		if(m_context)
			m_context->state = FireRenderContext::StateExiting;

		stopMayaRender();

		FireRenderThread::RunProcOnMainThread([&]()
		{
			m_stopCallback(m_aovs, m_settings);

			m_progressBars.reset();
		});

		if (m_context)
		{
			FireRenderThread::RunOnceProcAndWait([&]()
			{
				AutoMutexLock contextLock(m_contextLock);
				m_context->cleanScene();

				AutoMutexLock contextCreationLock(m_contextCreationLock);
				m_context.reset();
			});
		}
	}

	return true;
}

// -----------------------------------------------------------------------------
bool FireRenderProduction::RunOnViewportThread()
{
	RPR_THREAD_ONLY;

	switch (m_context->state)
	{
		// The context is exiting.
	case FireRenderContext::StateExiting:
		return false;


		// The context is rendering.
	case FireRenderContext::StateRendering:
	{
		if (m_needsContextRefresh)
		{
			refreshContext();

			m_needsContextRefresh = false;
		}

		if (m_cancelled || m_context->keepRenderRunning() == false)
		{
			stop();

			return false;
		}

		try
		{	// Render.
			AutoMutexLock contextLock(m_contextLock);
			m_context->render(false);

			m_context->updateProgress();

			// Read pixel data for the AOV displayed in the render
			// view. Flip the image so it's the right way up in the view.
			m_renderViewAOV->readFrameBuffer(*m_context, true);

			FireRenderThread::RunProcOnMainThread([this]()
			{
				// Update the Maya render view.
				m_renderViewAOV->sendToRenderView();

				if (rcWarningDialog.shown)
					rcWarningDialog.close();
			});

			// Ensure display gamma correction is enabled for image file output. It
			// may be disabled initially if it's not set to be applied to Maya views.
			m_context->enableDisplayGammaCorrection(m_globals);
			m_aovs->readFrameBuffers(*m_context, false);
		}
		catch (...)
		{
			throw;
		}

		return true;
	}

	case FireRenderContext::StatePaused:
	case FireRenderContext::StateUpdating:
	default:
		return true;
	}
}

// -----------------------------------------------------------------------------
void FireRenderProduction::updateRenderView()
{
	// Clear the scheduled flag.
	m_renderViewUpdateScheduled = false;

	// Check that rendering is still active.
	if (m_context->state != FireRenderContext::StateRendering)
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

void FireRenderProduction::setStopCallback(stop_callback callback)
{
	m_stopCallback = callback;
}

void FireRenderProduction::waitForIt()
{
	while (m_isRunning)
	{
		FireRenderThread::RunItemsQueuedForTheMainThread();

		this_thread::sleep_for(10ms);
	}
}

// -----------------------------------------------------------------------------
void FireRenderProduction::scheduleRenderViewUpdate()
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
void FireRenderProduction::startMayaRender()
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
	m_cancelled = false;

	pause(false);
}

// -----------------------------------------------------------------------------
void FireRenderProduction::stopMayaRender()
{
	// Check that a render has started.
	if (!m_renderStarted)
		return;

	m_renderStarted = false;
}

// -----------------------------------------------------------------------------
void FireRenderProduction::beginMayaUpdate()
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
void FireRenderProduction::endMayaUpdate()
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
void FireRenderProduction::readFrameBuffer()
{
	RPR_THREAD_ONLY;

	m_context->readFrameBuffer(m_pixels.data(),
		RPR_AOV_COLOR,
		m_context->width(),
		m_context->height(),
		m_region,
		true);
}

bool FireRenderProduction::mainThreadPump()
{
	MAIN_THREAD_ONLY;

	if (m_isRunning)
	{
		AutoMutexLock contextCreationLock(m_contextCreationLock);

		if (m_progressBars && m_context)
		{
			m_progressBars->update(m_context->getProgress());

			m_cancelled = m_progressBars->isCancelled();

			if (m_cancelled)
			{
				DebugPrint("Rendering canceled!");

				m_progressBars.reset();
			}
		}

		return !m_cancelled;
	}

	return false;
}

// -----------------------------------------------------------------------------
void FireRenderProduction::refreshContext()
{
	if (!m_context->isDirty())
		return;

	m_context->Freshen(false,
		[this]() -> bool
	{
		return m_cancelled;
	});
}
