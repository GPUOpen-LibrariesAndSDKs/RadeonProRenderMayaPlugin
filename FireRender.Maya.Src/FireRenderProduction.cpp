/**********************************************************************
Copyright 2020 Advanced Micro Devices, Inc
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
********************************************************************/
#include "FireRenderProduction.h"
#include "Context/TahoeContext.h"
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
#include "RenderViewUpdater.h"

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

#if defined(_WIN32)
#include "athenaSystemInfo_Win.h"
#endif

#if defined(__APPLE__)
#include "athenaSystemInfo_Mac.h"
#include <sys/sysctl.h>
#endif  // defined(__APPLE__)

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

void getTimeDigits(size_t timeInMs, unsigned int& minutes, unsigned int& seconds, unsigned int& mmseconds)
{
	unsigned int secondsTotal = (unsigned int)(timeInMs / 1000);
	minutes = secondsTotal / 60;
	seconds = secondsTotal % 60;
	mmseconds = timeInMs % 1000;
}

std::string getTimeSpentString(size_t timeInMs)
{
	unsigned int minutes = 0;
	unsigned int seconds = 0;
	unsigned int mmseconds = 0;

	getTimeDigits(timeInMs, minutes, seconds, mmseconds);

	std::string str = string_format("%dm %ds %dms", minutes, seconds, mmseconds);
	
	return str;
}

std::string getFormattedTime(size_t timeInMs)
{
	unsigned int minutes = 0;
	unsigned int seconds = 0;
	unsigned int mmseconds = 0;

	getTimeDigits(timeInMs, minutes, seconds, mmseconds);

	return string_format("%02d:%02d.%03d", minutes, seconds, mmseconds);
}

void FireRenderProduction::SetupWorkProgressCallback()
{
	m_contextPtr->SetWorkProgressCallback([this](const ContextWorkProgressData& progressData)
		{
			if (!m_globals.useDetailedContextWorkLog &&
				progressData.progressType != ProgressType::RenderComplete &&
				progressData.progressType != ProgressType::SyncComplete &&
				progressData.progressType != ProgressType::ObjectSyncComplete)
			{
				return;
			}

			std::string strTime = getFormattedTime(progressData.currentTimeInMiliseconds);

			std::string strOutput;

			switch (progressData.progressType)
			{
			case ProgressType::ObjectPreSync:
			{
				strOutput = string_format("Syncing object: %d/%d", progressData.currentIndex, progressData.totalCount);
				break;
			}
			case ProgressType::ObjectSyncComplete:
				m_progressBars->update(progressData.GetPercentProgress(), true);
				break;
			case ProgressType::SyncComplete:
				strOutput = string_format("RPR scene synchronization time: %s", getTimeSpentString(progressData.elapsedTotal).c_str());
				break;
			case ProgressType::RenderPassStarted:
				strOutput = string_format("Render Pass: %d/%d", progressData.currentIndex, progressData.totalCount);
				break;
			case ProgressType::RenderComplete:
				DisplayRenderTimeData(strTime);
				strOutput = string_format("RPR denoising and tonemapping time: %s", getTimeSpentString(progressData.elapsedPostRenderTonemap).c_str());
				strOutput = strTime + ": " + strOutput;
				MGlobal::displayInfo(MString(strOutput.c_str()));

				strOutput = string_format("RPR total production render command time: %s", getTimeSpentString(progressData.elapsedTotal).c_str());
				break;
			}

			if (!strOutput.empty())
			{
				strOutput = strTime + ": " + strOutput;

				MGlobal::displayInfo(MString(strOutput.c_str()));
			}
		});
}

void FireRenderProduction::UpdateGlobals(void)
{
	m_globals.readFromCurrentScene();
}

bool FireRenderProduction::isTileRender() const
{
	return m_globals.tileRenderingEnabled;
}

bool FireRenderProduction::Init(int contextWidth, int contextHeight, RenderRegion& region)
{

	if (GlobalRenderUtilsDataHolder::GetGlobalRenderUtilsDataHolder()->IsSavingIntermediateEnabled())
	{
		GlobalRenderUtilsDataHolder::GetGlobalRenderUtilsDataHolder()->UpdateStartTime();
		std::remove(std::string(GlobalRenderUtilsDataHolder::GetGlobalRenderUtilsDataHolder()->FolderPath() + "time_log.txt").c_str());
	}

	// Read common render settings.
	MRenderUtil::getCommonRenderSettings(m_settings);

	MString renderStamp;
	if (m_globals.useRenderStamp)
		renderStamp = m_globals.renderStampText;

	m_aovs = &m_globals.aovs;

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
	if (!m_contextPtr->buildScene(false, false, false))
	{
		return false;
	}

	if (!m_globals.tileRenderingEnabled)
	{
		m_NorthStarRenderingHelper.SetData(m_contextPtr.get(), std::bind(&FireRenderProduction::OnBufferAvailableCallback, this, std::placeholders::_1));
	}
	m_aovs->setFromContext(*m_contextPtr);

	m_needsContextRefresh = true;
	m_contextPtr->setResolution(contextWidth, contextHeight, true);
	m_contextPtr->setCamera(m_camera, true);

	m_contextPtr->setUseRegion(m_isRegion);

	bool showWarningDialog = false;
	if (m_contextPtr->isFirstIterationAndShadersNOTCached())
		showWarningDialog = true;	//first iteration and shaders are _NOT_ cached

	if (m_isRegion)
		m_contextPtr->setRenderRegion(m_region);

	// Initialize the render progress bar UI.
	m_progressBars = make_unique<RenderProgressBars>(m_contextPtr->isUnlimited());
	m_progressBars->SetPreparingSceneText(true);

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

	SetupWorkProgressCallback();

	refreshContext();

	m_needsContextRefresh = false;

	m_progressBars->SetRenderingText(true);

	m_contextPtr->setStartedRendering();

	return true;
}

bool FireRenderProduction::startTileRender()
{
	assert(m_globals.tileRenderingEnabled);

	int contextWidth = m_globals.tileSizeX;
	int contextHeight = m_globals.tileSizeY;

	RenderRegion region = RenderRegion(contextWidth, contextHeight);

	Init(contextWidth, contextHeight, region);
	FireRenderThread::KeepRunning([this]()
	{
		try
		{
			RenderTiles();
			stop();
			m_isRunning = false;
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

	return true;
}
// -----------------------------------------------------------------------------
bool FireRenderProduction::startFullFrameRender()
{
	assert(!m_globals.tileRenderingEnabled);

	int contextWidth = m_width;
	int contextHeight = m_height;

	RenderRegion region = m_region;

	Init(contextWidth, contextHeight, region);

	// Start the render
	FireRenderThread::KeepRunning([this]()
	{
		try
		{
			m_isRunning = RunOnViewportThread();
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

	m_NorthStarRenderingHelper.Start();

	return true;
}

void FireRenderProduction::OnBufferAvailableCallback(float progress)
{
	AutoMutexLock pixelsLock(m_pixelsLock);

	bool frameFinished = fabs(1.0f - progress) <= FLT_EPSILON;
	bool shouldUpdateRenderView = !m_contextPtr->IsDenoiserCreated() || (m_contextPtr->IsDenoiserCreated() && frameFinished);

	m_renderViewAOV->readFrameBuffer(*m_contextPtr);
	
	if (!shouldUpdateRenderView)
	{
		return;
	}

	FireRenderThread::RunProcOnMainThread([this]()
		{
			// Update the Maya render view.
			m_renderViewAOV->sendToRenderView();

			if (rcWarningDialog.shown)
				rcWarningDialog.close();
		});
}

// -----------------------------------------------------------------------------
bool FireRenderProduction::pause(bool value)
{
	m_isPaused = value;

	if (m_isPaused)
	{
		m_contextPtr->SetState(FireRenderContext::StatePaused);
	}
	else
	{
		m_contextPtr->SetState(FireRenderContext::StateRendering);

		m_needsContextRefresh = true;
	}

	return true;
}

// -----------------------------------------------------------------------------
bool FireRenderProduction::stop()
{
	if (!m_isRunning)
		return true;

	m_NorthStarRenderingHelper.SetStopFlag();

	if (m_contextPtr)
	{
		m_contextPtr->SetState(FireRenderContext::StateExiting);
	}

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

	if (FireRenderThread::AreWeOnMainThread())
	{
		while (!m_NorthStarRenderingHelper.IsStopped())
		{
			FireRenderThread::RunItemsQueuedForTheMainThread();
		}
	}

	m_NorthStarRenderingHelper.StopAndJoin();

	if (m_contextPtr)
	{
		if (FireRenderThread::AreWeOnMainThread())
		{
			// Try-lock context lock. If can't lock it then RPR thread is rendering - run item queue
			while (!m_contextLock.try_lock())
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

	return true;
}

// -----------------------------------------------------------------------------
template <class T>
void WriteAthenaFieldAsString(const std::string& fieldName, const T& data)
{
	std::ostringstream ss;
	ss << data;
	AthenaWrapper::GetAthenaWrapper()->WriteField(fieldName, ss.str());
}

template <class T>
void WriteAthenaField(const std::string& fieldName, const T& data)
{
	AthenaWrapper::GetAthenaWrapper()->WriteField(fieldName, data);
}

#if defined(OSMac_)
int getNumCPUCores()
{
	int numCPU;
	std::size_t len = sizeof(numCPU);

	/* set the mib for hw.ncpu */
	int mib[] = { CTL_HW, HW_AVAILCPU }; // alternatively, try HW_NCPU;

	/* get the number of CPUs from the system */
	sysctl(mib, sizeof(mib) / sizeof(int), &numCPU, &len, NULL, 0);

	if (numCPU < 1)
	{
		mib[1] = HW_NCPU;
		sysctl(mib, sizeof(mib) / sizeof(int), &numCPU, &len, NULL, 0);
		if (numCPU < 1)
			numCPU = 1;
	}

	return numCPU;
}

#elif defined(__linux__)
int getNumCPUCores()
{
	int numCPU = sysconf(_SC_NPROCESSORS_ONLN);

	return numCPU;
}
#endif

void FireRenderProduction::UploadAthenaData()
{
	// We drop support for all Mayas older then 2022 since they have old Python 2.7
#if MAYA_API_VERSION < 20220000
	return;
#endif

	AthenaWrapper::GetAthenaWrapper()->StartNewFile();

	// operating system
#if defined(_WIN32)
	std::string osName;
	std::string osVersion;
	getOSName(osName, osVersion);
	WriteAthenaField("OS Name", osName);
	WriteAthenaField("OS Version", osVersion);

	// - Get the timezone info.
	std::string timezoneName;
	getTimeZone(timezoneName);
	WriteAthenaField("OS TZ", timezoneName);

#elif defined(__APPLE__)
    char buffer[1024];

    getTimeZone(buffer, sizeof(buffer)/sizeof(buffer[0]));
    WriteAthenaField("OS TZ", buffer);

    getOSName(buffer, sizeof(buffer)/sizeof(buffer[0]));
    WriteAthenaField("OS Name", buffer);

    getOSVersion(buffer, sizeof(buffer)/sizeof(buffer[0]));
    WriteAthenaField("OS Version", buffer);
#elif defined(__linux__)
	WriteAthenaField("OS Name", "Linux");
#endif

	// os arch
	WriteAthenaField("OS Arch", "64bit");

	// plug-in version
	WriteAthenaFieldAsString("ProRender Plugin Version", PLUGIN_VERSION);

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

	WriteAthenaField("ProRender Core Version", oss.str());

	// host application
	WriteAthenaField("Host App", "Maya");

	// render time
	WriteAthenaField("Seconds spent on render", m_contextPtr->m_secondsSpentOnLastRender);

	// device used
	// - CPU Name
	std::string CPUName = RenderStampUtils::GetCPUNameString();
	WriteAthenaField("CPU Name", CPUName);

	// - CPU Cores
	int numCPU = getNumCPUCores();
	WriteAthenaField("CPU Cores", numCPU);

	// - GPU0 Name
	std::vector<HardwareResources::Device> allDevices = HardwareResources::GetAllDevices();
	if (allDevices.size() > 0)
	{
		std::string GPU0Name = allDevices[0].name;
		WriteAthenaField("GPU0 Name", GPU0Name);
	}

	// - GPU1 Name
	std::vector<std::string> GPU1Name; // list GPU 0-15
	GPU1Name.reserve(allDevices.size());
	for (HardwareResources::Device& device : allDevices)
		GPU1Name.push_back(device.name);
	WriteAthenaField("GPU1 Name", GPU1Name);

	// - device used
	int renderDevice = RenderStampUtils::GetRenderDevice();
	switch (renderDevice)
	{
		case RenderStampUtils::RPR_RENDERDEVICE_CPUONLY:
		{
			WriteAthenaField("CPU Enabled", true);
			WriteAthenaField("GPU0 Enabled", false);
			break;
		}

		case RenderStampUtils::RPR_RENDERDEVICE_GPUONLY:
		{
			WriteAthenaField("CPU Enabled", false);
			WriteAthenaField("GPU0 Enabled", true);
			break;
		}

		default: // CPU+GPU
			WriteAthenaField("CPU Enabled", true);
			WriteAthenaField("GPU0 Enabled", true);
	}

	std::vector<bool> gpusUsed;
	MIntArray devicesUsing;
	MGlobal::executeCommand("optionVar -q RPR_DevicesSelected", devicesUsing);
	size_t numDevices = std::min<size_t>(devicesUsing.length(), allDevices.size());

	for (size_t idx = 0; idx < numDevices; ++idx)
		gpusUsed.push_back(devicesUsing[(unsigned int)idx] != 0);

	WriteAthenaField("GPU1 Enabled", gpusUsed);

	// polygon count
	WriteAthenaField("Num Polygons", GetScenePolyCount());

	// render resolution
	std::vector<unsigned int> imgRes = { m_width, m_height };
	WriteAthenaField("Resolution", imgRes);

	// render result
	switch (m_contextPtr->m_lastRenderResultState)
	{
		case FireRenderContext::COMPLETED:
		{
			WriteAthenaField("End status", "successfull | completed");
			break;
		}

		case FireRenderContext::CANCELED:
		{
			WriteAthenaField("End status", "successfull | cancelled");
			break;
		}

		case FireRenderContext::CRASHED:
		{
			WriteAthenaField("End status", "failed | crashed");
			break;
		}

		default:
			WriteAthenaField("End status", "failed | error!");
	}

	// locale
	const char* currLocale = std::setlocale(LC_NUMERIC, "");
	WriteAthenaFieldAsString("OS Locale", currLocale);

	// lights
	WriteAthenaField("Lights Count", m_contextPtr->GetScene().LightObjectCount());

	// time and date
	auto curr = std::chrono::system_clock::now();
	std::time_t currTime = std::chrono::system_clock::to_time_t(curr);
	WriteAthenaField("Stop Time", std::string(std::ctime(&currTime)) );

	std::time_t renderStartTime = std::chrono::system_clock::to_time_t(m_contextPtr->m_lastRenderStartTime);
	WriteAthenaField("Start Time", std::string(std::ctime(&renderStartTime)) );

	// aov's
	static std::map<unsigned int, std::string> aovNames =
	{
		 {RPR_AOV_COLOR, "RPR_AOV_COLOR"}
		,{RPR_AOV_OPACITY, "RPR_AOV_OPACITY" }
		,{RPR_AOV_WORLD_COORDINATE, "RPR_AOV_WORLD_COORDINATE" }
		,{RPR_AOV_UV, "RPR_AOV_UV" }
		,{RPR_AOV_MATERIAL_ID, "RPR_AOV_MATERIAL_IDX" }
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

	std::vector<std::string> aovsUsed;
	aovsUsed.reserve(aovNames.size());

	for (int aovID = 0; aovID < RPR_AOV_MAX; aovID++)
		if (m_contextPtr->isAOVEnabled(aovID))
			aovsUsed.push_back(aovNames[aovID]);

	WriteAthenaField("AOVs Enabled", aovsUsed);

	// completed iterations
	WriteAthenaField("Samples", m_contextPtr->m_currentIteration);

	// textures
	size_t countTextures = 0;
	long long texturesSize = 0;
	std::tie(countTextures, texturesSize) = GeSceneTexturesCountAndSize();
	WriteAthenaField("Num Textures", countTextures);
	WriteAthenaField("Textures Size", texturesSize/1000);

	// ray depth
	WriteAthenaField("Ray Depth", m_globals.maxRayDepth);
	WriteAthenaField("Diffuse Ray Depth", m_globals.maxRayDepthDiffuse);
	WriteAthenaField("Reflection Ray Depth", m_globals.maxRayDepthGlossy);
	WriteAthenaField("Refraction Ray Depth", m_globals.maxRayDepthRefraction);
	WriteAthenaField("Shadow Ray Depth", m_globals.maxRayDepthShadow);

	// maya version
	MString mayaVer = MGlobal::mayaVersion();
	WriteAthenaFieldAsString("App Version", mayaVer.asChar());

	// denoiser
	static std::map<FireRenderGlobals::DenoiserType, std::string> denoiserName =
	{
		{FireRenderGlobals::kBilateral, "Bilateral"} ,
		{FireRenderGlobals::kLWR, "LWR"} ,
		{FireRenderGlobals::kEAW, "EAW"} ,
		{FireRenderGlobals::kML, "ML"} ,
	};

	bool isDenoiserEnabled = m_globals.denoiserSettings.enabled;
	WriteAthenaField("RIF Type", isDenoiserEnabled ? denoiserName[m_globals.denoiserSettings.type] : "Not Enabled");

	RenderType renderType = m_contextPtr->GetRenderType();
	RenderQuality quality = GetRenderQualityForRenderType(renderType);
	static std::map<RenderQuality, std::string> renderQualityName =
	{
		{RenderQuality::RenderQualityFull, "Full"},
		{RenderQuality::RenderQualityHigh, "High"},
		{RenderQuality::RenderQualityMedium, "Medium"},
		{RenderQuality::RenderQualityLow, "Low"},
		{RenderQuality::RenderQualityNorthStar, "forced NorthStar"},
		{RenderQuality::RenderQualityHybridPro, "forced HybridPro"},
	};
	WriteAthenaField("Quality", renderQualityName[quality]);

	AthenaWrapper::GetAthenaWrapper()->AthenaSendFile(pythonCallWrap);
}

void DisplayTimeMessage(float timeVal, std::string strMsg)
{
	unsigned int ms = lround(timeVal);
	unsigned int min = ms / 60000;
	ms = ms - 60000 * min;
	unsigned int sec = ms / 1000;
	ms = ms - 1000 * sec;

	std::string strOutput;
	strOutput = strMsg + string_format("%dmin, %dsec, %dms", min, sec, ms);
	MGlobal::displayInfo(MString(strOutput.c_str()));
}

void FireRenderProduction::DisplayRenderTimeData(const std::string& strTime) const
{
	DisplayTimeMessage(m_contextPtr->m_syncTime, strTime + ": RPR Core sync time: ");

	DisplayTimeMessage(m_contextPtr->m_firstFrameRenderTime, strTime + ": First frame render time: ");

	DisplayTimeMessage(m_contextPtr->m_lastRenderedFrameRenderTime, strTime + ": Last frame render time: ");

	DisplayTimeMessage(m_contextPtr->m_totalRenderTime, strTime + ": Total render time: ");
}

std::tuple<size_t, long long> FireRenderProduction::GeSceneTexturesCountAndSize() const
{
	size_t data_size = 0;
	rprContextGetInfo(m_contextPtr->context(), RPR_CONTEXT_LIST_CREATED_IMAGES, 0, nullptr, &data_size);
	std::vector<rpr_image> images(data_size / sizeof(rpr_image));
	rprContextGetInfo(m_contextPtr->context(), RPR_CONTEXT_LIST_CREATED_IMAGES, data_size, images.data(), &data_size);

	long long texturesSize = 0;

	for (auto image : images) 
	{
		size_t data_size = 0;
		rprImageGetInfo(image, RPR_IMAGE_DATA_SIZEBYTE, 0, nullptr, &data_size);

		long long temp;
		rprImageGetInfo(image, RPR_IMAGE_DATA_SIZEBYTE, data_size, &temp, nullptr);

		texturesSize += temp;
	}

	return std::make_tuple(images.size(), texturesSize);
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

	switch (m_contextPtr->GetState())
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

				UploadAthenaData();
				
				m_contextPtr->m_polycountLastRender = 0;

				m_contextPtr->ResetRAMBuffers();

				m_contextPtr->m_tonemapStartTime = GetCurrentChronoTime();
				TonemapFromAOVs();
				DenoiseFromAOVs();

				stop();
				m_rendersCount++;

				return false;
			}

			try
			{	// Render.
				AutoMutexLock contextLock(m_contextLock);
				if (m_contextPtr->GetState() != FireRenderContext::StateRendering) 
					return false;

				RenderFullFrame();
			}
			catch (...)
			{
				m_contextPtr->m_lastRenderResultState = FireRenderContext::CRASHED;

				UploadAthenaData();

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

void FireRenderProduction::TonemapFromAOVs()
{
	if (!m_contextPtr->IsTonemappingEnabled())
		return;

	FireRenderAOV* pColorAOV = m_aovs->getAOV(RPR_AOV_COLOR);
	assert(pColorAOV != nullptr);

	m_contextPtr->ProcessTonemap(*m_renderViewAOV, *pColorAOV, m_region.getWidth(), m_region.getHeight(), RenderRegion(0, m_region.right - m_region.left, m_region.top - m_region.bottom, 0), [this](RV_PIXEL* data)
	{
		// Update the Maya render view.
		FireRenderThread::RunProcOnMainThread([this, data]()
		{
			RenderViewUpdater::UpdateAndRefreshRegion(data, m_width, m_height, m_region);
		});
	});
}

void FireRenderProduction::DenoiseFromAOVs()
{
	if (!m_contextPtr->IsDenoiserEnabled())
		return;

	FireRenderAOV* pColorAOV = m_aovs->getAOV(RPR_AOV_COLOR);
	assert(pColorAOV != nullptr);

	m_contextPtr->ProcessDenoise(*m_renderViewAOV, *pColorAOV, m_region.getWidth(), m_region.getHeight(), RenderRegion(0, m_region.right - m_region.left, m_region.top - m_region.bottom, 0), [this](RV_PIXEL* data)
	{
		// Update the Maya render view.
		FireRenderThread::RunProcOnMainThread([this, data]()
		{
			RenderViewUpdater::UpdateAndRefreshRegion(data, m_width, m_height, m_region);
		});
	});
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

	AOVPixelBuffers& outBuffers = m_contextPtr->PixelBuffers();
	outBuffers.clear();
	m_aovs->ForEachActiveAOV([&](FireRenderAOV& aov) 
	{
		auto ret = outBuffers.insert(std::pair<unsigned int, PixelBuffer>(aov.id, PixelBuffer()));
		ret.first->second.resize(m_width, m_height);
	});

	m_contextPtr->setSamplesPerUpdate(m_globals.completionCriteriaFinalRender.completionCriteriaMaxIterations);

	// we need to resetup camera because total width and height differs with tileSizeX and tileSizeY
	m_contextPtr->camera().TranslateCameraExplicit(info.totalWidth, info.totalHeight);

	tileRenderer.Render(*m_contextPtr, info, outBuffers, [&](RenderRegion& region, int progress, AOVPixelBuffers& out)
	{
		// make proper size
		unsigned int width = region.getWidth();
		unsigned int height = region.getHeight();

		m_contextPtr->resize(width, height, true);

		m_aovs->setRegion(RenderRegion(width, height), region.getWidth(), region.getHeight());
		m_aovs->allocatePixels();

		m_contextPtr->render(false);


		// copy data to buffer
		m_aovs->ForEachActiveAOV([&](FireRenderAOV& aov)
		{
			aov.readFrameBuffer(*m_contextPtr);

			auto it = out.find(aov.id);

			if (it == out.end())
				return;

			it->second.overwrite(aov.pixels.get(), region, info.totalHeight, info.totalWidth, aov.id);
		});

		// send data to Maya render view
		FireRenderThread::RunProcOnMainThread([this, region]()
		{
			// Update the Maya render view.
			RenderViewUpdater::UpdateAndRefreshRegion(m_renderViewAOV->pixels.get(), region.getWidth(), region.getHeight(), region);

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

#ifdef _DEBUG
#ifdef DUMP_TILES_AOVS_ALL
	// debug dump resulting AOVs
	for (auto& tmp : outBuffers)
		tmp.second.debugDump(m_height, m_width, std::string(FireRenderAOV::GetAOVName(tmp.first)));
#endif
#endif

	// setup denoiser if necessary
	m_contextPtr->TryCreateDenoiserImageFilters(true);

	RV_PIXEL* data = nullptr;
	std::vector<float> vecData;

	if (m_contextPtr->IsDenoiserCreated() && (m_renderViewAOV->id == RPR_AOV_COLOR))
	{
		// run denoiser on cached data if necessary
		bool denoiseResult = false;
		vecData = m_contextPtr->GetDenoisedData(denoiseResult);
		data = (RV_PIXEL*)vecData.data();
	}
	else
	{
		// else upload data from buffers directly
		auto it = outBuffers.find(m_renderViewAOV->id);
		assert(it != outBuffers.end());
		data = it->second.get();
	}

	// run merge opacity
	m_contextPtr->ProcessMergeOpactityFromRAM(data, info.totalWidth, info.totalHeight);

	// apply render stamp
	FireMaya::RenderStamp renderStamp;
	MString stampStr(m_renderViewAOV->renderStamp);
	renderStamp.AddRenderStamp(*m_contextPtr, data, m_width, m_height, stampStr.asChar());

	// update the Maya render view
	FireRenderThread::RunProcOnMainThread([this, data]()
	{
		// Update the Maya render view.
		RenderViewUpdater::UpdateAndRefreshRegion(data, m_width, m_height, RenderRegion(0, m_width - 1, m_height - 1, 0));
	});

	outBuffers.clear();

	UploadAthenaData();
}

void FireRenderProduction::RenderFullFrame()
{
	m_contextPtr->render(false);

	m_contextPtr->updateProgress();

	// Read pixel data for the AOV displayed in the render view.
	{
		AutoMutexLock pixelsLock(m_pixelsLock);
		m_aovs->readFrameBuffers(*m_contextPtr);

		FireRenderThread::RunProcOnMainThread([this]()
		{
			// Update the Maya render view.
			m_renderViewAOV->sendToRenderView();

			if (rcWarningDialog.shown)
				rcWarningDialog.close();
		});
	}

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
			m_renderViewAOV->writeToFile(*m_contextPtr, filePath, colorOnly, imageFormat);

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
				m_contextPtr->AbortRender();

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
}
