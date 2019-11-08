#include "FireRenderProduction.h"
#include "Context/TahoeContext.h"
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
#include "RenderStampUtils.h"

#include "TileRenderer.h"
#include "Athena/athenaWrap.h"

#include "RenderStampUtils.h"
#include "GlobalRenderUtilsDataHolder.h"
#include <iostream>
#include <fstream>

#include "Context/ContextCreator.h"

#include <functional>
#include <clocale>
#include <chrono>
#include <ctime>

#include "common.h"

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
	m_progressBars(),
	m_rendersCount(0)
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
			m_isRegion = m_contextPtr->setUseRegion(m_isRegion);

			if (m_isRegion)
			{
				m_contextPtr->setRenderRegion(m_region);
			}

			// Invalidate the context to restart the render.
			m_contextPtr->setDirty();
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

	if (GlobalRenderUtilsDataHolder::GetGlobalRenderUtilsDataHolder()->IsSavingIntermediateEnabled())
	{
		GlobalRenderUtilsDataHolder::GetGlobalRenderUtilsDataHolder()->UpdateStartTime();
		std::remove(std::string(GlobalRenderUtilsDataHolder::GetGlobalRenderUtilsDataHolder()->FolderPath() + "time_log.txt").c_str());
	}

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

			m_contextPtr = ContextCreator::CreateAppropriateContextForRenderType(RenderType::ProductionRender);
			m_contextPtr->SetRenderType(RenderType::ProductionRender);
		}

		m_contextPtr->enableAOV(RPR_AOV_OPACITY);
		if (m_globals.adaptiveThreshold > 0.0f)
		{
			m_contextPtr->enableAOV(RPR_AOV_VARIANCE);
		}

		m_aovs->applyToContext(*m_contextPtr);

		// Stop before restarting if already running.
		if (m_isRunning)
			stop();

		// Check dimensions are valid.
		if (m_width == 0 || m_height == 0)
			return false;

		m_contextPtr->setCallbackCreationDisabled(true);
		if (!m_contextPtr->buildScene(false, false, false, false))
		{
			return false;
		}

		m_aovs->setFromContext(*m_contextPtr);

		m_needsContextRefresh = true;
		m_contextPtr->setResolution(contextWidth, contextHeight, true);
		m_contextPtr->setCamera(m_camera, true);

		m_contextPtr->setUseRegion(m_isRegion);

		if (m_contextPtr->isFirstIterationAndShadersNOTCached())
			showWarningDialog = true;	//first iteration and shaders are _NOT_ cached

		if (m_isRegion)
			m_contextPtr->setRenderRegion(m_region);

		return true;
	});

	if (ret)
	{
		// Initialize the render progress bar UI.
		m_progressBars = make_unique<RenderProgressBars>(m_contextPtr->isUnlimited());

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

		m_contextPtr->setStartedRendering();

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
		m_contextPtr->state = FireRenderContext::StatePaused;
	}
	else
	{
		m_contextPtr->state = FireRenderContext::StateRendering;

		m_needsContextRefresh = true;
	}

	return true;
}

// -----------------------------------------------------------------------------
bool FireRenderProduction::stop()
{
	if (m_isRunning)
	{
		if (m_contextPtr)
			m_contextPtr->state = FireRenderContext::StateExiting;

		stopMayaRender();

		FireRenderThread::RunProcOnMainThread([&]()
		{
			m_stopCallback(m_aovs, m_settings);
			m_progressBars.reset();

			std::string renderStampText = RenderStampUtils::FormatRenderStamp(*m_contextPtr, "\\nFrame: %f  Render Time: %pt  Passes: %pp");

			MString command;
			command.format("renderWindowEditor -e -pcaption \"^1s\" renderView", renderStampText.c_str());

			MGlobal::executeCommandOnIdle(command);

			RenderStampUtils::ClearCache();
		});

		if (m_contextPtr)
		{
			if (FireRenderThread::AreWeOnMainThread())
			{
				// Try-lock context lock. If can't lock it then RPR thread is rendering - run item queue
				while (!m_contextLock.tryLock())
				{
					FireRenderThread::RunItemsQueuedForTheMainThread();
				}

				m_contextPtr->cleanScene();

				m_contextLock.unlock();
			}
			else
			{
				std::shared_ptr<FireRenderContext> refToKeepAlive = m_contextPtr;
				m_contextPtr.reset();

				refToKeepAlive->cleanSceneAsync(refToKeepAlive);
			}
		}
	}

	return true;
}

// -----------------------------------------------------------------------------

void FireRenderProduction::UploadAthenaData()
{
	AthenaWrapper* pAthenaWrapper = AthenaWrapper::GetAthenaWrapper();

	std::ostringstream data_field;
	std::ostringstream header;

	header.str(std::string());
	header.clear();
	data_field.str(std::string());
	data_field.clear();

	// operating system
	header << "OS";

#if defined(_WIN32)
	data_field << "Windows";

#elif defined(__APPLE__)
	data_field << "macOS";

#elif defined(__linux__)
	data_field << "Linux";

#endif

	pAthenaWrapper->WriteField(header.str(), data_field.str());

	// plug-in version
	data_field.str(std::string());
	data_field.clear();
	data_field << PLUGIN_VERSION;

	header.str(std::string());
	header.clear();
	header << "RPR plug-in version";

	pAthenaWrapper->WriteField(header.str(), data_field.str());

	// core version
#ifdef RPR_VERSION_MAJOR_MINOR_REVISION
	std::ostringstream oss;
	oss << RPR_VERSION_MAJOR << "." << RPR_VERSION_MINOR << RPR_VERSION_REVISION;
#else
	int mj = (RPR_API_VERSION & 0xFFFF00000) >> 28;
	int mn = (RPR_API_VERSION & 0xFFFFF) >> 8;

	std::ostringstream oss;
	oss << std::hex << mj << "." << mn;
#endif
	header.str(std::string());
	header.clear();
	header << "RPR Core version";
	pAthenaWrapper->WriteField(header.str(), oss.str());

	// host application
	data_field.str(std::string());
	data_field.clear();
	data_field << "Maya";

	header.str(std::string());
	header.clear();
	header << "Host application";
	pAthenaWrapper->WriteField(header.str(), data_field.str());

	// render time
	header.str(std::string());
	header.clear();
	data_field.str(std::string());
	data_field.clear();
	data_field << m_contextPtr->m_secondsSpentOnLastRender;
	header << "Seconds spent on render";
	pAthenaWrapper->WriteField(header.str(), data_field.str());

	// device used
	header.str(std::string());
	header.clear();

	std::string deviceName;

	int renderDevice = RenderStampUtils::GetRenderDevice();

	switch (renderDevice)
	{
		case RenderStampUtils::RPR_RENDERDEVICE_CPUONLY:
		{
			deviceName += RenderStampUtils::GetCPUNameString();
			break;
		}

		case RenderStampUtils::RPR_RENDERDEVICE_GPUONLY:
		{
			deviceName += RenderStampUtils::GetFriendlyUsedGPUName();
			break; 
		}

		default: // CPU+GPU
			deviceName += std::string(RenderStampUtils::GetCPUNameString()) + " / " + RenderStampUtils::GetFriendlyUsedGPUName();
	}

	header << "Devices used";
	pAthenaWrapper->WriteField(header.str(), deviceName);

	// polygon count
	data_field.str(std::string());
	data_field.clear();
	data_field << GetScenePolyCount();

	header.str(std::string());
	header.clear();
	header << "Polygon count";

	pAthenaWrapper->WriteField(header.str(), data_field.str());

	// render resolution
	header.str(std::string());
	header.clear();
	header << "Image resolution";

	data_field.str(std::string());
	data_field.clear();
	data_field << "Width: " << m_width << "; Height: " << m_height;

	pAthenaWrapper->WriteField(header.str(), data_field.str());

	// render result
	header.str(std::string());
	header.clear();
	header << "End status";

	data_field.str(std::string());
	data_field.clear();

	switch (m_contextPtr->m_lastRenderResultState)
	{
		case FireRenderContext::COMPLETED:
		{
			data_field << "successfully completed";
			break;
		}

		case FireRenderContext::CANCELED:
		{
			data_field << "killed by user";
			break;
		}

		case FireRenderContext::CRASHED:
		{
			data_field << "crashed";
			break;
		}

		default:
			data_field << "error!";
	}
	pAthenaWrapper->WriteField(header.str(), data_field.str());

	// locale
	header.str(std::string());
	header.clear();
	header << "Locale";

	data_field.str(std::string());
	data_field.clear();
	char* currLocale = std::setlocale(LC_NUMERIC, "");
	data_field << currLocale;

	pAthenaWrapper->WriteField(header.str(), data_field.str());

	// lights
	header.str(std::string());
	header.clear();
	header << "Lights count";

	data_field.str(std::string());
	data_field.clear();
	int countLights = m_contextPtr->GetScene().ShapeObjectCount();
	data_field << countLights;

	pAthenaWrapper->WriteField(header.str(), data_field.str());

	// time and date
	header.str(std::string());
	header.clear();
	header << "Finish time and date";

	data_field.str(std::string());
	data_field.clear();
	auto curr = std::chrono::system_clock::now();
	std::time_t currTime = std::chrono::system_clock::to_time_t(curr);
	data_field << std::ctime(&currTime);

	pAthenaWrapper->WriteField(header.str(), data_field.str());

	// aov's
	header.str(std::string());
	header.clear();
	header << "Enabled AOV's";

	data_field.str(std::string());
	data_field.clear();

	static std::map<unsigned int, std::string> aovNames =
	{
		 {RPR_AOV_COLOR, "RPR_AOV_COLOR"}
		,{RPR_AOV_OPACITY, "RPR_AOV_OPACITY" }
		,{RPR_AOV_WORLD_COORDINATE, "RPR_AOV_WORLD_COORDINATE" }
		,{RPR_AOV_UV, "RPR_AOV_UV" }
		,{RPR_AOV_MATERIAL_IDX, "RPR_AOV_MATERIAL_IDX" }
		,{RPR_AOV_GEOMETRIC_NORMAL, "RPR_AOV_GEOMETRIC_NORMAL" }
		,{RPR_AOV_SHADING_NORMAL, "RPR_AOV_SHADING_NORMAL" }
		,{RPR_AOV_DEPTH, "RPR_AOV_DEPTH" }
		,{RPR_AOV_OBJECT_ID, "RPR_AOV_OBJECT_ID" }
		,{RPR_AOV_OBJECT_GROUP_ID, "RPR_AOV_OBJECT_GROUP_ID" }
		,{RPR_AOV_SHADOW_CATCHER, "RPR_AOV_SHADOW_CATCHER" }
		,{RPR_AOV_BACKGROUND, "RPR_AOV_BACKGROUND" }
		,{RPR_AOV_EMISSION, "RPR_AOV_EMISSION" }
		,{RPR_AOV_VELOCITY, "RPR_AOV_VELOCITY" }
		,{RPR_AOV_DIRECT_ILLUMINATION, "RPR_AOV_DIRECT_ILLUMINATION" }
		,{RPR_AOV_INDIRECT_ILLUMINATION, "RPR_AOV_INDIRECT_ILLUMINATION"}
		,{RPR_AOV_AO, "RPR_AOV_AO" }
		,{RPR_AOV_DIRECT_DIFFUSE, "RPR_AOV_DIRECT_DIFFUSE" }
		,{RPR_AOV_DIRECT_REFLECT, "RPR_AOV_DIRECT_REFLECT" }
		,{RPR_AOV_INDIRECT_DIFFUSE, "RPR_AOV_INDIRECT_DIFFUSE" }
		,{RPR_AOV_INDIRECT_REFLECT, "RPR_AOV_INDIRECT_REFLECT" }
		,{RPR_AOV_REFRACT, "RPR_AOV_REFRACT" }
		,{RPR_AOV_VOLUME, "RPR_AOV_VOLUME" }
		,{RPR_AOV_LIGHT_GROUP0, "RPR_AOV_LIGHT_GROUP0" }
		,{RPR_AOV_LIGHT_GROUP1, "RPR_AOV_LIGHT_GROUP1" }
		,{RPR_AOV_LIGHT_GROUP2, "RPR_AOV_LIGHT_GROUP2" }
		,{RPR_AOV_LIGHT_GROUP3, "RPR_AOV_LIGHT_GROUP3" }
		,{RPR_AOV_DIFFUSE_ALBEDO, "RPR_AOV_DIFFUSE_ALBEDO" }
		,{RPR_AOV_VARIANCE, "RPR_AOV_VARIANCE" }
		,{RPR_AOV_VIEW_SHADING_NORMAL, "RPR_AOV_VIEW_SHADING_NORMAL" }
		,{RPR_AOV_REFLECTION_CATCHER, "RPR_AOV_REFLECTION_CATCHER" }
		,{RPR_AOV_MAX, "RPR_AOV_MAX" }
	};

	for (int aovID = 0; aovID != RPR_AOV_MAX; aovID++)
		if (m_contextPtr->isAOVEnabled(aovID))
			data_field << aovNames[aovID] << "; ";

	pAthenaWrapper->WriteField(header.str(), data_field.str());

	// completed iterations
	header.str(std::string());
	header.clear();
	header << "Completed iterations count";

	data_field.str(std::string());
	data_field.clear();
	data_field << m_contextPtr->m_currentIteration;

	pAthenaWrapper->WriteField(header.str(), data_field.str());
}

size_t FireRenderProduction::GetScenePolyCount() const
{
	size_t shapeCount = 0;
	auto status = rprSceneGetInfo(m_contextPtr->scene(), RPR_SCENE_SHAPE_LIST, 0, nullptr, &shapeCount);
	shapeCount /= sizeof(rpr_shape);
	if (status != RPR_SUCCESS || shapeCount == 0) return 0;

	std::vector<rpr_shape> shapes(shapeCount);
	status = rprSceneGetInfo(m_contextPtr->scene(), RPR_SCENE_SHAPE_LIST, shapeCount * sizeof(rpr_shape), shapes.data(), nullptr);
	return std::accumulate(std::begin(shapes), std::end(shapes), (size_t)0, [](size_t accumulator, const rpr_shape shape) 
		{
			size_t polygonCount = 0;
			rprMeshGetInfo(shape, RPR_MESH_POLYGON_COUNT, sizeof(polygonCount), &polygonCount, nullptr);
			return accumulator + polygonCount;
		});
}

bool FireRenderProduction::RunOnViewportThread()
{
	RPR_THREAD_ONLY;

	switch (m_contextPtr->state)
	{
		// The context is exiting.
		case FireRenderContext::StateExiting:
		{
			m_rendersCount++;
			return false;
		}

		// The context is rendering.
		case FireRenderContext::StateRendering:
		{
			if (m_cancelled || m_contextPtr->keepRenderRunning() == false)
			{
				m_contextPtr->m_lastRenderResultState = (m_cancelled) ? FireRenderContext::CANCELED : FireRenderContext::COMPLETED;

				AthenaWrapper::GetAthenaWrapper()->StartNewFile();
				UploadAthenaData();
				
				AthenaWrapper::GetAthenaWrapper()->AthenaSendFile(pythonCallWrap);

				m_contextPtr->m_polycountLastRender = 0;

				stop();
				m_rendersCount++;

				return false;
			}

			try
			{	// Render.
				AutoMutexLock contextLock(m_contextLock);
				if (m_contextPtr->state != FireRenderContext::StateRendering) 
					return false;

				//m_context->m_lastRenderResultState = FireRenderContext::CRASHED; // need to set before crash happens

				RenderFullFrame();
			}
			catch (...)
			{
				m_contextPtr->m_lastRenderResultState = FireRenderContext::CRASHED;

				AthenaWrapper::GetAthenaWrapper()->StartNewFile();
				UploadAthenaData();

				AthenaWrapper::GetAthenaWrapper()->AthenaSendFile(pythonCallWrap);

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

	m_contextPtr->setSamplesPerUpdate(m_globals.completionCriteriaFinalRender.completionCriteriaMaxIterations);

	// we need to resetup camera because total width and height differs with tileSizeX and tileSizeY
	m_contextPtr->camera().TranslateCameraExplicit(info.totalWidth, info.totalHeight);

	tileRenderer.Render(*m_contextPtr, info, [this](RenderRegion& region, int progress)
	{
		// make proper size
		unsigned int width = region.getWidth();
		unsigned int height = region.getHeight();

		m_contextPtr->resize(width, height, true);

		m_aovs->setRegion(RenderRegion(width, height), region.getWidth(), region.getHeight());
		m_aovs->allocatePixels();

		m_contextPtr->render(false);

		// Read pixel data for the AOV displayed in the render
		// view. Flip the image so it's the right way up in the view.
		m_renderViewAOV->readFrameBuffer(*m_contextPtr, true);

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

		m_contextPtr->setProgress(progress);

		bool isContinue = !m_cancelled;

		if (isContinue)
		{
			m_contextPtr->setStartedRendering();
		}

		return isContinue;
	}
	);

	AthenaWrapper::GetAthenaWrapper()->StartNewFile();
	UploadAthenaData();
	AthenaWrapper::GetAthenaWrapper()->AthenaSendFile(pythonCallWrap);

	int debugi = 1;
}

void FireRenderProduction::RenderFullFrame()
{
	m_contextPtr->render(false);

	m_contextPtr->updateProgress();

	// Read pixel data for the AOV displayed in the render
	// view. Flip the image so it's the right way up in the view.
	m_renderViewAOV->readFrameBuffer(*m_contextPtr, true);

	FireRenderThread::RunProcOnMainThread([this]()
	{
		// Update the Maya render view.
		m_renderViewAOV->sendToRenderView();

		if (rcWarningDialog.shown)
			rcWarningDialog.close();
	});

	// Ensure display gamma correction is enabled for image file output. It
	// may be disabled initially if it's not set to be applied to Maya views.
	m_contextPtr->enableDisplayGammaCorrection(m_globals);

	// _TODO Investigate this, looks like this call is performance waste. Why we need to read all AOVs on every render call ?
	m_aovs->readFrameBuffers(*m_contextPtr, false);

	if (GlobalRenderUtilsDataHolder::GetGlobalRenderUtilsDataHolder()->IsSavingIntermediateEnabled())
	{
		bool shouldSave = GlobalRenderUtilsDataHolder::GetGlobalRenderUtilsDataHolder()->ShouldSaveFrame(m_contextPtr->m_currentIteration);

		if (shouldSave)
		{
			bool colorOnly = true;
			unsigned int imageFormat = 8; // "jpg"
			MString filePath = GlobalRenderUtilsDataHolder::GetGlobalRenderUtilsDataHolder()->FolderPath().c_str();
			filePath += m_contextPtr->m_currentIteration;
			filePath += ".jpg";
			m_renderViewAOV->writeToFile(filePath, colorOnly, imageFormat);

			std::ofstream timeLoggingFile;
			timeLoggingFile.open(GlobalRenderUtilsDataHolder::GetGlobalRenderUtilsDataHolder()->FolderPath() + "time_log.txt", std::ofstream::out | std::ofstream::app);
			timeLoggingFile << m_contextPtr->m_currentIteration << " ";
			long numberOfClicks = clock() - GlobalRenderUtilsDataHolder::GetGlobalRenderUtilsDataHolder()->GetStartTime();
			double secondsSpentRendering = numberOfClicks / (double)CLOCKS_PER_SEC;
			timeLoggingFile << secondsSpentRendering << "s \n";

			timeLoggingFile.close();
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

	m_contextPtr->readFrameBuffer(m_pixels.data(),
		RPR_AOV_COLOR,
		m_contextPtr->width(),
		m_contextPtr->height(),
		m_region,
		true);
}

bool FireRenderProduction::mainThreadPump()
{
	MAIN_THREAD_ONLY;

	if (m_isRunning)
	{
		AutoMutexLock contextCreationLock(m_contextCreationLock);

		if (m_progressBars && m_contextPtr)
		{
			m_progressBars->update(m_contextPtr->getProgress());

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

	if (!m_contextPtr->isDirty())
		return;

	try
	{
		m_contextPtr->Freshen(false,
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
