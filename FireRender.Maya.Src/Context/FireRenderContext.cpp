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
#include <GL/glew.h>
#include <maya/M3dView.h>
#include "FireRenderContext.h"
#include <cassert>
#include <float.h>
#include <maya/MDagPathArray.h>
#include <maya/MMatrix.h>
#include <maya/MFnNurbsSurface.h>
#include <maya/MFnMeshData.h>
#include <maya/MFnMesh.h>
#include <maya/MFnLight.h>
#include <maya/MSelectionList.h>
#include <maya/MImage.h>
#include <maya/MItDag.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnDagNode.h>
#include <maya/MPlug.h>
#include <maya/MItDependencyNodes.h>
#include <maya/MUserEventMessage.h>
#include <maya/MItDependencyGraph.h>
#include <maya/MAnimControl.h>

#include "AutoLock.h"
#include "VRay.h"
#include "ImageFilter/ImageFilter.h"

#include "RprComposite.h"
#include <iostream>
#include <fstream>

#include "FireRenderThread.h"
#include "FireRenderMaterialSwatchRender.h"
#include "CompositeWrapper.h"
#include <InstancerMASH.h>

#include <deque>

#ifdef WIN32 // alembic support is disabled on MAC until alembic build issue on MAC is resolved
#include "FireRenderGPUCache.h"
#endif

#ifdef OPTIMIZATION_CLOCK
	#include <chrono>
#endif

using namespace RPR;
using namespace FireMaya;

#ifdef DEBUG_LOCKS
std::map<FireRenderContext*,FireRenderContext::Lock::frcinfo> FireRenderContext::Lock::lockMap;
#endif

#include <imageio.h>

#include <maya/MUuid.h>
#include "FireRenderIBL.h"
#include <maya/MFnRenderLayer.h>

#ifdef OPTIMIZATION_CLOCK
	int FireRenderContext::timeInInnerAddPolygon;
	int FireRenderContext::overallAddPolygon;
	int FireRenderContext::overallCreateMeshEx;
	unsigned long long FireRenderContext::timeGetDataFromMaya;
	unsigned long long FireRenderContext::translateData;
	int FireRenderContext::inTranslateMesh;
	int FireRenderContext::inGetFaceMaterials;
	int FireRenderContext::getTessellatedObj;
	int FireRenderContext::deleteNodes;
#endif

int FireRenderContext::INCORRECT_PLUGIN_ID = -1;

FireRenderContext::FireRenderContext() :
	m_width(0),
	m_height(0),
	m_useRegion(false),
	m_motionBlur(false),
	m_cameraMotionBlur(false),
	m_viewportMotionBlur(false),
	m_motionBlurCameraExposure(0.0f),
	m_motionSamples(0),
	m_cameraAttributeChanged(false),
	m_samplesPerUpdate(1),
	m_secondsSpentOnLastRender(0.0),
	m_lastRenderResultState(NOT_SET),
	m_polycountLastRender(0),
	m_currentIteration(0),
	m_currentFrame(0),
	m_progress(0),
	m_interactive(false),
	m_camera(this, MDagPath()),
	m_globalsChanged(false),
	m_renderLayersChanged(false),
	m_cameraDirty(true),
	m_denoiserChanged(false),
	m_denoiserFilter(nullptr),
	m_upscalerFilter(nullptr),
	m_shadowColor{ 0.0f, 0.0f, 0.0f },
	m_shadowTransparency(0),
	m_shadowWeight(1),
	m_RenderType(RenderType::Undefined),
	m_bIsGLTFExport(false),
	m_IterationsPowerOf2Mode(false),
	m_DisableSetDirtyObjects(false),
	m_lastRenderedFrameRenderTime(0.0f),
	m_firstFrameRenderTime(0.0f),
	m_syncTime(0.0f),
	m_totalRenderTime(0.0f),
	m_tonemapStartTime{}
{
	DebugPrint("FireRenderContext::FireRenderContext()");

	m_workStartTime = GetCurrentChronoTime();
	m_state = StateUpdating;
	m_dirty = false;
}

FireRenderContext::~FireRenderContext()
{
	DebugPrint("FireRenderContext(context=%p)::~FireRenderContext()", this);
	// Unsubscribe from all callbacks.
	removeCallbacks();

	m_denoiserFilter.reset();
	m_upscalerFilter.reset();
}

int FireRenderContext::initializeContext()
{
	DebugPrint("FireRenderContext::initializeContext()");

	RPR_THREAD_ONLY;

	LOCKMUTEX(this);

	auto createFlags = FireMaya::Options::GetContextDeviceFlags(m_RenderType);

	rpr_int res;
	createContextEtc(createFlags, true, &res);

	return res;
}

void FireRenderContext::resize(unsigned int w, unsigned int h, bool renderView, rpr_GLuint* glTexture)
{
	RPR_THREAD_ONLY;

	// Set the context resolution.
	setResolution(w, h, renderView, glTexture);
}

int FireRenderContext::GetAOVMaxValue() const
{
	return 0x20;
}

void FireRenderContext::setResolution(unsigned int w, unsigned int h, bool renderView, rpr_GLuint* glTexture)
{
	RPR_THREAD_ONLY;
	DebugPrint("FireRenderContext::setResolution(%d,%d)", w, h);
	m_width = w;
	m_height = h;
	m_isRenderView = renderView;

	auto context = scope.Context();

	if (!context)
		return;

	int MaxAOV = GetAOVMaxValue();
	for (int i = 0; i < MaxAOV; ++i)
	{
		initBuffersForAOV(context, i, glTexture);
	}
}

MString GetModelPath(); // forward declaration

bool FireRenderContext::TryCreateTonemapImageFilters()
{
	if (!IsTonemappingEnabled())
		return false;

	if (m_pixelBuffers.size() == 0)
		return false;

	std::uint32_t width = m_useRegion ? (uint32_t)m_region.getWidth() : (uint32_t)m_pixelBuffers[RPR_AOV_COLOR].width();
	std::uint32_t height = m_useRegion ? (uint32_t)m_region.getHeight() : (uint32_t)m_pixelBuffers[RPR_AOV_COLOR].height();
	size_t rifImageSize = sizeof(RV_PIXEL) * width * height;

	try
	{
		MString mlModelsFolder = GetModelPath();

		m_tonemap = std::shared_ptr<ImageFilter>(new ImageFilter(
			context(),
			width,
			height,
			mlModelsFolder.asChar(),
			true
		));

		void* pBuffer = PixelBuffers()[RPR_AOV_COLOR].data();

		switch (m_globals.toneMappingType)
		{
		case 1:
		{
			m_tonemap->CreateFilter(RifFilterType::LinearTonemap);
			m_tonemap->AddInput(RifColor, PixelBuffers()[RPR_AOV_COLOR].data(), rifImageSize, 0.0f);
			RifParam p = { RifParamType::RifFloat, (rif_float)m_globals.toneMappingLinearScale };
			m_tonemap->AddParam("key", p);
			break;
		}
		case 2:
		{
			m_tonemap->CreateFilter(RifFilterType::PhotoLinearTonemap);
			m_tonemap->AddInput(RifColor, PixelBuffers()[RPR_AOV_COLOR].data(), rifImageSize, 0.0f);
			RifParam p = { RifParamType::RifFloat, (rif_float)m_globals.toneMappingPhotolinearSensitivity };
			m_tonemap->AddParam("sensitivity", p);
			p = { RifParamType::RifFloat, (rif_float)m_globals.toneMappingPhotolinearFstop };
			m_tonemap->AddParam("fstop", p);
			p = { RifParamType::RifFloat, (rif_float)m_globals.toneMappingPhotolinearExposure };
			m_tonemap->AddParam("exposureTime", p);
			p = { RifParamType::RifFloat, (rif_float)2.2f };
			m_tonemap->AddParam("gamma", p);
			break;
		}
		case 3:
		{
			m_tonemap->CreateFilter(RifFilterType::AutoLinearTonemap);
			m_tonemap->AddInput(RifColor, PixelBuffers()[RPR_AOV_COLOR].data(), rifImageSize, 0.0f);
			RifParam p = { RifParamType::RifFloat, (rif_float)2.2f };
			m_tonemap->AddParam("gamma", p);
			break;
		}
		case 4:
		{
			m_tonemap->CreateFilter(RifFilterType::MaxWhiteTonemap);
			m_tonemap->AddInput(RifColor, PixelBuffers()[RPR_AOV_COLOR].data(), rifImageSize, 0.0f);
			break;
		}
		case 5:
		{
			m_tonemap->CreateFilter(RifFilterType::ReinhardTonemap);
			m_tonemap->AddInput(RifColor, PixelBuffers()[RPR_AOV_COLOR].data(), rifImageSize, 0.0f);
			RifParam p = { RifParamType::RifFloat, (rif_float)2.2f };
			m_tonemap->AddParam("gamma", p);
			p = { RifParamType::RifFloat, (rif_float)m_globals.toneMappingReinhard02Prescale };
			m_tonemap->AddParam("preScale", p);
			p = { RifParamType::RifFloat, (rif_float)m_globals.toneMappingReinhard02Postscale };
			m_tonemap->AddParam("postScale", p);
			p = { RifParamType::RifFloat, (rif_float)m_globals.toneMappingReinhard02Burn };
			m_tonemap->AddParam("burn", p);
			break;
		}
		case 6:
		{
			return false;
		}

		default:
			assert(false);
		}

		m_tonemap->AttachFilter();
		return true;
	}
	catch (std::exception& e)
	{
		m_denoiserFilter.reset();
		ErrorPrint(e.what());
		MGlobal::displayError("RPR failed to setup tonemapper, turning it off.");
		return false;
	}
}

bool FireRenderContext::TryCreateDenoiserImageFilters(bool useRAMBufer /* = false*/)
{
	bool shouldDenoise = IsDenoiserSupported() &&
						(m_globals.denoiserSettings.enabled && 
						((m_RenderType == RenderType::ProductionRender) || 
						(m_RenderType == RenderType::IPR)));

	if (!shouldDenoise)
		return false;

	if (!useRAMBufer)
	{
		setupDenoiserFB();
		return true;
	}

	if (useRAMBufer && (m_pixelBuffers.size() != 0))
	{
		setupDenoiserRAM();
		return true;
	}

	return false;
}

void FireRenderContext::resetAOV(int index, rpr_GLuint* glTexture)
{
	auto context = scope.Context();

	initBuffersForAOV(context, index, glTexture);
}

void FireRenderContext::enableAOVAndReset(int index, bool flag, rpr_GLuint* glTexture)
{
	enableAOV(index, flag);
	resetAOV(index, flag ? glTexture : nullptr);
}

bool aovExists(int index)
{
	if (index <= RPR_AOV_MATTE_PASS)
		return true;

	if ((index >= RPR_AOV_CRYPTOMATTE_MAT0) && (index <= RPR_AOV_CRYPTOMATTE_MAT5))
		return true;

	if ((index >= RPR_AOV_CRYPTOMATTE_OBJ0) && (index <= RPR_AOV_CRYPTOMATTE_OBJ5))
		return true;

	if (index == RPR_AOV_DEEP_COLOR)
		return true;

	return false;
}

void FireRenderContext::initBuffersForAOV(frw::Context& context, int index, rpr_GLuint* glTexture)
{
	rpr_framebuffer_format fmt = { 4, RPR_COMPONENT_TYPE_FLOAT32 };

	m.framebufferAOV[index].Reset();
	m.framebufferAOV_resolved[index].Reset();

	if (!IsAOVSupported(index))
	{
		return;
	}

	if (!aovExists(index))
	{
		return;
	}

	if (aovEnabled[index]) 
    {
		bool createResolveFB = true;
		if (index == RPR_AOV_DEEP_COLOR)
		{
			fmt.type = RPR_COMPONENT_TYPE_DEEP;

			createResolveFB = false;
		}
		
		m.framebufferAOV[index] = frw::FrameBuffer (context, m_width, m_height, fmt);
		m.framebufferAOV[index].Clear();
		context.SetAOV(m.framebufferAOV[index], index);

		if (createResolveFB)
		{
			m.framebufferAOV_resolved[index] = frw::FrameBuffer(context, m_width, m_height, fmt);
			m.framebufferAOV_resolved[index].Clear();
		}
	}
	else
	{
		if (aovExists(index))
		{
			context.SetAOV(nullptr, index);
		}
	}
}

void FireRenderContext::setCamera(MDagPath& cameraPath, bool useNonDefaultCameraType)
{
	MAIN_THREAD_ONLY;
	DebugPrint("FireRenderContext::setCamera(...)");
	m_camera.SetObject(cameraPath.node());

	if (useNonDefaultCameraType)
	{
		m_camera.setType(m_globals.cameraType);
	}
	else
		m_camera.setType(0);
}

void FireRenderContext::updateLimits(bool animation)
{
	// Update limits.
	updateLimitsFromGlobalData(m_globals, animation);
}

void FireRenderContext::updateLimitsFromGlobalData(const FireRenderGlobalsData & globalData, bool animation, bool batch)
{
	CompletionCriteriaParams params = isInteractive() ? globalData.completionCriteriaViewport : globalData.completionCriteriaFinalRender;

	// Set to a single iteration for animations.
	if (animation)
	{
		params.completionCriteriaMaxIterations = 1;
		params.makeInfiniteTime();
	}

	int iterationStep = 1;

	// Update completion criteria.
	setCompletionCriteria(params);
}

bool FireRenderContext::buildScene(bool isViewport, bool glViewport, bool freshen)
{
	MAIN_THREAD_ONLY;
	DebugPrint("FireRenderContext::buildScene()");

	m_globals.readFromCurrentScene();

	// Backdoor for enabling aovs in IPR/Viewport
	if (isInteractive())
	{
		EnableAOVsFromRSIfEnvVarSet(*this, m_globals.aovs);
	}

	if (IsDenoiserEnabled())
	{
		turnOnAOVsForDenoiser();
	}
	
	if (m_interactive && m_globals.contourIsEnabled)
	{
		turnOnAOVsForContour();
	}

	auto createFlags = FireMaya::Options::GetContextDeviceFlags(m_RenderType);

#ifdef DBG_LOG_RENDER_TYPE
	{
		std::ofstream loggingFile;
		loggingFile.open("C:\\temp\\dbg\\render_flags_log.txt", std::ofstream::out | std::ofstream::app);
		loggingFile << "m_RenderType = " << (int)m_RenderType << "\n";
		loggingFile << "createFlags = " << createFlags << "\n\n";
		loggingFile.close();
	}
#endif

	{
		LOCKMUTEX(this);

		FireRenderThread::UseTheThread((createFlags & RPR_CREATION_FLAGS_ENABLE_CPU) != 0);

		bool avoidMaterialSystemDeletion_workaround = isViewport && (createFlags & RPR_CREATION_FLAGS_ENABLE_CPU);

		rpr_int res;
		if (!createContextEtc(createFlags, !avoidMaterialSystemDeletion_workaround, &res, false))
		{
			MGlobal::displayError("Aborting switching to Radeon ProRender.");
			MString msg;
			FireRenderError error(res, msg, true);

			return false;
		}

		GetScope().CreateScene();
		updateLimitsFromGlobalData(m_globals);
		setupContextContourMode(m_globals, createFlags);
		setupContextHybridParams(m_globals); 
		setupContextPostSceneCreation(m_globals);

		setMotionBlurParameters(m_globals);
		setupContextAirVolume(m_globals);
		setupContextCryptomatteSettings(m_globals);

		// Update render selected objects only flag
		int isRenderSelectedOnly = 0;
		MGlobal::executeCommand("isRenderSelectedObjectsOnlyFlagSet()", isRenderSelectedOnly);
		m_renderSelectedObjectsOnly = isRenderSelectedOnly > 0;

		MStatus status;

		MItDag itDag(MItDag::kDepthFirst, MFn::kDagNode, &status);
		if (MStatus::kSuccess != status)
			MGlobal::displayError("MItDag::MItDag");

		for (; !itDag.isDone(); itDag.next())
		{
			MDagPath dagPath;
			status = itDag.getPath(dagPath);

			if (MStatus::kSuccess != status)
			{
				MGlobal::displayError("MDagPath::getPath");
				break;
			}

			AddSceneObject(dagPath);
		}

		//Should be called when all scene objects are constucted
		BuildLateinitObjects();

		attachCallbacks();
	}

	if(freshen)
		Freshen(true, [] { return false; });

	return true;
}

static const std::vector<int> g_denoiserAovs = { 
	RPR_AOV_SHADING_NORMAL, 
	RPR_AOV_WORLD_COORDINATE,
	RPR_AOV_OBJECT_ID, 
	RPR_AOV_DEPTH, 
	RPR_AOV_DIFFUSE_ALBEDO 
};

void FireRenderContext::turnOnAOVsForDenoiser(bool allocBuffer)
{
	// Turn on necessary AOVs
	forceTurnOnAOVs(g_denoiserAovs, allocBuffer);
}

static const std::vector<int> g_contourAovs = {
	RPR_AOV_OBJECT_ID, 
	RPR_AOV_SHADING_NORMAL,
	RPR_AOV_MATERIAL_ID,
	RPR_AOV_UV
};

void FireRenderContext::turnOnAOVsForContour(bool allocBuffer /*= false*/)
{
	// Turn on necessary AOVs
	forceTurnOnAOVs(g_contourAovs, allocBuffer);
}

void FireRenderContext::forceTurnOnAOVs(const std::vector<int>& aovsToAdd, bool allocBuffer /*= false*/)
{
	// Turn on necessary AOVs
	for (const int aov : aovsToAdd)
	{
		if (!isAOVEnabled(aov))
		{
			enableAOV(aov);

			if (allocBuffer)
			{
				auto ctx = scope.Context();
				initBuffersForAOV(ctx, aov);
			}
		}
	}
}

#if defined(LINUX)
bool FireRenderContext::CanCreateAiDenoiser() const
{
	return false;
}

#else

bool FireRenderContext::CanCreateAiDenoiser() const
{
	bool canCreateAiDenoiser = false;

	std::string gpuName;

	MIntArray devicesUsing;
	MGlobal::executeCommand("optionVar -q RPR_DevicesSelected", devicesUsing);

	auto allDevices = HardwareResources::GetAllDevices();
	size_t numDevices = std::min<size_t>(devicesUsing.length(), allDevices.size());

	for (int i = 0; i < numDevices; i++)
	{
		const HardwareResources::Device& gpuInfo = allDevices[i];
		
		if (devicesUsing[i])
		{
			gpuName = gpuInfo.name;
			canCreateAiDenoiser = true;
		}
	}

	return canCreateAiDenoiser;
}
#endif

MString GetModelPath()
{
	MString path;
	MStatus s = MGlobal::executeCommand("getModulePath -moduleName RadeonProRender", path);
	MString modelsFolder = path + "/data/models";

	return modelsFolder;
}

bool FireRenderContext::setupUpscalerForViewport(RV_PIXEL* data)
{
	std::uint32_t width = (std::uint32_t) m_pixelBuffers[RPR_AOV_COLOR].width();
	std::uint32_t height = (std::uint32_t) m_pixelBuffers[RPR_AOV_COLOR].height();

	size_t rifImageSize = sizeof(RV_PIXEL) * width * height;

	// setup upscaler
	m_upscalerFilter = std::shared_ptr<ImageFilter>(new ImageFilter(
		context(),
		2 * width,
		2 * height,
		GetModelPath().asChar()
	));

	m_upscalerFilter->CreateFilter(RifFilterType::Upscaler, /*parameter is ignored for upscaler creation*/ false);

	m_upscalerFilter->SetInputOverrideSize(width, height);
	m_upscalerFilter->AddInput(RifColor, (float*) data, rifImageSize, 0.0f);

	m_upscalerFilter->AttachFilter();

	return true;
}


bool FireRenderContext::setupDenoiserForViewport()
{
	bool canCreateAiDenoiser = CanCreateAiDenoiser();
	bool useOpenImageDenoise = !canCreateAiDenoiser;

	std::uint32_t width = (std::uint32_t) m_pixelBuffers[RPR_AOV_COLOR].width();
	std::uint32_t height = (std::uint32_t) m_pixelBuffers[RPR_AOV_COLOR].height();
	size_t rifImageSize = sizeof(RV_PIXEL) * width * height;

	try
	{
		MString mlModelsFolder = GetModelPath();

		m_denoiserFilter = std::shared_ptr<ImageFilter>(new ImageFilter(
			context(),
			width,
			height,
			mlModelsFolder.asChar()
		));

		RifFilterType ft = RifFilterType::MlDenoise;
		m_denoiserFilter->CreateFilter(ft, useOpenImageDenoise);
	
		m_denoiserFilter->AddInput(RifColor, m_pixelBuffers[RPR_AOV_COLOR].data(), rifImageSize, 0.0f);

		m_denoiserFilter->AddInput(RifNormal, m_pixelBuffers[RPR_AOV_SHADING_NORMAL].data(), rifImageSize, 0.0f);
		m_denoiserFilter->AddInput(RifDepth, m_pixelBuffers[RPR_AOV_DEPTH].data(), rifImageSize, 0.0f);
		m_denoiserFilter->AddInput(RifAlbedo, m_pixelBuffers[RPR_AOV_DIFFUSE_ALBEDO].data(), rifImageSize, 0.0f);

		RifParam p = { RifParamType::RifOther, true };
		m_denoiserFilter->AddParam("enable16bitCompute", p);

		m_denoiserFilter->AttachFilter();

	}
	catch (std::exception& e)
	{
		m_denoiserFilter.reset();
		ErrorPrint(e.what());
		MGlobal::displayError("RPR failed to setup denoiser, turning it off.");

		return false;
	}

	return true;
}


void FireRenderContext::setupDenoiserRAM()
{
	bool canCreateAiDenoiser = CanCreateAiDenoiser();
	bool useOpenImageDenoise = !canCreateAiDenoiser;

	std::uint32_t width = m_useRegion ? (uint32_t)m_region.getWidth() : (uint32_t)m_pixelBuffers[RPR_AOV_COLOR].width();
	std::uint32_t height = m_useRegion ? (uint32_t)m_region.getHeight() : (uint32_t)m_pixelBuffers[RPR_AOV_COLOR].height();
	size_t rifImageSize = sizeof(RV_PIXEL) * width * height;

	try
	{
		MString mlModelsFolder = GetModelPath();

		m_denoiserFilter = std::shared_ptr<ImageFilter>(new ImageFilter(
			context(), 
			width,
			height,
			mlModelsFolder.asChar()
		));

		RifParam p;

		switch (m_globals.denoiserSettings.type)
		{
		case FireRenderGlobals::kBilateral:
			m_denoiserFilter->CreateFilter(RifFilterType::BilateralDenoise);
			m_denoiserFilter->AddInput(RifColor, m_pixelBuffers[RPR_AOV_COLOR].data(), rifImageSize, 0.3f);
			m_denoiserFilter->AddInput(RifNormal, m_pixelBuffers[RPR_AOV_SHADING_NORMAL].data(), rifImageSize, 0.01f);
			m_denoiserFilter->AddInput(RifWorldCoordinate, m_pixelBuffers[RPR_AOV_WORLD_COORDINATE].data(), rifImageSize, 0.01f);

			p = { RifParamType::RifInt, m_globals.denoiserSettings.radius };
			m_denoiserFilter->AddParam("radius", p);
			break;

		case FireRenderGlobals::kLWR:
			m_denoiserFilter->CreateFilter(RifFilterType::LwrDenoise);
			m_denoiserFilter->AddInput(RifColor, m_pixelBuffers[RPR_AOV_COLOR].data(), rifImageSize, 0.1f);
			m_denoiserFilter->AddInput(RifNormal, m_pixelBuffers[RPR_AOV_SHADING_NORMAL].data(), rifImageSize, 0.1f);
			m_denoiserFilter->AddInput(RifDepth, m_pixelBuffers[RPR_AOV_DEPTH].data(), rifImageSize, 0.1f);
			m_denoiserFilter->AddInput(RifWorldCoordinate, m_pixelBuffers[RPR_AOV_WORLD_COORDINATE].data(), rifImageSize, 0.1f);
			m_denoiserFilter->AddInput(RifObjectId, m_pixelBuffers[RPR_AOV_OBJECT_ID].data(), rifImageSize, 0.1f);
			m_denoiserFilter->AddInput(RifTrans, m_pixelBuffers[RPR_AOV_OBJECT_ID].data(), rifImageSize, 0.1f);

			p = { RifParamType::RifInt, m_globals.denoiserSettings.samples };
			m_denoiserFilter->AddParam("samples", p);

			p = { RifParamType::RifInt,  m_globals.denoiserSettings.filterRadius };
			m_denoiserFilter->AddParam("halfWindow", p);

			p.mType = RifParamType::RifFloat;
			p.mData.f = m_globals.denoiserSettings.bandwidth;
			m_denoiserFilter->AddParam("bandwidth", p);
			break;

		case FireRenderGlobals::kEAW:
			m_denoiserFilter->CreateFilter(RifFilterType::EawDenoise);
			m_denoiserFilter->AddInput(RifColor, m_pixelBuffers[RPR_AOV_COLOR].data(), rifImageSize, m_globals.denoiserSettings.color);
			m_denoiserFilter->AddInput(RifNormal, m_pixelBuffers[RPR_AOV_SHADING_NORMAL].data(), rifImageSize, m_globals.denoiserSettings.normal);
			m_denoiserFilter->AddInput(RifDepth, m_pixelBuffers[RPR_AOV_DEPTH].data(), rifImageSize, m_globals.denoiserSettings.depth);
			m_denoiserFilter->AddInput(RifTrans, m_pixelBuffers[RPR_AOV_OBJECT_ID].data(), rifImageSize, m_globals.denoiserSettings.trans);
			m_denoiserFilter->AddInput(RifWorldCoordinate, m_pixelBuffers[RPR_AOV_WORLD_COORDINATE].data(), rifImageSize, 0.1f);
			m_denoiserFilter->AddInput(RifObjectId, m_pixelBuffers[RPR_AOV_OBJECT_ID].data(), rifImageSize, 0.1f);
			break;

		case FireRenderGlobals::kML:
		{
			RifFilterType ft = m_globals.denoiserSettings.colorOnly ? RifFilterType::MlDenoiseColorOnly : RifFilterType::MlDenoise;
			m_denoiserFilter->CreateFilter(ft, useOpenImageDenoise);
		}
		m_denoiserFilter->AddInput(RifColor, m_pixelBuffers[RPR_AOV_COLOR].data(), rifImageSize, 0.0f);

		if (!m_globals.denoiserSettings.colorOnly)
		{
			m_denoiserFilter->AddInput(RifNormal, m_pixelBuffers[RPR_AOV_SHADING_NORMAL].data(), rifImageSize, 0.0f);
			m_denoiserFilter->AddInput(RifDepth, m_pixelBuffers[RPR_AOV_DEPTH].data(), rifImageSize, 0.0f);
			m_denoiserFilter->AddInput(RifAlbedo, m_pixelBuffers[RPR_AOV_DIFFUSE_ALBEDO].data(), rifImageSize, 0.0f);
		}

		break;

		default:
			assert(false);
		}

		m_denoiserFilter->AttachFilter();
	}
	catch (std::exception& e)
	{
		m_denoiserFilter.reset();
		ErrorPrint(e.what());
		MGlobal::displayError("RPR failed to setup denoiser, turning it off.");
	}
}

void FireRenderContext::setupDenoiserFB()
{
	const rpr_framebuffer fbColor = m.framebufferAOV_resolved[RPR_AOV_COLOR].Handle();
	const rpr_framebuffer fbShadingNormal = m.framebufferAOV_resolved[RPR_AOV_SHADING_NORMAL].Handle();
	const rpr_framebuffer fbDepth = m.framebufferAOV_resolved[RPR_AOV_DEPTH].Handle();
	const rpr_framebuffer fbWorldCoord = m.framebufferAOV_resolved[RPR_AOV_WORLD_COORDINATE].Handle();
	const rpr_framebuffer fbObjectId = m.framebufferAOV_resolved[RPR_AOV_OBJECT_ID].Handle();
	const rpr_framebuffer fbDiffuseAlbedo = m.framebufferAOV_resolved[RPR_AOV_DIFFUSE_ALBEDO].Handle();
	const rpr_framebuffer fbTrans = fbObjectId;

	bool canCreateAiDenoiser = CanCreateAiDenoiser();
	bool useOpenImageDenoise = !canCreateAiDenoiser;

	try
	{
		MString mlModelsFolder = GetModelPath();

		m_denoiserFilter = std::shared_ptr<ImageFilter>(new ImageFilter(context(), m_width, m_height, mlModelsFolder.asChar()));

		RifParam p;

		switch (m_globals.denoiserSettings.type)
		{
		case FireRenderGlobals::kBilateral:
			m_denoiserFilter->CreateFilter(RifFilterType::BilateralDenoise);
			m_denoiserFilter->AddInput(RifColor, fbColor, 0.3f);
			m_denoiserFilter->AddInput(RifNormal, fbShadingNormal, 0.01f);
			m_denoiserFilter->AddInput(RifWorldCoordinate, fbWorldCoord, 0.01f);

			p = { RifParamType::RifInt, m_globals.denoiserSettings.radius };
			m_denoiserFilter->AddParam("radius", p);
			break;

		case FireRenderGlobals::kLWR:
			m_denoiserFilter->CreateFilter(RifFilterType::LwrDenoise);
			m_denoiserFilter->AddInput(RifColor, fbColor, 0.1f);
			m_denoiserFilter->AddInput(RifNormal, fbShadingNormal, 0.1f);
			m_denoiserFilter->AddInput(RifDepth, fbDepth, 0.1f);
			m_denoiserFilter->AddInput(RifWorldCoordinate, fbWorldCoord, 0.1f);
			m_denoiserFilter->AddInput(RifObjectId, fbObjectId, 0.1f);
			m_denoiserFilter->AddInput(RifTrans, fbTrans, 0.1f);

			p = { RifParamType::RifInt, m_globals.denoiserSettings.samples };
			m_denoiserFilter->AddParam("samples", p);

			p = { RifParamType::RifInt,  m_globals.denoiserSettings.filterRadius };
			m_denoiserFilter->AddParam("halfWindow", p);

			p.mType = RifParamType::RifFloat;
			p.mData.f = m_globals.denoiserSettings.bandwidth;
			m_denoiserFilter->AddParam("bandwidth", p);
			break;

		case FireRenderGlobals::kEAW:
			m_denoiserFilter->CreateFilter(RifFilterType::EawDenoise);
			m_denoiserFilter->AddInput(RifColor, fbColor, m_globals.denoiserSettings.color);
			m_denoiserFilter->AddInput(RifNormal, fbShadingNormal, m_globals.denoiserSettings.normal);
			m_denoiserFilter->AddInput(RifDepth, fbDepth, m_globals.denoiserSettings.depth);
			m_denoiserFilter->AddInput(RifTrans, fbTrans, m_globals.denoiserSettings.trans);
			m_denoiserFilter->AddInput(RifWorldCoordinate, fbWorldCoord, 0.1f);
			m_denoiserFilter->AddInput(RifObjectId, fbObjectId, 0.1f);
			break;

		case FireRenderGlobals::kML:
			{
				RifFilterType ft = m_globals.denoiserSettings.colorOnly ? RifFilterType::MlDenoiseColorOnly : RifFilterType::MlDenoise;
				m_denoiserFilter->CreateFilter(ft, useOpenImageDenoise);
			}
			m_denoiserFilter->AddInput(RifColor, fbColor, 0.0f);

			if (!m_globals.denoiserSettings.colorOnly)
			{
				m_denoiserFilter->AddInput(RifNormal, fbShadingNormal, 0.0f);
				m_denoiserFilter->AddInput(RifDepth, fbDepth, 0.0f);
				m_denoiserFilter->AddInput(RifAlbedo, fbDiffuseAlbedo, 0.0f);

				p = { RifParamType::RifOther, false };
				m_denoiserFilter->AddParam("enable16bitCompute", p);
			}
			else
			{
				p = { RifParamType::RifOther, m_globals.denoiserSettings.enable16bitCompute };
				m_denoiserFilter->AddParam("enable16bitCompute", p);
			}

			break;

		default:
			assert(false);
		}

		m_denoiserFilter->AttachFilter();
	}
	catch (std::exception& e)
	{
		m_denoiserFilter.reset();
		ErrorPrint( e.what() );
		MGlobal::displayError("RPR failed to setup denoiser, turning it off.");
	}
}

void FireRenderContext::UpdateDefaultLights()
{
	MAIN_THREAD_ONLY;
	bool enabled = true;
	for (auto ob : m_sceneObjects)
	{
		if (auto node = dynamic_cast<FireRenderNode*>(ob.second.get()))
		{
			if (node->IsEmissive())
			{
				enabled = false;
				break;
			}
		}
	}

	if (enabled)
	{
		MObject renGlobalsObj;
		GetDefaultRenderGlobals(renGlobalsObj);
		MFnDependencyNode globalsNode(renGlobalsObj);
		MPlug plug = globalsNode.findPlug("enableDefaultLight");
		plug.getValue(enabled);
	}

	if (!enabled)	// switch off if there is one
	{
		if (m_defaultLight)
		{
			GetScene().Detach(m_defaultLight);
			m_defaultLight.Reset();
			setCameraAttributeChanged(true);
		}
	}
	else if (!m_camera.Object().isNull())
	{
		MFnDagNode dagNode(m_camera.Object());
		MDagPath dagPath;
		dagNode.getPath(dagPath);
		if (dagPath.isValid())
		{
			if (!m_defaultLight)
			{
				m_defaultLight = GetContext().CreateDirectionalLight();
				GetScene().Attach(m_defaultLight);
			}

			// build direction light matrix
			MVector vZ(-0.2, 0.2, 0.5);
			vZ.normalize();
			MVector vY = vZ ^ MVector::xAxis;
			vY.normalize();
			MVector vX = vY ^ vZ;
			vX.normalize();

			MMatrix mLightLocal;
			mLightLocal.setToIdentity();

			vX.get(mLightLocal[0]);
			vY.get(mLightLocal[1]);
			vZ.get(mLightLocal[2]);

			MMatrix mLight = mLightLocal * dagPath.inclusiveMatrix();

			rpr_float mfloats[4][4];
			mLight.get(mfloats);

			float scale = 2.5f; // for equal Maya's default light
			m_defaultLight.SetRadiantPower(scale, scale, scale);
			m_defaultLight.SetTransform(mfloats[0]);

			setCameraAttributeChanged(true);
		}
	}
}

void FireRenderContext::setRenderMode(RenderMode renderMode)
{
	RPR_THREAD_ONLY;
	DebugPrint("FireRenderContext::setRenderMode( %d )", renderMode);

	GetContext().SetParameter(RPR_CONTEXT_RENDER_MODE, renderMode);
	setDirty();
}

void FireRenderContext::SetPreviewMode(int preview)
{
	GetContext().SetParameter(RPR_CONTEXT_PREVIEW, preview);
}

void FireRenderContext::cleanScene()
{
	FireRenderThread::RunOnceProcAndWait([this]()
	{
		DebugPrint("FireRenderContext::cleanScene()");
		LOCKMUTEX(this);
		removeCallbacks();

		// Remove shapes first (It is connected with issue in Hybrid. We should clean up all meshes first before deleting lights)
		FireRenderObjectMap::iterator it = m_sceneObjects.begin();

		while (it != m_sceneObjects.end())	
		{
			FireRenderMesh* fireRenderMesh = dynamic_cast<FireRenderMesh*> (it->second.get());
			FireRenderLight* fireRenderLight = dynamic_cast<FireRenderLight*> (it->second.get());

			if (fireRenderMesh != nullptr)
			{
				fireRenderMesh->setVisibility(false);
				it = m_sceneObjects.erase(it);
			}
			else if (fireRenderLight != nullptr && fireRenderLight->data().isAreaLight)
			{
				fireRenderLight->detachFromScene();
				it = m_sceneObjects.erase(it);
			}
			else
			{
				it++;
			}
		}

		// detach lights before the rest of the objects to unlink lights correctly
		it = m_sceneObjects.begin();
		while (it != m_sceneObjects.end())
		{
			FireRenderLight* fireRenderLight = dynamic_cast<FireRenderLight*> (it->second.get());
			FireRenderEnvLight* envLight = dynamic_cast<FireRenderEnvLight*>(it->second.get());

			if (fireRenderLight != nullptr)
			{
				fireRenderLight->detachFromScene();
				it = m_sceneObjects.erase(it);
			}
			else if (envLight != nullptr)
			{
				envLight->detachFromScene();
				it = m_sceneObjects.erase(it);
			}
			else
			{
				it++;
			}
		}

		m_sceneObjects.clear();

		m_camera.clear();
		m_defaultLight.Reset();
		m.Reset();
		m_denoiserFilter.reset();

		if (white_balance)
		{
			rprContextDetachPostEffect(context(), white_balance.Handle());
			white_balance.Reset();
		}
		if (simple_tonemap)
		{
			rprContextDetachPostEffect(context(), simple_tonemap.Handle());
			simple_tonemap.Reset();
		}
		if (m_tonemap)
		{
			m_tonemap.reset();
		}
		if (m_normalization)
		{
			rprContextDetachPostEffect(context(), m_normalization.Handle());
			m_normalization.Reset();
		}
		if (gamma_correction)
		{
			rprContextDetachPostEffect(context(), gamma_correction.Handle());
			gamma_correction.Reset();
		}

		for (auto& fb : m.framebufferAOV)
			fb.Reset();

		for (auto& fb : m.framebufferAOV_resolved)
			fb.Reset();

		scope.Reset();
	});
}

void FireRenderContext::cleanSceneAsync(std::shared_ptr<FireRenderContext> refToKeepAlive)
{
	m_cleanSceneFuture = std::async( [] (std::shared_ptr<FireRenderContext> refToKeepAlive)
		{ refToKeepAlive->cleanScene(); }, refToKeepAlive);
}

void FireRenderContext::initSwatchScene()
{
	DebugPrint("FireRenderContext::buildSwatchScene(...)");

	auto createFlags = FireMaya::Options::GetContextDeviceFlags();

	rpr_int res;
	if (!createContextEtc(createFlags, true, &res))
	{
		MString msg;
		FireRenderError errorToShow(res, msg, true);
		throw res;
	}

	enableAOV(RPR_AOV_COLOR);

	FireRenderMesh* mesh = new FireRenderMesh(this, MDagPath());
	mesh->buildSphere();
	mesh->setVisibility(true);
	m_sceneObjects["mesh"] = std::shared_ptr<FireRenderObject>(mesh);

	if (mesh)
	{
		if (auto shader = GetShader(MObject()))
		{
			for (auto& element : mesh->Elements())
				element.shape.SetShader(shader);
		}
	}

	m_camera.buildSwatchCamera();

	FireRenderLight *light = new FireRenderLight(this, MDagPath());
	light->buildSwatchLight();
	light->attachToScene();
	m_sceneObjects["light"] = std::shared_ptr<FireRenderObject>(light);

	m_globals.readFromCurrentScene();
	setupContextPostSceneCreation(m_globals);

	UpdateCompletionCriteriaForSwatch();
}

void FireRenderContext::UpdateCompletionCriteriaForSwatch()
{
	CompletionCriteriaParams completionParams;

	bool enableSwatches = false;

	int iterations = FireRenderGlobalsData::getThumbnailIterCount(&enableSwatches);

	if (!enableSwatches)
	{
		iterations = 0;
	}

	completionParams.completionCriteriaMaxIterations = iterations;
	completionParams.completionCriteriaMinIterations = iterations;

	// Time will be infinite. Left all "hours", "minutes" and "seconds" as 0.

	setSamplesPerUpdate(iterations);
	setCompletionCriteria(completionParams);
}

void FireRenderContext::render(bool lock)
{
	RPR_THREAD_ONLY;
	LOCKMUTEX((lock ? this : nullptr));

	auto context = scope.Context();

	if (!context)
		return;

	OnPreRender();

	if (m_restartRender)
	{
		int MaxAOV = GetAOVMaxValue();
		for (int i = 0; i < MaxAOV; ++i) 
		{
			if (aovEnabled[i])
			{
				m.framebufferAOV[i].Clear();

				if (m.framebufferAOV_resolved[i].IsValid())
				{
					m.framebufferAOV_resolved[i].Clear();
				}
			}
		}

		if (m_interactive && m_denoiserChanged && IsDenoiserEnabled())
		{
			turnOnAOVsForDenoiser(true);
			if (GetRenderType() != RenderType::ViewportRender)
			{
				setupDenoiserFB();
			}
			m_denoiserChanged = false;
		}

		m_restartRender = false;
		m_renderStartTime = GetCurrentChronoTime();
		m_currentIteration = 0;
		m_currentFrame = 0;

		if (m_IterationsPowerOf2Mode)
		{
			m_samplesPerUpdate = 1;
		}
	}

	// may need to change iteration step
	int iterationStep = m_samplesPerUpdate;

	if (!m_completionCriteriaParams.isUnlimitedIterations())
	{
		int remainingIterations = m_completionCriteriaParams.completionCriteriaMaxIterations - m_currentIteration;
		if (remainingIterations < iterationStep)
		{
			iterationStep = remainingIterations;
		}
	}

	context.SetParameter(RPR_CONTEXT_ITERATIONS, iterationStep);
	context.SetParameter(RPR_CONTEXT_FRAMECOUNT, m_currentFrame);

	ContextWorkProgressData progressData;

	progressData.currentIndex = m_currentIteration;
	progressData.totalCount = m_completionCriteriaParams.completionCriteriaMaxIterations;
	progressData.currentTimeInMiliseconds = TimeDiffChrono<std::chrono::milliseconds>(GetCurrentChronoTime(), m_workStartTime);
	progressData.progressType = ProgressType::RenderPassStarted;

	TriggerProgressCallback(progressData);

	if (m_useRegion)
		context.RenderTile(m_region.left, m_region.right+1, m_height - m_region.top - 1, m_height - m_region.bottom);
	else
		context.Render();

	if (m_IterationsPowerOf2Mode && !m_globals.contourIsEnabled)
	{
		const int maxIterations = 32;
		if (m_samplesPerUpdate < maxIterations)
		{
			m_samplesPerUpdate = m_samplesPerUpdate * 2;
		}
	}

	if (m_currentIteration == 0)
	{
		m_lastRenderStartTime = std::chrono::system_clock::now();

		DebugPrint("RPR GPU Memory used: %dMB", context.GetMemoryUsage() >> 20);
	}

	m_currentIteration += iterationStep;
	m_currentFrame++;

	m_cameraAttributeChanged = false;
}

void FireRenderContext::setSamplesPerUpdate(int samplesPerUpdate)
{
	m_samplesPerUpdate = samplesPerUpdate;
}

void FireRenderContext::saveToFile(MString& filePath, const ImageFileDescription& imgDescription)
{
	RPR_THREAD_ONLY;
	DebugPrint("FireRenderContext::saveToFile(...)");
	float* data = new float[m_width * m_height * 4];

	{
		LOCKMUTEX(this);

		rpr_framebuffer fb = frameBufferAOV_Resolved(RPR_AOV_COLOR);

		size_t dataSize = 0;
		auto frstatus = rprFrameBufferGetInfo(fb, RPR_FRAMEBUFFER_DATA, 0, NULL, &dataSize);
		checkStatus(frstatus);

		frstatus = rprFrameBufferGetInfo(fb, RPR_FRAMEBUFFER_DATA, dataSize, &data[0], NULL);
		checkStatus(frstatus);
	}

	OIIO::ImageOutput *outImage = OIIO::ImageOutput::create(filePath.asChar());
	if (outImage)
	{
		OIIO::ImageSpec imgSpec(m_width, m_height, 4, OIIO::TypeDesc::FLOAT);
		outImage->open(filePath.asChar(), imgSpec);
		outImage->write_image(OIIO::TypeDesc::FLOAT, &data[0]);
		outImage->close();
		delete outImage;
	}
	else
	{
		float* tmpBuffer = new float[m_width * m_height * 4];
		memcpy(&tmpBuffer[0], &data[0], sizeof(float) * m_width * m_height * 4);
		for (unsigned int y = 0; y < (m_height); y++)
		{
			memcpy(
				&data[y * m_width * 4],
				&tmpBuffer[(m_height - y - 1) * m_width * 4],
				sizeof(float) * m_width * 4
			);
		}
		delete[] tmpBuffer;

		MStatus status;
		//save 24bit with MImage
		//for some reason Maya invert the red channel with the blue channel so compensate this effect
		for (unsigned int pId = 0; pId < (m_width * m_height); pId++)
		{
			float red = data[4 * pId];
			data[4 * pId] = data[4 * pId + 2];
			data[4 * pId + 2] = red;
		}
		MImage newImage;
		newImage.create(m_width, m_height, 4u, MImage::kFloat);
		newImage.setFloatPixels(&data[0], m_width, m_height);
		newImage.convertPixelFormat(MImage::kByte);
		newImage.setRGBA(true);
		status = newImage.writeToFile(filePath, imgDescription.extension.c_str());
		if (status != MS::kSuccess)
		{
			MGlobal::displayError("Unable to save " + filePath);
		}
	}

	delete[] data;
}

std::vector<float> FireRenderContext::getRenderImageData()
{
	RPR_THREAD_ONLY;
	DebugPrint("FireRenderContext::getRenderImageData()");

	rpr_framebuffer fb = frameBufferAOV_Resolved(RPR_AOV_COLOR);

	size_t dataSize = 0;
	auto frstatus = rprFrameBufferGetInfo(fb, RPR_FRAMEBUFFER_DATA, 0, NULL, &dataSize);
	checkStatus(frstatus);

	std::vector<float> data(m_width * m_height * 4);
	frstatus = rprFrameBufferGetInfo(fb, RPR_FRAMEBUFFER_DATA, dataSize, data.data(), NULL);
	checkStatus(frstatus);

	return data;
}

unsigned int FireRenderContext::width() const
{
	return m_width;
}

unsigned int FireRenderContext::height() const
{
	return m_height;
}

bool FireRenderContext::isRenderView() const
{
	return m_isRenderView;
}

bool FireRenderContext::createContext(rpr_creation_flags createFlags, rpr_context& result, int* pOutRes)
{
	RPR_THREAD_ONLY;

	rpr_context context = nullptr;

	bool useThread = (createFlags & RPR_CREATION_FLAGS_ENABLE_CPU) == RPR_CREATION_FLAGS_ENABLE_CPU;
	DebugPrint("* Creating Context: %d (0x%x) - useThread: %d", createFlags, createFlags, useThread);

    if (MetalContextAvailable())
    {
        if (isMetalOn() && !(createFlags & RPR_CREATION_FLAGS_ENABLE_CPU))
        {
            createFlags = createFlags | RPR_CREATION_FLAGS_ENABLE_METAL;
        }
    }
    else
    {
        createFlags = createFlags & ~RPR_CREATION_FLAGS_ENABLE_METAL;
    }

	int res = CreateContextInternal(createFlags, &context);

	if (pOutRes != nullptr)
	{
		*pOutRes = res;
	}

	// display a warning when contour and cpu are selected since contour is not rendered on cpu and on cpu + gpu.
	bool useCpu = (createFlags & RPR_CREATION_FLAGS_ENABLE_CPU) == RPR_CREATION_FLAGS_ENABLE_CPU;
	bool isContour = Globals().contourIsEnabled;
	if (isContour && useCpu)
	{
		MGlobal::displayWarning("Contour is not rendered on CPU. Please choose only GPU on 'Render Devices' to render contour");
	}

	switch (res)
	{
	case RPR_SUCCESS:
		result = context;
		FireRenderThread::UseTheThread(useThread);
		return true;
	case RPR_ERROR_UNSUPPORTED:
		MGlobal::displayError("RPR_ERROR_UNSUPPORTED");
		return false;
	}

	MGlobal::displayError(MString("rprCreateContext returned error: ") + res);

	DebugPrint("rprCreateContext returned error: %d (%s, %d)", res, __FILE__, __LINE__);
	return false;
}

int FireRenderContext::getThreadCountToOverride() const
{
	bool useViewportParams = isInteractive();
	                     
	bool isOverriden;
	int cpuThreadCount;

	FireRenderGlobalsData::getCPUThreadSetup(isOverriden, cpuThreadCount, m_RenderType);

	if (isOverriden)
	{
		return cpuThreadCount;
	}

	// means we dont want to override cpu thread count
	return 0;
}

void FireRenderContext::BuildLateinitObjects()
{
	for (const MDagPath path : m_LateinitMASHInstancers)
	{
		CreateSceneObject<InstancerMASH, NodeCachingOptions::AddPath>(path);
	}
	m_LateinitMASHInstancers.clear();
}

bool FireRenderContext::createContextEtc(rpr_creation_flags creation_flags, bool destroyMaterialSystemOnDelete, int* pOutRes, bool createScene /*= true*/)
{
	return FireRenderThread::RunOnceAndWait<bool>([this, &creation_flags, destroyMaterialSystemOnDelete, pOutRes, createScene]()
	{
		RPR_THREAD_ONLY;

		DebugPrint("FireRenderContext::createContextEtc(%d)", creation_flags);

		rpr_context handle;
		bool contextCreated = createContext(creation_flags, handle, pOutRes);
		if (!contextCreated)
		{
			MGlobal::displayError("Unable to create Radeon ProRender context.");
			return false;
		}

		scope.Init(handle, destroyMaterialSystemOnDelete, createScene);

#ifdef _DEBUG
		static int dumpDebug;
		if (!dumpDebug)
		{
			scope.Context().DumpParameterInfo();
			dumpDebug = 1;
		}
#endif

		for (auto& fb : m.framebufferAOV)
			fb.Reset();

		for (auto& fb : m.framebufferAOV_resolved)
			fb.Reset();

		return true;
	});
}

rpr_context FireRenderContext::context()
{
	RPR_THREAD_ONLY;

	assert(scope.Context());
	return scope.Context().Handle();
}

rpr_material_system FireRenderContext::materialSystem()
{
	RPR_THREAD_ONLY;

	assert(scope.MaterialSystem());
	return scope.MaterialSystem().Handle();
}

rpr_framebuffer FireRenderContext::frameBufferAOV(int aov) const
{
	RPR_THREAD_ONLY;

	assert(m.framebufferAOV[aov]);
	return m.framebufferAOV[aov].Handle();
}

rpr_framebuffer FireRenderContext::frameBufferAOV_Resolved(int aov) {
	RPR_THREAD_ONLY;
	frw::FrameBuffer fb;

	if (m.framebufferAOV[aov].Handle() == nullptr)
	{
		return nullptr;
	}

	if (needResolve() && aov != RPR_AOV_DEEP_COLOR)
	{
		//resolve tone mapping
		m.framebufferAOV[aov].Resolve(m.framebufferAOV_resolved[aov], aov != RPR_AOV_COLOR);
		fb = m.framebufferAOV_resolved[aov];
	}
	else
	{
		fb = m.framebufferAOV[aov];
	}
	return fb.Handle();
}

// -----------------------------------------------------------------------------
bool FireRenderContext::ConsiderShadowReflectionCatcherOverride(const ReadFrameBufferRequestParams& params)
{
	RPR_THREAD_ONLY;

	/**
	 * Shadow catcher can work only if COLOR, BACKGROUND, OPACITY and SHADOW_CATCHER AOVs are turned on.
	 * If all of them are turned on and shadow catcher is requested - run composite pipeline.
	 */
	bool isShadowCather = (params.aov == RPR_AOV_COLOR) &&
		params.mergeShadowCatcher &&
		m.framebufferAOV[RPR_AOV_SHADOW_CATCHER] &&
		m.framebufferAOV[RPR_AOV_MATTE_PASS] &&
		scope.GetShadowCatcherShader();

	/**
	 * Reflection catcher can work only if COLOR, BACKGROUND, OPACITY and REFLECTION_CATCHER AOVs are turned on.
	 * If all of them are turned on and reflection catcher is requested - run composite pipeline.
	 */
	bool isReflectionCatcher = (params.aov == RPR_AOV_COLOR) &&
		params.mergeShadowCatcher &&
		m.framebufferAOV[RPR_AOV_REFLECTION_CATCHER] &&
		m.framebufferAOV[RPR_AOV_BACKGROUND] &&
		m.framebufferAOV[RPR_AOV_OPACITY] &&
		scope.GetReflectionCatcherShader();

	if ((!isReflectionCatcher) && (!isShadowCather))
		return false; // SC or RC not used

	std::lock_guard<std::mutex> lock(m_rifLock);

	if (isShadowCather && isReflectionCatcher)
	{
		rifReflectionShadowCatcherOutput(params);
		return true;
	}

	if (isShadowCather)
	{
		rifShadowCatcherOutput(params);
		return true;
	}

	if (isReflectionCatcher)
	{
		rifReflectionCatcherOutput(params);
		return true;
	}

	// SC or RC not used
	return false;
}

#ifdef _DEBUG
void FireRenderContext::DebugDumpFramebufferAOV(int aov, const char* pathToFile /*= nullptr*/) const
{
	std::map<unsigned int, std::string> aovNames =
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
		,{RPR_AOV_MATTE_PASS, "RPR_AOV_MATTE_PASS" }
		,{RPR_AOV_CRYPTOMATTE_MAT0, "RPR_AOV_CRYPTOMATTE_MAT0" }
		,{RPR_AOV_CRYPTOMATTE_MAT1, "RPR_AOV_CRYPTOMATTE_MAT1" }
		,{RPR_AOV_CRYPTOMATTE_MAT2, "RPR_AOV_CRYPTOMATTE_MAT2" }
		,{RPR_AOV_CRYPTOMATTE_MAT3, "RPR_AOV_CRYPTOMATTE_MAT3" }
		,{RPR_AOV_CRYPTOMATTE_MAT4, "RPR_AOV_CRYPTOMATTE_MAT4" }
		,{RPR_AOV_CRYPTOMATTE_MAT5, "RPR_AOV_CRYPTOMATTE_MAT5" }
		,{RPR_AOV_CRYPTOMATTE_OBJ0, "RPR_AOV_CRYPTOMATTE_OBJ0" }
		,{RPR_AOV_CRYPTOMATTE_OBJ1, "RPR_AOV_CRYPTOMATTE_OBJ1" }
		,{RPR_AOV_CRYPTOMATTE_OBJ2, "RPR_AOV_CRYPTOMATTE_OBJ2" }
		,{RPR_AOV_CRYPTOMATTE_OBJ3, "RPR_AOV_CRYPTOMATTE_OBJ3" }
		,{RPR_AOV_CRYPTOMATTE_OBJ4, "RPR_AOV_CRYPTOMATTE_OBJ4" }
		,{RPR_AOV_CRYPTOMATTE_OBJ5, "RPR_AOV_CRYPTOMATTE_OBJ5" }
		,{RPR_AOV_MAX, "RPR_AOV_MAX" }
	};

	static int count = 0;
	count++;
	std::stringstream ssFileNameResolved;

	if (pathToFile != nullptr)
	{
		ssFileNameResolved << pathToFile << count;
	}

	ssFileNameResolved << aovNames[aov] << "_resolved.png"; 
	rprFrameBufferSaveToFile(m.framebufferAOV_resolved[aov].Handle(), ssFileNameResolved.str().c_str());

	std::stringstream ssFileNameNOTResolved;

	if (pathToFile != nullptr)
	{
		ssFileNameNOTResolved << pathToFile << count;
	}

	ssFileNameNOTResolved << aovNames[aov] << "_NOT_resolved.png";
	rprFrameBufferSaveToFile(m.framebufferAOV[aov].Handle(), ssFileNameNOTResolved.str().c_str());
}
#endif

std::vector<float> FireRenderContext::GetTonemappedData(bool& result)
{
	try
	{
		m_tonemap->Run();
		std::vector<float> vecData = m_tonemap->GetData();
		result = true;
		return vecData;
	}
	catch (std::exception& e)
	{
		m_denoiserFilter.reset();
		ErrorPrint(e.what());
		MGlobal::displayError("RPR failed to execute denoiser, turning it off.");

		result = false;
		return std::vector<float>();
	}
}

#ifdef _DEBUG
void FireRenderContext::DebugDumpDenoiseSourceAOV(const std::string& pathToFile)
{
	if (m_globals.denoiserSettings.type == FireRenderGlobals::kBilateral)
	{
		m.framebufferAOV_resolved[RPR_AOV_COLOR].DebugDumpFrameBufferToFile(std::string("RPR_AOV_COLOR"));
		m.framebufferAOV_resolved[RPR_AOV_SHADING_NORMAL].DebugDumpFrameBufferToFile(std::string("RPR_AOV_SHADING_NORMAL"));
		m.framebufferAOV_resolved[RPR_AOV_WORLD_COORDINATE].DebugDumpFrameBufferToFile(std::string("RPR_AOV_WORLD_COORDINATE"));
		m.framebufferAOV_resolved[RPR_AOV_DEPTH].DebugDumpFrameBufferToFile(std::string("RPR_AOV_DEPTH"));
		m_pixelBuffers[RPR_AOV_COLOR].debugDump(m_pixelBuffers[
			RPR_AOV_COLOR].height(),
				m_pixelBuffers[RPR_AOV_COLOR].width(),
				std::string("RPR_AOV_COLOR"), pathToFile);

		m_pixelBuffers[RPR_AOV_SHADING_NORMAL].debugDump(m_pixelBuffers[
			RPR_AOV_SHADING_NORMAL].height(),
				m_pixelBuffers[RPR_AOV_SHADING_NORMAL].width(),
				std::string("RPR_AOV_SHADING_NORMAL"), pathToFile);

		m_pixelBuffers[RPR_AOV_WORLD_COORDINATE].debugDump(m_pixelBuffers[
			RPR_AOV_WORLD_COORDINATE].height(),
				m_pixelBuffers[RPR_AOV_WORLD_COORDINATE].width(),
				std::string("RPR_AOV_WORLD_COORDINATE"), pathToFile);

		m_pixelBuffers[RPR_AOV_DEPTH].debugDump(m_pixelBuffers[
			RPR_AOV_DEPTH].height(),
				m_pixelBuffers[RPR_AOV_DEPTH].width(),
				std::string("RPR_AOV_DEPTH"), pathToFile);
	}
}
#endif

std::vector<float> FireRenderContext::GetDenoisedData(bool& result)
{	
#ifdef _DEBUG
#ifdef DUMP_FRAMEBUFF
	const std::string pathToFile = "C://debug//fb//";
	DebugDumpDenoiseSourceAOV(pathToFile);
#endif // DUMP_FRAMEBUFF
#endif // DEBUG

	try
	{
		m_denoiserFilter->Run();
		std::vector<float> vecData = m_denoiserFilter->GetData();
		result = true;
		return vecData;
	}
	catch (std::exception& e)
	{
		m_denoiserFilter.reset();
		ErrorPrint(e.what());
		MGlobal::displayError("RPR failed to execute denoiser, turning it off.");

		result = false;
		return std::vector<float>();
	}
}

RV_PIXEL* FireRenderContext::GetAOVData(const ReadFrameBufferRequestParams& params)
{
	rpr_int frstatus = RPR_SUCCESS;
	size_t dataSize;

	// Get data from the RPR frame buffer.
	rpr_framebuffer frameBuffer = frameBufferAOV_Resolved(params.aov);
	frstatus = rprFrameBufferGetInfo(frameBuffer, RPR_FRAMEBUFFER_DATA, 0, nullptr, &dataSize);
	checkStatus(frstatus);

#ifdef _DEBUG
#ifdef DUMP_PIXELS_SOURCE
	rprFrameBufferSaveToFile(frameBuffer, "C:\\temp\\dbg\\3.png");
#endif
#endif

	// Check that the reported frame buffer size
	// in bytes matches the required dimensions.
	assert(dataSize == (sizeof(RV_PIXEL) * params.PixelCount()));

	// Copy the frame buffer into temporary memory, if
	// required, or directly into the supplied pixel buffer.
	if (params.UseTempData())
		m_tempData.resize(params.PixelCount());

	RV_PIXEL* data = params.UseTempData() ? m_tempData.get() : params.pixels;
	frstatus = rprFrameBufferGetInfo(frameBuffer, RPR_FRAMEBUFFER_DATA, dataSize, &data[0], nullptr);
	checkStatus(frstatus);

	return data;
}

void FireRenderContext::ReadOpacityAOV(const ReadFrameBufferRequestParams& params)
{
	// No need to merge opacity for any FB other then color
	if (!params.mergeOpacity || params.aov != RPR_AOV_COLOR)
		return;

	rpr_framebuffer opacityFrameBuffer = frameBufferAOV_Resolved(RPR_AOV_OPACITY);
	if (opacityFrameBuffer == nullptr)
		return;

	size_t dataSize = (sizeof(RV_PIXEL) * params.PixelCount());

	m_opacityData.resize(params.PixelCount());

	if (params.UseTempData())
	{
		m_opacityTempData.resize(params.PixelCount());

		rpr_int frstatus = rprFrameBufferGetInfo(opacityFrameBuffer, RPR_FRAMEBUFFER_DATA, dataSize, m_opacityTempData.get(), nullptr);
		checkStatus(frstatus);

		copyPixels(m_opacityData.get(), m_opacityTempData.get(), params.width, params.height, params.region);
	}
	else
	{
		rpr_int frstatus = rprFrameBufferGetInfo(opacityFrameBuffer, RPR_FRAMEBUFFER_DATA, dataSize, m_opacityData.get(), nullptr);
		checkStatus(frstatus);
	}
}

void FireRenderContext::CombineOpacity(int aov, RV_PIXEL* pixels, unsigned int area)
{
	assert(pixels);

	//combine (Opacity to Alpha)
	// No need to merge opacity for any FB other then color
	if (aov != RPR_AOV_COLOR)
		return;

	combineWithOpacity(pixels, area, m_opacityData.get());
}

RV_PIXEL* FireRenderContext::readFrameBufferSimple(ReadFrameBufferRequestParams& params)
{
	RPR_THREAD_ONLY;

	// debug output (if enabled)
#ifdef DUMP_AOV_SOURCE
	const std::string pathToFile = "C://debug//fb//";
	DebugDumpFramebufferAOV(params.aov, pathToFile.c_str());
#endif

	// process shadow and/or reflection catcher logic
	bool isShadowReflectionCatcherUsed = ConsiderShadowReflectionCatcherOverride(params);
	if (isShadowReflectionCatcherUsed)
		return nullptr;

	// load data from AOV
	return GetAOVData(params);
}

void FireRenderContext::readFrameBuffer(ReadFrameBufferRequestParams& params)
{
	RPR_THREAD_ONLY;

	RV_PIXEL* data = readFrameBufferSimple(params);

	if (data == nullptr)
	{
		return;
	}

	// Read opacity AOV if needed
	ReadOpacityAOV(params);

	// Copy the region from the temporary
	// buffer into supplied pixel memory.
	// _TODO Investigate if "|| IsDenoiserCreated()" is really necessary?  
	if (params.UseTempData() || (IsDenoiserCreated()))
	{
		copyPixels(params.pixels, data, params.width, params.height, params.region);
	}

	//combine (Opacity to Alpha)
	// No need to merge opacity for any FB other then color
	if (params.mergeOpacity)
	{
		CombineOpacity(params.aov, params.pixels, params.region.getArea());
	}
}

#ifdef _DEBUG
void generateBitmapImage(unsigned char *image, int height, int width, int pitch, const char* imageFileName);
#endif

// -----------------------------------------------------------------------------
void FireRenderContext::copyPixels(RV_PIXEL* dest, RV_PIXEL* source,
	unsigned int sourceWidth, unsigned int sourceHeight,
	const RenderRegion& region) const
{
	RPR_THREAD_ONLY;
	// Get region dimensions.
	unsigned int regionWidth = region.getWidth();
	unsigned int regionHeight = region.getHeight();

	for (unsigned int y = 0; y < regionHeight; y++)
	{
		unsigned int destIndex = y * regionWidth;
				
		unsigned int sourceIndex = 
			(sourceHeight - (region.top - y) - 1) * sourceWidth + region.left;

		memcpy(&dest[destIndex], &source[sourceIndex], sizeof(RV_PIXEL) * regionWidth);
	}

#ifdef _DEBUG
#ifdef DUMP_PIXELS_SOURCE
	if (debugDump) {
		static int debugDumpIdx = 0;
		std::vector<RV_PIXEL> sourcePixels;
		for (unsigned int y = 0; y < regionHeight; y++)
		{
			for (unsigned int x = 0; x < regionWidth; x++)
			{
				RV_PIXEL pixel = source[x + y * regionWidth];
				sourcePixels.push_back(pixel);
			}
		}

		std::vector<unsigned char> buffer2;
		for (unsigned int y = 0; y < regionHeight; y++)
		{
			for (unsigned int x = 0; x < regionWidth; x++)
			{
				RV_PIXEL& pixel = sourcePixels[x + y * regionWidth];
				char r = 255 * pixel.r;
				char g = 255 * pixel.g;
				char b = 255 * pixel.b;

				buffer2.push_back(r);
				buffer2.push_back(g);
				buffer2.push_back(b);
				buffer2.push_back(255);
			}
		}

		std::string dumpAddr = "C:\\temp\\dbg\\2.bmp" + std::to_string(debugDumpIdx++);
		unsigned char* dst2 = buffer2.data();
		generateBitmapImage(dst2, sourceHeight, sourceWidth, sourceWidth * 4, dumpAddr.c_str());
	}
#endif
#endif
}

// -----------------------------------------------------------------------------
void FireRenderContext::combineWithOpacity(RV_PIXEL* pixels, unsigned int size, RV_PIXEL *opacityPixels) const
{
	if (opacityPixels != NULL)
	{
		float alpha = 0.0f;
		for (unsigned int i = 0; i < size; i++)
		{
			alpha = opacityPixels[i].r;

			pixels[i].a = alpha;
		}
	}
}

FireRenderObject* FireRenderContext::getRenderObject(const std::string& name)
{
	auto it = m_sceneObjects.find(name);
	if (it != m_sceneObjects.end())
		return it->second.get();
	return nullptr;
}

FireRenderObject* FireRenderContext::getRenderObject(const MDagPath& ob)
{
	auto uuid = getNodeUUid(ob);
	return getRenderObject(uuid);
}

FireRenderObject* FireRenderContext::getRenderObject(const MObject& ob)
{
	auto uuid = getNodeUUid(ob);
	return getRenderObject(uuid);
}


void FireRenderContext::RemoveRenderObject(const MObject& ob)
{
	RPR_THREAD_ONLY;

	for (auto it = m_sceneObjects.begin(); it != m_sceneObjects.end();)
	{
		if (auto frNode = dynamic_cast<FireRenderNode*>(it->second.get()))
		{
			MDagPath dagPath;

			// it is unsafe to call DagPath for the object which has been just removed from scene (crash may occur)
			if (frNode->Object() != ob)
			{
				dagPath = frNode->DagPath();
			}

			if (!dagPath.isValid() || dagPath.node() == ob || dagPath.transform() == ob)
			{
				// remove object from dag path cache
				FireRenderObject* pRobj = getRenderObject(ob);
				if (pRobj != nullptr)
				{
					MDagPath tmpPath;
					bool found = GetNodePath(tmpPath, pRobj->uuid());

					if (found)
					{
						m_nodePathCache.erase(pRobj->uuid());
					}
				}

				// remove object from main meshes cache
				/**
					Sometimes it get fired for Node or Object, but the map of meshes should contain only FireRenderMesh
					Dynamic cast is needed to type check
				*/
				FireRenderMesh* mesh = dynamic_cast<FireRenderMesh*>(frNode);

				// remove object from scene
				frNode->detachFromScene();
				it = m_sceneObjects.erase(it);
				setDirty();

				continue;
			}
		}
		++it;
	}
}

void FireRenderContext::RefreshInstances()
{
	MAIN_THREAD_ONLY;

	std::list<MDagPath> toAdd;
	for (auto it = m_sceneObjects.begin(); it != m_sceneObjects.end();)
	{
		if (auto frNode = dynamic_cast<FireRenderNode*>(it->second.get()))
		{
			auto dagPath = frNode->DagPath();
			if (!dagPath.isValid())
			{
				frNode->detachFromScene();
				it = m_sceneObjects.erase(it);
				setDirty();
				continue;
			}

			MDagPathArray paths;
			if (MStatus::kSuccess == MDagPath::getAllPathsTo(frNode->Object(), paths))
			{
				for (auto path : paths)
				{
					if (!getRenderObject(path))
						toAdd.push_back(path);
				}
			}
		}
		++it;
	}

	for (auto it : toAdd)
		AddSceneObject(it);
}

rpr_scene  FireRenderContext::scene()
{
	RPR_THREAD_ONLY;

	return scope.Scene().Handle();
}

FireRenderCamera & FireRenderContext::camera()
{
	return m_camera;
}

void FireRenderContext::attachCallbacks()
{
	DebugPrint("FireRenderContext(context=%p)::attachCallbacks()", this);
	if (getCallbackCreationDisabled())
		return;

	//Remove old callbacks from this context if any exist
	removeCallbacks();

	MStatus status;
	m_removedNodeCallback = MDGMessage::addNodeRemovedCallback(FireRenderContext::removedNodeCallback, "dependNode", this, &status);
	m_addedNodeCallback = MDGMessage::addNodeAddedCallback(FireRenderContext::addedNodeCallback, "dependNode", this, &status);

	MSelectionList slist;
	MObject node;
	slist.add("RadeonProRenderGlobals");
	slist.getDependNode(0, node);
	m_renderGlobalsCallback = MNodeMessage::addAttributeChangedCallback(node, FireRenderContext::globalsChangedCallback, this, &status);
	slist.add("renderLayerManager");
	slist.getDependNode(1, node);
	m_renderLayerCallback = MNodeMessage::addAttributeChangedCallback(node, FireRenderContext::renderLayerManagerCallback, this, &status);
}

void FireRenderContext::removeCallbacks()
{
	DebugPrint("FireRenderContext(context=%p)::removeCallbacks()", this);
	
	if (m_removedNodeCallback)
		MMessage::removeCallback(m_removedNodeCallback);
	if (m_addedNodeCallback)
		MMessage::removeCallback(m_addedNodeCallback);
	if (m_renderGlobalsCallback)
		MMessage::removeCallback(m_renderGlobalsCallback);
	if (m_renderLayerCallback)
		MMessage::removeCallback(m_renderLayerCallback);

	m_removedNodeCallback = m_addedNodeCallback = m_renderGlobalsCallback = m_renderLayerCallback = 0;
}

void FireRenderContext::removedNodeCallback(MObject &node, void *clientData)
{
	if (!DoesNodeAffectContextRefresh(node))
	{
		return;
	}

	if (auto frContext = GetCallbackContext(clientData))
	{
		DebugPrint("FireRenderContext(context=%p)::removedNodeCallback()", frContext);
		// If we have same node in added list then it wasn't yet processed by FireRenderContext.
		// We should remove it from added list, since it's already removed. Calling any function on such node
		// will lead to crash in Maya
		auto it = std::find(frContext->m_addedNodes.begin(), frContext->m_addedNodes.end(), node);
		if (it != frContext->m_addedNodes.end())
		{
			frContext->m_addedNodes.erase(it);
			return;
		}

		frContext->m_removedNodes.push_back(node);
		frContext->setDirty();
	}
}

void FireRenderContext::addedNodeCallback(MObject &node, void *clientData)
{
	if (auto frContext = GetCallbackContext(clientData))
	{
		DebugPrint("FireRenderContext(context=%p)::addedNodeCallback()", frContext);

		if (DoesNodeAffectContextRefresh(node))
		{
			frContext->m_addedNodes.push_back(node);
			frContext->setDirty();
		}
	}
}

bool FireRenderContext::DoesNodeAffectContextRefresh(const MObject &node)
{
	if (node.isNull())
	{
		return false;
	}

	return node.hasFn(MFn::kDagNode) || node.hasFn(MFn::kDisplayLayer);
}

void FireRenderContext::addNode(const MObject& node)
{
	MAIN_THREAD_ONLY;

	if (!DoesNodeAffectContextRefresh(node))
	{
		return;
	}

	if (node.hasFn(MFn::kDagNode))
	{
		MFnDagNode dagNode(node);

		MDagPath dagPath;
		if (MStatus::kSuccess == dagNode.getPath(dagPath))
			AddSceneObject(dagPath);
	}
	else if (node.hasFn(MFn::kDisplayLayer))
	{
		FireRenderDisplayLayer* layer = new FireRenderDisplayLayer(this, node);
		AddSceneObject(layer);
	}
}

void FireRenderContext::removeNode(MObject& node)
{
	MAIN_THREAD_ONLY;

	RemoveRenderObject(node);
}

void FireRenderContext::updateFromGlobals(bool applyLock)
{
	MAIN_THREAD_ONLY;

	if (m_tonemappingChanged)
	{
        if (applyLock)
        {
            LOCKFORUPDATE(this);
        }

		m_globals.readFromCurrentScene();
		updateTonemapping(m_globals);

		m_tonemappingChanged = false;
	}

	if (!m_globalsChanged)
		return;

    if (applyLock)
    {
        LOCKFORUPDATE(this);
    }
    
	auto createFlags = FireMaya::Options::GetContextDeviceFlags(m_RenderType);

	m_globals.readFromCurrentScene();
	setupContextContourMode(m_globals, createFlags);
	setupContextHybridParams(m_globals);
	setupContextAirVolume(m_globals);
	setupContextPostSceneCreation(m_globals);
	setupContextCryptomatteSettings(m_globals);

	updateLimitsFromGlobalData(m_globals);
	updateMotionBlurParameters(m_globals);

	m_camera.setType(m_globals.cameraType);

	m_globalsChanged = false;
}

void FireRenderContext::updateRenderLayers()
{
	MAIN_THREAD_ONLY;

	if (!m_renderLayersChanged)
		return;

	for (const auto& it : m_sceneObjects)
	{
		if (auto frNode = dynamic_cast<FireRenderNode*>(it.second.get()))
		{
			auto node = frNode->Object();
			MDagPath path = MDagPath::getAPathTo(node);
			MFnDependencyNode nodeFn(node);
			MString name = nodeFn.name();
			if (path.isValid())
			{
				if (path.isVisible())
					frNode->attachToScene();
				else
					frNode->detachFromScene();
			}
			else
			{
				MGlobal::displayError("invalid path " + name);
			}
		}
	}

	m_renderLayersChanged = false;
}

// TODO Need to be refactored in a more flexible way
void FireRenderContext::globalsChangedCallback(MNodeMessage::AttributeMessage msg, MPlug &plug, MPlug &otherPlug, void *clientData)
{
	MAIN_THREAD_ONLY

	DebugPrint("FireRenderContext::globalsChangedCallback(%d, %s, isNull=%d, 0x%x)", msg,  plug.name().asUTF8(), otherPlug.isNull(), clientData);

	if (auto frContext = GetCallbackContext(clientData))
	{
		AutoMutexLock lock(frContext->m_dirtyMutex);

		bool restartRender = false;
		if (FireRenderGlobalsData::isTonemapping(plug.name()))
		{
			frContext->m_tonemappingChanged = true;
			restartRender = true;
		}
		else
		{
			if (FireRenderGlobalsData::isDenoiser(plug.name()))
			{
				frContext->m_denoiserChanged = true;
				restartRender = true;
			}
			else if (FireRenderGlobalsData::IsMotionBlur(plug.name()))
			{
				restartRender = true;
			}
			else if (FireRenderGlobalsData::IsAirVolume(plug.name()))
			{
				restartRender = true;
			}

			RenderType renderType = frContext->GetRenderType();
			RenderQuality quality = GetRenderQualityForRenderType(renderType);

			if (!frContext->IsRenderQualitySupported(quality))
			{
				// If current Render Context does not support newly selected render quality we need to restart the render
				frContext->m_DoesContextSupportCurrentSettings = false;			
			}

			frContext->m_globalsChanged = true;

			frContext->setDirty();

			if (restartRender)
			{
				frContext->m_restartRender = restartRender;
			}
		}
	}
}

void FireRenderContext::updateMotionBlurParameters(const FireRenderGlobalsData& globalData)
{
	RPR_THREAD_ONLY;

	bool updateMeshes = false;
	bool updateCamera = false;

	// Here is logic about proper update only necessary objects depending of type of Context
	if (isInteractive())
	{
		if (m_viewportMotionBlur != globalData.viewportMotionBlur)
		{
			updateMeshes = true;
			updateCamera = true;
		}
	}
	else
	{
		if (m_motionBlur == false && globalData.motionBlur == false)
		{
			return;
		}

		if (m_motionBlur != globalData.motionBlur)
		{
			updateMeshes = true;
			updateCamera = true;
		}

		if (m_cameraMotionBlur != m_globals.cameraMotionBlur)
		{
			updateCamera = true;
		}
	}

	if (m_motionBlurCameraExposure != m_globals.motionBlurCameraExposure)
	{
		updateCamera = true;
	}

	if (updateMeshes)
	{
		for (const auto& it : m_sceneObjects)
		{
			if (auto frMesh = dynamic_cast<FireRenderMesh*>(it.second.get()))
			{
				frMesh->setDirty();
			}
		}
	}

	if (updateCamera)
	{
		m_camera.setDirty();
	}

	setMotionBlurParameters(globalData);
}

void FireRenderContext::setMotionBlurParameters(const FireRenderGlobalsData& globalData)
{
	m_motionBlur = globalData.motionBlur;
	m_cameraMotionBlur = m_globals.cameraMotionBlur;
	m_motionBlurCameraExposure = m_globals.motionBlurCameraExposure;
	m_viewportMotionBlur = globalData.viewportMotionBlur;
	m_velocityAOVMotionBlur = globalData.velocityAOVMotionBlur;

	m_motionSamples = globalData.motionSamples;
}

bool FireRenderContext::isInteractive() const
{
	return (m_RenderType == RenderType::IPR) || (m_RenderType == RenderType::ViewportRender);
}

void FireRenderContext::renderLayerManagerCallback(MNodeMessage::AttributeMessage msg, MPlug & plug, MPlug & otherPlug, void * clientData)
{
	if (auto frContext = GetCallbackContext(clientData))
	{
		if (plug.partialName() == "crl")
		{
			AutoMutexLock lock(frContext->m_dirtyMutex);

			frContext->m_renderLayersChanged = true;
			frContext->setDirty();
		}
	}
}

bool FireRenderContext::AddSceneObject(FireRenderObject* ob)
{
	RPR_THREAD_ONLY;

	if (ob->uuid().empty())
		return false;

	if (m_sceneObjects.find(ob->uuid()) != m_sceneObjects.end())
		DebugPrint("ERROR: Replacing existing object without deleting first");

	m_sceneObjects[ob->uuid()] = std::shared_ptr<FireRenderObject>(ob);
	ob->setDirty();

	return true;
}

bool IsXgenGuides(MObject& node)
{
	MFnDependencyNode fnNode(node);
	MStatus returnStatus;

	// - get out attribute
	MObject attr = fnNode.attribute("outSplineData", &returnStatus);
	CHECK_MSTATUS(returnStatus)

	MPlug plug = fnNode.findPlug(attr, false, &returnStatus);
	CHECK_MSTATUS(returnStatus)

	// - iterate through node connections; search for connected xgmSplineDescription
	MItDependencyGraph itdep(
		plug,
		MFn::kPluginDependNode,
		MItDependencyGraph::kDownstream,
		MItDependencyGraph::kDepthFirst,
		MItDependencyGraph::kPlugLevel,
		&returnStatus);
	CHECK_MSTATUS(returnStatus)

	for (; !itdep.isDone(); itdep.next())
	{
		MFnDependencyNode connectionNode(itdep.currentItem());
		MString connectionNode_typename = connectionNode.typeName();

		// xgmSplineDescription found => this is guides => shouldn't create hairs
		if (connectionNode_typename == "xgmSplineDescription")
			return true;
	}

	return false;
}

bool FireRenderContext::AddSceneObject(const MDagPath& dagPath)
{
	MAIN_THREAD_ONLY;
	using namespace FireMaya;

	FireRenderObject* ob = nullptr;
	MObject node = dagPath.node();

	bool volumeSupported = IsVolumeSupported();
	bool hairSupported = IsHairSupported();

	if (node.hasFn(MFn::kDagNode))
	{
		MFnDagNode dagNode(node);
		MDagPath dagPathTmp;

		bool isGPUCacheNode = false;

		if (isGeometry(node))
		{
			ob = CreateSceneObject<FireRenderMesh, NodeCachingOptions::AddPath>(dagPath);
		}
		else if (dagNode.typeId() == TypeId::FireRenderIESLightLocator
			|| isLight(node)
			|| VRay::isNonEnvironmentLight(dagNode))
		{
			if (dagNode.typeName() == "ambientLight") 
			{
				ob = CreateSceneObject<FireRenderEnvLight, NodeCachingOptions::AddPath>(dagPath);
			}
			else if (dagNode.typeId() == FireMaya::TypeId::FireRenderPhysicalLightLocator)
			{
				if (IsPhysicalLightTypeSupported(FireRenderPhysLight::GetPhysLightType(dagPath.node())))
				{
					ob = CreateSceneObject<FireRenderPhysLight, NodeCachingOptions::AddPath>(dagPath);
				}
			}
			else
			{
				ob = CreateSceneObject<FireRenderLight, NodeCachingOptions::AddPath>(dagPath);
			}
		}
		else if (dagNode.typeId() == TypeId::FireRenderIBL
			|| dagNode.typeId() == TypeId::FireRenderEnvironmentLight
			|| dagNode.typeName() == "ambientLight"
			|| VRay::isEnvironmentLight(dagNode))
		{
			ob = CreateEnvLight(dagPath);
		}
		else if (dagNode.typeId() == TypeId::FireRenderSkyLocator
			|| VRay::isSkyLight(dagNode))
		{
			ob = CreateSky(dagPath);
		}
		else if (dagNode.typeName() == "fluidShape" && volumeSupported) // Maya native volume
		{
			if (IsNorthstarVolumeSupported())
			{
				ob = CreateSceneObject<NorthstarFluidVolume, NodeCachingOptions::AddPath>(dagPath);
			}
			else
			{
				ob = CreateSceneObject<FireRenderFluidVolume, NodeCachingOptions::AddPath>(dagPath);
			}
		}
		else if (dagNode.typeName() == "RPRVolume" && volumeSupported) // can read .vdb files
		{
			if (IsNorthstarVolumeSupported()) 
			{
				ob = CreateSceneObject<NorthstarRPRVolume, NodeCachingOptions::AddPath>(dagPath);
			}
			else
			{
				ob = CreateSceneObject<FireRenderRPRVolume, NodeCachingOptions::AddPath>(dagPath);
			}
		}
		else if (dagNode.typeName() == "instancer")
		{
			m_LateinitMASHInstancers.push_back(dagPath);
		}
		else if (dagNode.typeName() == "xgmSplineDescription")
		{
			if (!IsXgenGuides(node) && hairSupported)
			{
				// check if xgmSplineDescription is a guides for other xgmSplineDescription
				// don't need to create hair object if that is the case
				ob = CreateSceneObject<FireRenderHairXGenGrooming, NodeCachingOptions::AddPath>(dagPath);
			}
			else if (dagNode.typeName() == "HairShape" && hairSupported)
			{
				// check if xgmSplineDescription is a guides for other xgmSplineDescription
				// don't need to create hair object if that is the case
				ob = CreateSceneObject<FireRenderHairXGenGrooming, NodeCachingOptions::AddPath>(dagPath);
			}
		}
		else if (dagNode.typeName() == "HairShape" && hairSupported)
		{
			ob = CreateSceneObject<FireRenderHairOrnatrix, NodeCachingOptions::AddPath>(dagPath);
		}
		else if (isTransformWithInstancedShape(node, dagPathTmp, isGPUCacheNode))
		{
			if (isGPUCacheNode)
			{
				#ifdef WIN32
				ob = CreateSceneObject<FireRenderGPUCache, NodeCachingOptions::DontAddPath>(dagPathTmp);
				#endif
			}
			else
			{
				ob = CreateSceneObject<FireRenderMesh, NodeCachingOptions::DontAddPath>(dagPathTmp);
			}
		}
		else if (dagNode.typeName() == "pfxHair" && hairSupported)
		{
			ob = CreateSceneObject<FireRenderHairNHair, NodeCachingOptions::AddPath>(dagPath);
		}
		else if (dagNode.typeName() == "locator" && m_bIsGLTFExport)
		{
			// Custom locators with custom RPR "RPRIsEmitter" flag set to 1 should be treated as custom emitters
			// Needed for DEMO preparations
			MPlug plug = dagNode.findPlug("RPRIsEmitter", false);

			if (!plug.isNull() && plug.asInt() > 0)
			{
				ob = CreateSceneObject<FireRenderCustomEmitter, NodeCachingOptions::DontAddPath>(dagPath);
			}
		}
		else if (dagNode.typeName() == "transform")
		{
			ob = CreateSceneObject<FireRenderNode, NodeCachingOptions::AddPath>(dagPath);
		}
#ifdef WIN32
		else if (dagNode.typeName() == "gpuCache")
		{
			ob = CreateSceneObject<FireRenderGPUCache, NodeCachingOptions::AddPath>(dagPath);
		}
#endif
		else
		{
			std::string typeName = dagNode.typeName().asUTF8();
			std::string nodeName = dagNode.name().asUTF8();
			DebugPrint("Ignoring %s: %s", dagNode.typeName().asUTF8(), dagNode.name().asUTF8());
		}
	}
	else
	{
		MFnDependencyNode depNode(node);
		std::string typeName = depNode.typeName().asUTF8();
		std::string nodeName = depNode.name().asUTF8();
		DebugPrint("Ignoring %s: %s", depNode.typeName().asUTF8(), depNode.name().asUTF8());
	}

	return !!ob;
}

FireRenderEnvLight* FireRenderContext::CreateEnvLight(const MDagPath& dagPath)
{
	return CreateSceneObject<FireRenderEnvLight, NodeCachingOptions::AddPath>(dagPath);
}

FireRenderSky* FireRenderContext::CreateSky(const MDagPath& dagPath)
{
	return CreateSceneObject<FireRenderSky, NodeCachingOptions::AddPath>(dagPath);
}

void FireRenderContext::setDirty()
{
	m_dirty = true;
}


bool FireRenderContext::isDirty()
{
	return m_dirty || (m_dirtyObjects.size() != 0) || m_cameraDirty || m_tonemappingChanged;
}

bool FireRenderContext::needsRedraw(bool setToFalseOnExit)
{
	bool value = m_needRedraw;

	if (setToFalseOnExit)
		m_needRedraw = false;

	return value;
}

void FireRenderContext::disableSetDirtyObjects(bool disable)
{
	m_DisableSetDirtyObjects = disable;
}

void FireRenderContext::setDirtyObject(FireRenderObject* obj)
{
	if (m_DisableSetDirtyObjects)
	{
		return;
	}

	if (obj == &m_camera)
	{
		m_cameraDirty = true;
		return;
	}

	// We should skip inactive cameras, because their changes shouldn't affect result image
	// If ignore this step - image in IPR would redraw when moving different camera in viewport
	// That image redrawing in IPR causes black square artifats
	if (m_camera.DagPath().transform() != obj->Object())
	{
		MItDag itDag;
		MStatus status = itDag.reset(obj->Object(), MItDag::kDepthFirst, MFn::kCamera);
		CHECK_MSTATUS(status);
		for (; !itDag.isDone(); itDag.next())
		{
			return;
		}
	}

	AutoMutexLock lock(m_dirtyMutex);

	// Find the object in objects list
	{
		auto it = m_sceneObjects.find(obj->uuid());
		if (it != m_sceneObjects.end())
		{
			std::shared_ptr<FireRenderObject> ptr = it->second;
			m_dirtyObjects[obj] = ptr;
		}
	}
}

HashValue FireRenderContext::GetStateHash()
{
	HashValue hash(size_t(this));

	for (auto& it : m_sceneObjects)
	{
		if (it.second)
			hash << it.second->GetStateHash();
	}

	hash << m_camera.GetStateHash();

	return hash;
}

void FireRenderContext::UpdateTimeAndTriggerProgressCallback(ContextWorkProgressData& syncProgressData, ProgressType progressType)
{
	if (progressType != ContextWorkProgressData::ProgressType::Unknown)
	{
		syncProgressData.progressType = progressType;
	}

	syncProgressData.currentTimeInMiliseconds = TimeDiffChrono<std::chrono::milliseconds>(GetCurrentChronoTime(), m_workStartTime);
	TriggerProgressCallback(syncProgressData);
}

void FireRenderContext::TriggerProgressCallback(const ContextWorkProgressData& syncProgressData)
{
	if (m_WorkProgressCallback)
	{
		FireRenderThread::RunProcOnMainThread([this, syncProgressData]()
		{
			m_WorkProgressCallback(syncProgressData);
		});
	}
}

bool FireRenderContext::Freshen(bool lock, std::function<bool()> cancelled)
{
	MAIN_THREAD_ONLY;

	bool shouldCalculateHash = GetRenderType() == RenderType::ViewportRender;

	// update camera world coordinate plug. it needs to have callbacks works in command line mode.
	// In UI it works better because viewport uses camera position in order to render the image thus it is cleaning and updating this plug.
	if (!m_callbackCreationDisabled)
	{
		MDagPath cameraTransformDagPath = m_camera.DagPath();

		// getting dagpath for camera transform here
		cameraTransformDagPath.pop();

		// Requesting value for World Matrix plug. It will recompute value, set plug as "clean" 
		// and call callback if camera postion was changed before.
		cameraTransformDagPath.inclusiveMatrix();
	}

	if (!isDirty() || cancelled())
		return false;

	LOCKFORUPDATE((lock ? this : nullptr));

	m_inRefresh = true;

	updateFromGlobals(false /*applyLock*/);

	decltype(m_addedNodes) addedNodes = m_addedNodes;
	m_addedNodes.clear();

	for (MObject& node : addedNodes)
		addNode(node);

	if (cancelled())
		return false;

	decltype(m_removedNodes) removedNodes = m_removedNodes;
	m_removedNodes.clear();

	for (MObject& node : removedNodes)
		removeNode(node);

	//Should be called when all scene objects are updated
	BuildLateinitObjects();

	bool changed = m_dirty;

	if (m_cameraDirty)
	{
		m_cameraDirty = false;
		m_camera.Freshen(shouldCalculateHash);
		changed = true;
	}

	size_t dirtyObjectsSize = m_dirtyObjects.size();
	ContextWorkProgressData syncProgressData;
	syncProgressData.totalCount = dirtyObjectsSize;
	TimePoint syncStartTime = GetCurrentChronoTime();

	UpdateTimeAndTriggerProgressCallback(syncProgressData, ProgressType::SyncStarted);

	std::deque<std::shared_ptr<FireRenderObject> > meshesToReload; // meshes which would be pre-processed
	std::deque<std::shared_ptr<FireRenderObject> > meshesToFreshen; // meshes which would be freshened

	while (!m_dirtyObjects.empty())
	{
		for (auto it = m_dirtyObjects.begin(); it != m_dirtyObjects.end(); )
		{
			if ((m_state != FireRenderContext::StateRendering) && (m_state != FireRenderContext::StateUpdating))
				return false;

			// Request the object with removal it from the dirty list. Use mutex to prevent list's modifications.
			m_dirtyMutex.lock();

			std::shared_ptr<FireRenderObject> ptr = it->second.lock();

			it = m_dirtyObjects.erase(it);

			m_dirtyMutex.unlock();

			// Now perform update
			if (!ptr)
			{
				DebugPrint("Cancelled freshing null object");
				continue;
			}

			changed = true;

			if (ptr->IsMesh())
			{
				FireRenderMesh* pMesh = dynamic_cast<FireRenderMesh*>(ptr.get());
				assert(pMesh != nullptr);

				bool meshChanged = pMesh->InitializeMaterials();
				bool shouldLoad = pMesh->IsMeshVisible(pMesh->DagPath(), this);

				if (meshChanged && shouldLoad)
				{
					meshesToReload.emplace_back() = ptr;
				}
				else
				{
					pMesh->SetReloadedSafe();
					meshesToFreshen.emplace_back() = ptr;
				}

				continue;
			}

			if (ptr->ShouldForceReload())
			{
				meshesToReload.emplace_back() = ptr;

				continue;
			}

			DebugPrint("Freshing object");

			UpdateTimeAndTriggerProgressCallback(syncProgressData, ProgressType::ObjectPreSync);
			ptr->Freshen(shouldCalculateHash);

			syncProgressData.currentIndex++;
			UpdateTimeAndTriggerProgressCallback(syncProgressData, ProgressType::ObjectSyncComplete);

			if (cancelled())
			{
				return false;
			}
		}
	}

	const bool isDeformationMotionBlurEnabled = motionBlur() && IsDeformationMotionBlurEnabled() && !isInteractive();
	const unsigned int motionSamplesCount = isDeformationMotionBlurEnabled ? motionSamples() : 1;

	// read data from meshes
	MTime initialTime = MAnimControl::currentTime();
	MTime currentTime = initialTime;
	unsigned int currentSampeIdx = 0;

	do
	{
		// iterate through meshes
		for (auto it = meshesToReload.begin(); it != meshesToReload.end(); ++it)
		{
			if (!it->get())
			{
				continue;
			}

			const bool success = it->get()->ReloadMesh(currentSampeIdx);
			if (success && (currentSampeIdx == 0))
			{
				meshesToFreshen.emplace_back() = *it;
			}
		}

		if (++currentSampeIdx >= motionSamplesCount)
			break;

		// positioning on next point of time (starting from currentTime)
		currentTime += (float)currentSampeIdx / (motionSamplesCount - 1);
		MGlobal::viewFrame(currentTime);
		
	} while (currentTime != MAnimControl::maxTime());

	MGlobal::viewFrame(initialTime);

	// process read data (would be done in multiple threads in the future)
	for (auto it = meshesToFreshen.begin(); it != meshesToFreshen.end(); ++it)
	{
		FireRenderObject* pMesh = it->get();
		if (pMesh == nullptr)
			continue;

		UpdateTimeAndTriggerProgressCallback(syncProgressData, ProgressType::ObjectPreSync);
		pMesh->Freshen(shouldCalculateHash);
		syncProgressData.currentIndex++;
		UpdateTimeAndTriggerProgressCallback(syncProgressData, ProgressType::ObjectSyncComplete);
	}

	syncProgressData.elapsedTotal = TimeDiffChrono<std::chrono::milliseconds>(GetCurrentChronoTime(), syncStartTime);
	UpdateTimeAndTriggerProgressCallback(syncProgressData, ProgressType::SyncComplete);

	if (changed)
	{
		UpdateDefaultLights();
		setCameraAttributeChanged(true);
	}

	if (cancelled())
		return false;

	updateRenderLayers();

	m_dirty = false;

	auto hash = GetStateHash();
	DebugPrint("Hash Value: %08X", int(hash));

	m_inRefresh = false;

	m_needRedraw = true;

	return true;
}

void FireRenderContext::SetState(StateEnum newState)
{
	if (m_state == newState)
	{
		return;
	}

	m_state = newState;

	if (m_state == StateEnum::StateExiting)
	{
		ContextWorkProgressData data;
		data.elapsedTotal = TimeDiffChrono<std::chrono::milliseconds>(GetCurrentChronoTime(), m_renderStartTime);
		data.elapsedPostRenderTonemap = TimeDiffChrono<std::chrono::milliseconds>(GetCurrentChronoTime(), m_tonemapStartTime);

		UpdateTimeAndTriggerProgressCallback(data, ProgressType::RenderComplete);
	}
}

bool FireRenderContext::setUseRegion(bool value)
{
	m_useRegion = value && IsRenderRegionSupported();

	return m_useRegion;
}

bool FireRenderContext::useRegion()
{
	return m_useRegion;
}

void FireRenderContext::setRenderRegion(const RenderRegion & region)
{
	m_region = region;
}

RenderRegion FireRenderContext::renderRegion()
{
	return m_region;
}

void FireRenderContext::setCallbackCreationDisabled(bool value)
{
	m_callbackCreationDisabled = value;
}

bool FireRenderContext::getCallbackCreationDisabled()
{
	return m_callbackCreationDisabled;
}

bool FireRenderContext::renderSelectedObjectsOnly() const
{
	return m_renderSelectedObjectsOnly;
}

bool FireRenderContext::motionBlur() const
{
	return !isInteractive() ? m_motionBlur : m_viewportMotionBlur;
}

bool FireRenderContext::cameraMotionBlur() const
{
	return !isInteractive() ? m_cameraMotionBlur : m_viewportMotionBlur;
}

float FireRenderContext::motionBlurCameraExposure() const
{
	return m_motionBlurCameraExposure;
}

unsigned int FireRenderContext::motionSamples() const
{
	return m_motionSamples;
}

void FireRenderContext::setCameraAttributeChanged(bool value)
{
	m_cameraAttributeChanged = value;
	if (value)
		m_restartRender = true;
}

void FireRenderContext::setCompletionCriteria(const CompletionCriteriaParams& completionCriteriaParams)
{
	m_completionCriteriaParams = completionCriteriaParams;
}

const CompletionCriteriaParams& FireRenderContext::getCompletionCriteria(void) const
{
	return m_completionCriteriaParams;
}

bool FireRenderContext::isUnlimited()
{
	// Corresponds to the kUnlimited enum in FireRenderGlobals.
	return m_completionCriteriaParams.isUnlimited();
}

std::chrono::time_point<std::chrono::steady_clock> start = std::chrono::high_resolution_clock::now();

void FireRenderContext::setStartedRendering()
{
	m_renderStartTime = GetCurrentChronoTime();
	m_currentIteration = 0;
}

bool FireRenderContext::keepRenderRunning()
{
	// Check if adaptive sampling finished it's work.
	// At zero iterarion there's no render info for active pixel count.
	if (m_currentIteration != 0 && GetScope().Context().IsAdaptiveSamplingFinalized())
	{
		return false;
	}

	// check iteration count completion criteria
	if (!m_completionCriteriaParams.isUnlimitedIterations() &&
		m_currentIteration >= m_completionCriteriaParams.completionCriteriaMaxIterations)
	{
		return false;
	}

	if (m_completionCriteriaParams.isUnlimitedTime())
	{
		return true;
	}

	// check time limit completion criteria
	double secondsSpentRendering = TimeDiffChrono<std::chrono::seconds>(GetCurrentChronoTime(), m_renderStartTime);

	return secondsSpentRendering < m_completionCriteriaParams.getTotalSecondsCount();
}

bool FireRenderContext::ShouldShowShaderCacheWarningWindow() 
{
    if (isMetalOn())
    {
        // Metal does not cache shaders the way OCL does
        return false;
    }

	if (!HasRPRCachePathSet())
	{
		// Path to cache not set => Core will not be caching shaders
		return false;
	}

	if (m_currentIteration == 0)
    {
		return !areShadersCached();
	}
	return false;
}

void FireRenderContext::updateProgress()
{
	double secondsSpentRendering = TimeDiffChrono<std::chrono::seconds>(GetCurrentChronoTime(), m_renderStartTime);

	m_secondsSpentOnLastRender = secondsSpentRendering;

	if (m_completionCriteriaParams.isUnlimited())
	{
		m_progress = 0;
	}
	else if (!m_completionCriteriaParams.isUnlimitedIterations())
	{
		double iterationState = m_currentIteration / (double)m_completionCriteriaParams.completionCriteriaMaxIterations;
		m_progress = static_cast<int>(ceil(iterationState * 100));
	}
	else
	{
		double timeState = secondsSpentRendering / m_completionCriteriaParams.getTotalSecondsCount();
		m_progress = static_cast<int>(ceil(timeState * 100));
	}
}

int FireRenderContext::getProgress()
{
	return std::min(m_progress, 100);
}

void FireRenderContext::setProgress(int percents)
{
	m_progress = std::min(percents, 100);
}



void FireRenderContext::doOutputFromComposites(const ReadFrameBufferRequestParams& params, size_t dataSize, const frw::FrameBuffer& frameBufferOut)
{
	// Find the number of pixels in the frame buffer.
	int pixelCount = params.PixelCount();
	// Check that the reported frame buffer size
	// in bytes matches the required dimensions.
	assert(dataSize == (sizeof(RV_PIXEL) * pixelCount));

	// A temporary pixel buffer is required if the region is less
	// than the full width and height, or the image should be flipped.
	bool useTempData = params.UseTempData();
	if (useTempData)
		m_tempData.resize(pixelCount);

	RV_PIXEL* data = useTempData ? m_tempData.get() : params.pixels;
	rpr_int frstatus = rprFrameBufferGetInfo(frameBufferOut.Handle(), RPR_FRAMEBUFFER_DATA, dataSize, &data[0], nullptr);
	checkStatus(frstatus);

	if (useTempData)
	{
		copyPixels(params.pixels, data, params.width, params.height, params.region);
	}
}

void ShadowCatcherOutDbg(
	unsigned int width,
	unsigned int height, 
	frw::Context& context,
	const RprComposite& compositeToSave,
	std::string name
	)
{
#define SHADOWCATCHERDEBUG
#ifdef SHADOWCATCHERDEBUG
	rpr_framebuffer frameBufferOutDbg = 0;
	rpr_framebuffer_format fmtOutDbg = { 4, RPR_COMPONENT_TYPE_FLOAT32 };
	rpr_framebuffer_desc descOutDbg;
	descOutDbg.fb_width = width;
	descOutDbg.fb_height = height;

	rpr_int frstatus = rprContextCreateFrameBuffer(context.Handle(), fmtOutDbg, &descOutDbg, &frameBufferOutDbg);
	checkStatus(frstatus);
	frstatus = rprCompositeCompute(compositeToSave, frameBufferOutDbg);
	checkStatus(frstatus);

	std::string outputName ("C:/temp/dbg/"); 
	outputName += name;
	outputName += ".png";
	frstatus = rprFrameBufferSaveToFile(frameBufferOutDbg, outputName.c_str());
#endif
}

size_t GetDataSize(rpr_framebuffer frameBuffer)
{
	// Get data from the RPR frame buffer
	size_t dataSize;
	rpr_int frstatus = rprFrameBufferGetInfo(frameBuffer, RPR_FRAMEBUFFER_DATA, 0, nullptr, &dataSize);
	checkStatus(frstatus);
	return dataSize;
}

frw::FrameBuffer GetOutFrameBuffer(const FireRenderContext::ReadFrameBufferRequestParams& params, frw::Context& context)
{
	rpr_framebuffer_format fmtOut = { 4, RPR_COMPONENT_TYPE_FLOAT32 };
	frw::FrameBuffer frameBufferOut(context, params.width, params.height, fmtOut);
	return frameBufferOut;
}

// -----------------------------------------------------------------------------
void FireRenderContext::rifShadowCatcherOutput(const ReadFrameBufferRequestParams& params)
{
	const bool forceCPUContext = true;

	const rpr_framebuffer colorFrameBuffer = frameBufferAOV_Resolved(RPR_AOV_COLOR);
	const rpr_framebuffer mattePassFrameBuffer = frameBufferAOV_Resolved(RPR_AOV_MATTE_PASS);
	const rpr_framebuffer shadowCatcherFrameBuffer = frameBufferAOV_Resolved(RPR_AOV_SHADOW_CATCHER);

	try
	{
		MString path;
		MStatus s = MGlobal::executeCommand("getModulePath -moduleName RadeonProRender", path);
		MString mlModelsFolder = path + "/data/models";
		std::shared_ptr<ImageFilter> shadowCatcherFilter = std::shared_ptr<ImageFilter>(new ImageFilter(context(), m_width, m_height, mlModelsFolder.asChar(), forceCPUContext));
		shadowCatcherFilter->CreateFilter(RifFilterType::ShadowCatcher);
		shadowCatcherFilter->AddInput(RifColor, colorFrameBuffer, 0.1f);
		shadowCatcherFilter->AddInput(RifMattePass, mattePassFrameBuffer, 0.1f);
		shadowCatcherFilter->AddInput(RifShadowCatcher, shadowCatcherFrameBuffer, 0.1f);

		RifParam p;

		p = { RifParamType::RifOther, (rif_float)params.shadowColor[0] };
		shadowCatcherFilter->AddParam("shadowColor[0]", p);

		p = { RifParamType::RifOther, (rif_float)params.shadowColor[1] };
		shadowCatcherFilter->AddParam("shadowColor[1]", p);

		p = { RifParamType::RifOther, (rif_float)params.shadowColor[2] };
		shadowCatcherFilter->AddParam("shadowColor[2]", p);

		p = { RifParamType::RifOther, (rif_float)params.shadowWeight };
		shadowCatcherFilter->AddParam("shadowWeight", p);

		p = { RifParamType::RifOther, (rif_float)params.shadowTransp };
		shadowCatcherFilter->AddParam("shadowTransp", p);

		shadowCatcherFilter->AttachFilter();

		shadowCatcherFilter->Run();
		std::vector<float> vecData = shadowCatcherFilter->GetData();
		RV_PIXEL* data = (RV_PIXEL*)&vecData[0];
		copyPixels(params.pixels, data, params.width, params.height, params.region);
	}
	catch (std::exception& e)
	{
		ErrorPrint(e.what());
	}
}

void FireRenderContext::rifReflectionCatcherOutput(const ReadFrameBufferRequestParams& params)
{
	bool forceCPUContext = true;

	const rpr_framebuffer colorFrameBuffer = frameBufferAOV_Resolved(RPR_AOV_COLOR);
	const rpr_framebuffer opacityFrameBuffer = frameBufferAOV_Resolved(RPR_AOV_OPACITY);
	const rpr_framebuffer reflectionCatcherFrameBuffer = frameBufferAOV_Resolved(RPR_AOV_REFLECTION_CATCHER);
	const rpr_framebuffer backgroundFrameBuffer = frameBufferAOV_Resolved(RPR_AOV_BACKGROUND);
	
	try
	{
		MString path;
		MStatus s = MGlobal::executeCommand("getModulePath -moduleName RadeonProRender", path);
		MString mlModelsFolder = path + "/data/models";
		std::shared_ptr<ImageFilter> catcherFilter = std::shared_ptr<ImageFilter>(new ImageFilter(context(), m_width, m_height, mlModelsFolder.asChar(), forceCPUContext));
		catcherFilter->CreateFilter(RifFilterType::ReflectionCatcher);
		catcherFilter->AddInput(RifColor, colorFrameBuffer, 0.1f);
		catcherFilter->AddInput(RifOpacity, opacityFrameBuffer, 0.1f);
		catcherFilter->AddInput(RifReflectionCatcher, reflectionCatcherFrameBuffer, 0.1f);
		catcherFilter->AddInput(RifBackground, backgroundFrameBuffer, 0.1f);

		RifParam p = { RifParamType::RifOther, (rif_int)iblDisplay };
		catcherFilter->AddParam("iblDisplay", p);
		catcherFilter->AttachFilter();

		catcherFilter->Run();
		std::vector<float> vecData = catcherFilter->GetData();
		RV_PIXEL* data = (RV_PIXEL*)&vecData[0];
		copyPixels(params.pixels, data, params.width, params.height, params.region);
	}
	catch (std::exception& e)
	{
		ErrorPrint(e.what());
	}
}

void FireRenderContext::rifReflectionShadowCatcherOutput(const ReadFrameBufferRequestParams& params)
{
	bool forceCPUContext = true;
	
	const rpr_framebuffer colorFrameBuffer = frameBufferAOV_Resolved(RPR_AOV_COLOR);
	const rpr_framebuffer opacityFrameBuffer = frameBufferAOV_Resolved(RPR_AOV_OPACITY);
	const rpr_framebuffer shadowCatcherFrameBuffer = frameBufferAOV_Resolved(RPR_AOV_SHADOW_CATCHER);
	const rpr_framebuffer reflectionCatcherFrameBuffer = frameBufferAOV_Resolved(RPR_AOV_REFLECTION_CATCHER);
	const rpr_framebuffer backgroundFrameBuffer = frameBufferAOV_Resolved(RPR_AOV_BACKGROUND);
	const rpr_framebuffer mattePassFrameBuffer = frameBufferAOV_Resolved(RPR_AOV_MATTE_PASS);

	try
	{
		MString path;
		MStatus s = MGlobal::executeCommand("getModulePath -moduleName RadeonProRender", path);
		MString mlModelsFolder = path + "/data/models";
		std::shared_ptr<ImageFilter> catcherFilter = std::shared_ptr<ImageFilter>(new ImageFilter(context(), m_width, m_height, mlModelsFolder.asChar(), forceCPUContext));
		catcherFilter->CreateFilter(RifFilterType::ShadowReflectionCatcher);
		catcherFilter->AddInput(RifColor, colorFrameBuffer, 0.1f);
		catcherFilter->AddInput(RifOpacity, opacityFrameBuffer, 0.1f);
		catcherFilter->AddInput(RifShadowCatcher, shadowCatcherFrameBuffer, 0.1f);
		catcherFilter->AddInput(RifReflectionCatcher, reflectionCatcherFrameBuffer, 0.1f);
		catcherFilter->AddInput(RifBackground, backgroundFrameBuffer, 0.1f);
		catcherFilter->AddInput(RifMattePass, mattePassFrameBuffer, 0.1f);

		RifParam p;

		p = { RifParamType::RifOther, (rif_float)params.shadowColor[0] };
		catcherFilter->AddParam("shadowColor[0]", p);

		p = { RifParamType::RifOther, (rif_float)params.shadowColor[1] };
		catcherFilter->AddParam("shadowColor[1]", p);

		p = { RifParamType::RifOther, (rif_float)params.shadowColor[2] };
		catcherFilter->AddParam("shadowColor[2]", p);

		p = { RifParamType::RifOther, (rif_float)params.shadowWeight };
		catcherFilter->AddParam("shadowWeight", p);

		p = { RifParamType::RifOther, (rif_float)params.shadowTransp };
		catcherFilter->AddParam("shadowTransp", p);

		p = { RifParamType::RifOther, (rif_int)iblDisplay };
		catcherFilter->AddParam("iblDisplay", p);

		catcherFilter->AttachFilter();

		catcherFilter->Run();
		std::vector<float> vecData = catcherFilter->GetData();
		RV_PIXEL* data = (RV_PIXEL*)&vecData[0];
		copyPixels(params.pixels, data, params.width, params.height, params.region);
	}
	catch (std::exception& e)
	{
		ErrorPrint(e.what());
	}
}

RenderType FireRenderContext::GetRenderType() const
{
	return m_RenderType;
}

void FireRenderContext::SetRenderType(RenderType renderType)
{
	m_RenderType = renderType;

	if ((m_RenderType == RenderType::ViewportRender) ||
		(m_RenderType == RenderType::IPR))
	{
		m_interactive = true;
	}
}

bool FireRenderContext::IsTonemappingEnabled(void) const
{
	return (m_globals.toneMappingType != 0) && (!m_globals.contourIsEnabled);
}

bool FireRenderContext::IsDenoiserEnabled(void) const 
{
	if (!IsDenoiserSupported())
	{
		return false;
	}

	bool viewportRendering = GetRenderType() == RenderType::ViewportRender;
	return m_globals.denoiserSettings.enabled && !viewportRendering || m_globals.denoiserSettings.viewportDenoiseUpscaleEnabled && viewportRendering;
}

bool FireRenderContext::ShouldResizeTexture(unsigned int& max_width, unsigned int& max_height) const
{
	if (GetRenderType() == RenderType::Thumbnail)
	{
		max_width = FireRenderMaterialSwatchRender::MaterialSwatchPreviewTextureSize;
		max_height = FireRenderMaterialSwatchRender::MaterialSwatchPreviewTextureSize;
		return true;
	}

	return false;
}

frw::Shader FireRenderContext::GetShader(MObject ob, MObject shadingEngine, const FireRenderMeshCommon* pMesh, bool forceUpdate)
{ 
	scope.SetContextInfo(this);

	MFnDependencyNode node(ob);

	frw::Shader shader = scope.GetShader(ob, pMesh, forceUpdate);

	shader.SetName(node.name().asChar());

	if (!shadingEngine.isNull())
	{
		MFnDependencyNode sgDependecyNode(shadingEngine);

		MPlug materialIdPlug = sgDependecyNode.findPlug("rmi", false);

		if (IsMaterialNodeIDSupported() && !materialIdPlug.isNull())
		{
			shader.SetMaterialId(materialIdPlug.asInt());
		}
	}

	return shader;
}

void FireRenderContext::enableAOV(int aov, bool flag)
{
	if (IsAOVSupported(aov))
	{
		aovEnabled[aov] = flag;
	}
}

bool FireRenderContext::isAOVEnabled(int aov) const 
{
	return aovEnabled[aov];
}

frw::Shader FireRenderContext::GetDefaultColorShader(frw::Value color)
{
	frw::DiffuseShader shader(GetMaterialSystem());
	shader.SetColor(color);

	return shader;
}

void FireRenderContext::ForEachFramebuffer(
	std::function<void(int aovId)> actionFunc,
	std::function<bool(int aovId)> filter
)
{
	for (int id = RPR_AOV_COLOR; id < RPR_AOV_MAX; ++id)
	{
		if (!aovEnabled[id])
			continue;

		if (!filter(id))
			continue;

		actionFunc(id);
	}
}

std::vector<float> FireRenderContext::DenoiseAndUpscaleForViewport()
{
	bool useRAMBuffer = true;
	RenderRegion region = RenderRegion(m_width, m_height);

	ReadFrameBufferRequestParams params(region);
	params.width = m_width;
	params.height = m_height;
	params.mergeOpacity = false;
	params.mergeShadowCatcher = true;
	params.shadowColor = m_shadowColor;
	params.shadowTransp = m_shadowTransparency;
	params.shadowWeight = m_shadowWeight;

	// read frame buffers
	if (useRAMBuffer)
	{
		ReadDenoiserFrameBuffersIntoRAM(params);
	}

	std::lock_guard<std::mutex> lock(m_rifLock);
	bool isDenoiserInitialized = setupDenoiserForViewport(); // will read data from outBuffers if useRAMBuffer == true
	assert(isDenoiserInitialized);
	if (!isDenoiserInitialized || !IsDenoiserCreated())
		return std::vector<float>();

	std::vector<float> vecData;
	bool denoiseResult = false;
	vecData = GetDenoisedData(denoiseResult);
	assert(denoiseResult);

	// save denoiser result in RAM buffer
	RV_PIXEL* data = (RV_PIXEL*)vecData.data();

#ifdef DUMP_DENOISE_VP_SOURCE
	PixelBuffer tempPixelBuffer1;
	tempPixelBuffer1.resize(m_width, m_height);
	tempPixelBuffer1.overwrite(data, region, params.height, params.width);
	const std::string pathToFile = "C://debug//fb//";
	tempPixelBuffer1.debugDump(params.height, params.width, "just_denoised", pathToFile);
#endif

	if (useRAMBuffer)
	{
		setupUpscalerForViewport(data);

		m_upscalerFilter->Run();
		vecData = m_upscalerFilter->GetData();
	}

#ifdef DUMP_DENOISE_VP_SOURCE
	PixelBuffer tempPixelBuffer;
	tempPixelBuffer.resize(m_width, m_height);
	tempPixelBuffer.overwrite((RV_PIXEL*)vecData.data(), region, params.height, params.width);
	tempPixelBuffer.debugDump(params.height, params.width, "denoised_and_upscaled", pathToFile);
#endif

	return vecData;
}

bool FireRenderContext::TonemapIntoRAM()
{
	// prepare color RAM buffer
	auto it = m_pixelBuffers.find(RPR_AOV_COLOR);
	assert(it == m_pixelBuffers.end()); // pixel buffers should be empty at this point
	auto ret = m_pixelBuffers.insert(std::pair<unsigned int, PixelBuffer>(RPR_AOV_COLOR, PixelBuffer()));
	ret.first->second.resize(m_width, m_height);

	// setup params
	RenderRegion tempRegion;
	if (useRegion())
	{
		tempRegion = m_region;
	}
	else
	{
		tempRegion = RenderRegion(m_width, m_height);
	}
	ReadFrameBufferRequestParams params(tempRegion);
	params.pixels = m_pixelBuffers[RPR_AOV_COLOR].get();
	params.aov = RPR_AOV_COLOR;
	params.width = m_width;
	params.height = m_height;
	params.mergeOpacity = false;
	params.mergeShadowCatcher = true;
	params.shadowColor = m_shadowColor;
	params.shadowTransp = m_shadowTransparency;
	params.shadowWeight = m_shadowWeight;

	// read frame buffer
	readFrameBuffer(params);

	// create and run tonemap filters
	std::lock_guard<std::mutex> lock(m_rifLock);
	bool isTonemapperInitialized = TryCreateTonemapImageFilters(); 

	if (!isTonemapperInitialized)
		return false;

	// run tonemaper on cached data
	std::vector<float> vecData;
	bool tonemapResult = false;
	vecData = GetTonemappedData(tonemapResult);
	assert(tonemapResult);

	// save result in RAM buffer
	RV_PIXEL* data = (RV_PIXEL*)vecData.data();
	m_pixelBuffers[RPR_AOV_COLOR].overwrite(data, tempRegion, params.height, params.width, RPR_AOV_COLOR);

	return true;
}

std::vector<float> FireRenderContext::DenoiseIntoRAM()
{
	bool shouldDenoise = IsDenoiserEnabled() &&
		((m_RenderType == RenderType::ProductionRender) || (m_RenderType == RenderType::IPR));

	if (!shouldDenoise)
		return std::vector<float>();

	bool useRAMBuffer = ShouldForceRAMDenoiser();

	// setup params
	RenderRegion tempRegion;
	if (useRegion())
	{
		tempRegion = m_region;
	}
	else
	{
		tempRegion = RenderRegion(m_width, m_height);
	}

	ReadFrameBufferRequestParams params(tempRegion);
	params.width = m_width;
	params.height = m_height;
	params.mergeOpacity = false;
	params.mergeShadowCatcher = true;
	params.shadowColor = m_shadowColor;
	params.shadowTransp = m_shadowTransparency;
	params.shadowWeight = m_shadowWeight;
	
	// read frame buffers
	if (useRAMBuffer)
	{
		ReadDenoiserFrameBuffersIntoRAM(params);
	}

	std::lock_guard<std::mutex> lock(m_rifLock);
	bool isDenoiserInitialized = TryCreateDenoiserImageFilters(useRAMBuffer); // will read data from outBuffers if useRAMBuffer == true
	assert(isDenoiserInitialized);
	if (!isDenoiserInitialized || !IsDenoiserCreated())
		return std::vector<float>();

	// run denoiser on cached data
	std::vector<float> vecData;
	bool denoiseResult = false;
	vecData = GetDenoisedData(denoiseResult);
	assert(denoiseResult);

	// save denoiser result in RAM buffer
	RV_PIXEL* data = (RV_PIXEL*)vecData.data();
	if (useRAMBuffer)
	{
		m_pixelBuffers[RPR_AOV_COLOR].overwrite(data, tempRegion, params.height, params.width, RPR_AOV_COLOR);
		params.pixels = m_pixelBuffers[RPR_AOV_COLOR].get();
		// run merge opacity
		params.aov = RPR_AOV_COLOR;
		params.mergeOpacity = camera().GetAlphaMask() && isAOVEnabled(RPR_AOV_OPACITY);
		ReadOpacityAOV(params);

		// combine (Opacity to Alpha)
		if (params.mergeOpacity)
		{
			CombineOpacity(RPR_AOV_COLOR, data, tempRegion.getArea());
		}
	}

	return vecData;
}

void FireRenderContext::ProcessMergeOpactityFromRAM(RV_PIXEL* data, int bufferWidth, int bufferHeight)
{
	if (!camera().GetAlphaMask() || !isAOVEnabled(RPR_AOV_OPACITY))
		return;

	auto it = m_pixelBuffers.find(RPR_AOV_OPACITY);
	if (it == m_pixelBuffers.end())
		return;

	size_t dataSize = (sizeof(RV_PIXEL) * bufferWidth * bufferHeight);

	m_opacityData.resize(bufferWidth * bufferHeight);

	RenderRegion tempRegion(bufferWidth, bufferHeight);
	copyPixels(m_opacityData.get(), it->second.get(), bufferWidth, bufferHeight, tempRegion);

	// combine (Opacity to Alpha)
	CombineOpacity(RPR_AOV_COLOR, data, tempRegion.getArea());
}

void FireRenderContext::ProcessTonemap(
	FireRenderAOV& renderViewAOV,
	FireRenderAOV& colorAOV,
	unsigned int width,
	unsigned int height,
	const RenderRegion& region,
	std::function<void(RV_PIXEL* pData)> callbackFunc)
{
	// run tonemapper
	bool success = TonemapIntoRAM();
	if (!success)
		return;

	// output tonemapper result
	RV_PIXEL* data = nullptr;
	auto it = PixelBuffers().find(RPR_AOV_COLOR);
	bool hasAov = it != PixelBuffers().end();
	if (!hasAov)
		return;

	data = (RV_PIXEL*)it->second.data();

	// save result
	bool isOutputAOVColor = renderViewAOV.id == RPR_AOV_COLOR;
	FireRenderAOV& outAOV = isOutputAOVColor ? renderViewAOV : colorAOV;
	outAOV.pixels.overwrite(data, region, height, width, RPR_AOV_COLOR);

	// callback
	if (isOutputAOVColor)
	{
		callbackFunc(data);
	}
}

void FireRenderContext::ProcessDenoise(
	FireRenderAOV& renderViewAOV, 
	FireRenderAOV& colorAOV,
	unsigned int width, 
	unsigned int height, 
	const RenderRegion& region, 
	std::function<void(RV_PIXEL* pData)> callbackFunc)
{
	// run denoiser
	std::vector<float> vecData = DenoiseIntoRAM();

	// output denoiser result
	RV_PIXEL* data = nullptr;
	auto it = PixelBuffers().find(RPR_AOV_COLOR);
	bool hasAov = it != PixelBuffers().end();
	if (hasAov) // different flow for rpr2 and rpr1
	{
		data = (RV_PIXEL*)it->second.data();
	}
	else
	{
		data = (RV_PIXEL*)vecData.data();
	}

	// apply render stamp
	if (!useRegion())
	{
		FireMaya::RenderStamp renderStamp;
		MString stampStr(renderViewAOV.renderStamp);
		renderStamp.AddRenderStamp(*this, data, width, height, stampStr.asChar());
	}

	// save result
	bool isOutputAOVColor = renderViewAOV.id == RPR_AOV_COLOR;
	FireRenderAOV& outAOV = isOutputAOVColor ? renderViewAOV : colorAOV;
	outAOV.pixels.overwrite(data, region, height, width, RPR_AOV_COLOR);

	// callback
	if (isOutputAOVColor)
	{
		callbackFunc(data);
	}
}

void FireRenderContext::ResetRAMBuffers()
{
	m_pixelBuffers.clear();
}

void FireRenderContext::ReadDenoiserFrameBuffersIntoRAM(ReadFrameBufferRequestParams& params)
{
	ForEachFramebuffer([&](int aovId)
	{
		auto it = m_pixelBuffers.find(aovId);
		if (it == m_pixelBuffers.end())
		{
			auto ret = m_pixelBuffers.insert(std::pair<unsigned int, PixelBuffer>(aovId, PixelBuffer()));
			ret.first->second.resize(m_width, m_height);
		}

		// setup params
		params.pixels = m_pixelBuffers[aovId].get();
		params.aov = aovId;

		// process frame buffer
		readFrameBuffer(params);

		// debug
#ifdef DENOISE_RAM_DBG
		m_pixelBuffers[aovId].debugDump(
			params.region.getHeight(), 
			params.region.getWidth(), 
			FireRenderAOV::GetAOVName(aovId), 
			"C:\\temp\\dbg\\");
#endif
	},

	[](int aovId)->bool
	{
		if (aovId == RPR_AOV_COLOR)
			return true;

		// is denoiser AOV
		return (std::find(g_denoiserAovs.begin(), g_denoiserAovs.end(), aovId) != g_denoiserAovs.end());
	});
}

frw::Light FireRenderContext::GetLightSceneObjectFromMObject(const MObject& node)
{
	const std::string& uuid = getNodeUUid(node);

	const auto it = m_sceneObjects.find(uuid);

	if (it == m_sceneObjects.end())
	{
		MGlobal::displayError("Unable to find linked light!");
		return frw::Light();
	}

	if (GetScope().GetCurrentlyParsedMesh() == nullptr)
	{
		MGlobal::displayError("'GetRprLightFromNode' method called outside of mesh parsing scope");
		return frw::Light();
	}

	FireRenderObject* lightObjectPointer = it->second.get();

	FireRenderLight* fireRenderLight = dynamic_cast<FireRenderLight*>(lightObjectPointer);
	FireRenderEnvLight* envLight = dynamic_cast<FireRenderEnvLight*>(lightObjectPointer);

	if (fireRenderLight && !fireRenderLight->GetFrLight().isAreaLight)
	{
		return fireRenderLight->GetFrLight().light;
	}
	else if (envLight)
	{
		return envLight->getLight();
	}

	MGlobal::displayWarning("light with uuid " + MString(lightObjectPointer->uuid().c_str()) + " does not support linking");
	return frw::Light();
}

bool FireRenderContext::HasRPRCachePathSet() const
{
	if (!scope.IsValid() || !scope.Context())
		return false; // context not created yet

	rpr_context pContext = scope.Context().Handle();
	assert(pContext != nullptr);
	if (!pContext)
		return false;

	size_t length;
	rpr_status rprStatus = rprContextGetInfo(pContext, RPR_CONTEXT_CACHE_PATH, sizeof(size_t), nullptr, &length);
	assert(RPR_SUCCESS == rprStatus);
	if (RPR_SUCCESS != rprStatus)
		return false;

	std::string path;
	path.resize(length);
	rprStatus = rprContextGetInfo(pContext, RPR_CONTEXT_CACHE_PATH, path.size(), path.data(), nullptr);
	assert(RPR_SUCCESS == rprStatus);
	if (RPR_SUCCESS != rprStatus)
		return false;

	if (path.empty())
		return false;

	if ((path.length()) == 1 && (path[0] == '\0'))
		return false;

	return true;
}

