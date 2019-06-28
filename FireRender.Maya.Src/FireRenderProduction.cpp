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

#include "TileRenderer.h"

#include "RenderStampUtils.h"

#ifdef OPTIMIZATION_CLOCK
	#include <chrono>
#endif

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

bool FireRenderProduction::isCancelled() const
{
	return m_cancelled;
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
		m_region = RenderRegion(m_width, m_height);

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

	int contextWidth = m_width;
	int contextHeight = m_height;

	RenderRegion region = m_region;

	if (m_globals.tileRenderingEnabled)
	{
		contextWidth = m_globals.tileSizeX;
		contextHeight = m_globals.tileSizeY;

		region = RenderRegion(contextWidth, contextHeight);
	}

	//m_isRegion = true;

	auto ret = FireRenderThread::RunOnceAndWait<bool>([this, &showWarningDialog, contextWidth, contextHeight]()
	{
		{
			AutoMutexLock contextCreationLock(m_contextCreationLock);

			m_context = make_shared<FireRenderContext>();
			m_context->SetRenderType(RenderType::ProductionRender);
		}

		if (m_globals.adaptiveThreshold > 0.0f)
		{
			m_context->enableAOV(RPR_AOV_VARIANCE);
		}

		m_aovs->applyToContext(*m_context);

		// Stop before restarting if already running.
		if (m_isRunning)
			stop();

		// Check dimensions are valid.
		if (m_width == 0 || m_height == 0)
			return false;

		m_context->setCallbackCreationDisabled(true);
		if (!m_context->buildScene(false, false, false, false))
		{
			return false;
		}

		m_needsContextRefresh = true;
		m_context->setResolution(contextWidth, contextHeight, true);
		m_context->setCamera(m_camera, true);

		m_context->setUseRegion(m_isRegion);

		if (m_context->isFirstIterationAndShadersNOTCached())
			showWarningDialog = true;	//first iteration and shaders are _NOT_ cached

		if (m_isRegion)
			m_context->setRenderRegion(m_region);

		return true;
	});

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
		m_aovs->setRegion(region, contextWidth, contextHeight);
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

		refreshContext();
		m_needsContextRefresh = false;

		m_context->setStartedRendering();

		// Start the render
		FireRenderThread::KeepRunning([this]()
		{
			try
			{
				if (m_globals.tileRenderingEnabled)
				{
					RenderTiles();
					stop();
					m_isRunning = false;
				}
				else
				{
					m_isRunning = RunOnViewportThread();
				}
			}
			catch (...)
			{
				// We should stop rendering if some exception(error on core side) occured. It will close all progress bars etc
				stop();
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
		if (m_context)
			m_context->state = FireRenderContext::StateExiting;

		stopMayaRender();

		FireRenderThread::RunProcOnMainThread([&]()
		{
			m_stopCallback(m_aovs, m_settings);
			m_progressBars.reset();

			std::string renderStampText = RenderStampUtils::FormatRenderStamp(*m_context, "\\nRender Time: %pt Passes: %pp");

			MString command;
			command.format("renderWindowEditor -e -pcaption \"^1s\" renderView", renderStampText.c_str());

			MGlobal::executeCommandOnIdle(command);
		});

		if (m_context)
		{
			if (FireRenderThread::AreWeOnMainThread())
			{
				// Try-lock context lock. If can't lock it then RPR thread is rendering - run item queue
				while (!m_contextLock.tryLock())
				{
					FireRenderThread::RunItemsQueuedForTheMainThread();
				}

				m_context->cleanScene();

				m_contextLock.unlock();
			}
			else
			{
				std::shared_ptr<FireRenderContext> refToKeepAlive = m_context;
				m_context.reset();

				refToKeepAlive->cleanSceneAsync(refToKeepAlive);
			}
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
		if (m_cancelled || m_context->keepRenderRunning() == false)
		{
			stop();

			return false;
		}

		try
		{	// Render.
			AutoMutexLock contextLock(m_contextLock);
			if (m_context->state != FireRenderContext::StateRendering) 
				return false;

			RenderFullFrame();
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

void FireRenderProduction::RenderTiles()
{
	TileRenderer tileRenderer;

	TileRenderInfo info;

	info.tilesFillType = TileRenderFillType::Normal;
	info.tileSizeX = m_globals.tileSizeX;
	info.tileSizeY = m_globals.tileSizeY;

	info.totalWidth = m_width;
	info.totalHeight = m_height;

	m_context->setSamplesPerUpdate(m_globals.completionCriteriaFinalRender.completionCriteriaMaxIterations);

	// we need to resetup camera because total width and height differs with tileSizeX and tileSizeY
	m_context->camera().TranslateCameraExplicit(info.totalWidth, info.totalHeight);

	tileRenderer.Render(*m_context, info, [this](RenderRegion& region, int progress)
	{
		// make proper size
		unsigned int width = region.getWidth();
		unsigned int height = region.getHeight();

		m_context->resize(width, height, true);

		m_aovs->setRegion(RenderRegion(width, height), region.getWidth(), region.getHeight());
		m_aovs->allocatePixels();

		m_context->render(false);

		// Read pixel data for the AOV displayed in the render
		// view. Flip the image so it's the right way up in the view.
		m_renderViewAOV->readFrameBuffer(*m_context, true);

		FireRenderThread::RunProcOnMainThread([this, region]()
		{
			// Update the Maya render view.

			MRenderView::updatePixels(region.left, region.right,
			region.bottom, region.top, m_renderViewAOV->pixels.get(), true);

			// Refresh the render view.
			MRenderView::refresh(region.left, region.right, region.bottom, region.top);

			if (rcWarningDialog.shown)
				rcWarningDialog.close();
		});

		m_context->setProgress(progress);

		bool isContinue = !m_cancelled;

		if (isContinue)
		{
			m_context->setStartedRendering();
		}

		return isContinue;
	}
	);
}

void FireRenderProduction::RenderFullFrame()
{
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

	// _TODO Investigate this, looks like this call is performance waste. Why we need to read all AOVs on every render call ?
	m_aovs->readFrameBuffers(*m_context, false);

	if (globalRenderUtilsDataHolder::GetGlobalRenderUtilsDataHolder()->IsSavingIntermediateEnabled())
	{
		bool shouldSave = globalRenderUtilsDataHolder::GetGlobalRenderUtilsDataHolder()->ShouldSaveFrame(m_context->m_currentIteration);

		if (shouldSave)
		{
			bool colorOnly = true;
			unsigned int imageFormat = 8; // "jpg"
			MString filePath = globalRenderUtilsDataHolder::GetGlobalRenderUtilsDataHolder()->FolderPath().c_str();
			filePath += m_context->m_currentIteration;
			filePath += ".jpg";
			m_renderViewAOV->writeToFile(filePath, colorOnly, imageFormat);
		}
	}
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
			m_region.bottom, m_region.top, false, true);
	// Otherwise, start a full render.
	else
		MRenderView::startRender(m_width, m_height, false, true);

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
#ifdef OPTIMIZATION_CLOCK
	auto start = std::chrono::steady_clock::now();
#endif

	if (!m_context->isDirty())
		return;

	try
	{
		m_context->Freshen(false,
			[this]() -> bool
		{
			return m_cancelled;
		});
	}
	catch (...)
	{
		stop();
		m_isRunning = false;
		m_error.set(current_exception());
	}

#ifdef OPTIMIZATION_CLOCK
	auto end = std::chrono::steady_clock::now();
	auto elapsed = duration_cast<milliseconds>(end - start);
	int ms = elapsed.count();
	LogPrint("time spent in refreshContext() = %d ms", ms);
#endif
}
