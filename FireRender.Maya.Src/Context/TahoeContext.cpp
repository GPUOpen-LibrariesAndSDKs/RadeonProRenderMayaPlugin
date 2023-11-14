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
#include "TahoeContext.h"
#include "RadeonProRender_MaterialX.h"

#include "maya/MColorManagementUtilities.h"
#include "maya/MFileObject.h"

#ifndef _WIN32
#include <unistd.h>
#endif

rpr_int NorthStarContext::m_gTahoePluginID = INCORRECT_PLUGIN_ID;

NorthStarContext::NorthStarContext() :
	m_PreviewMode(true)
{

}

rpr_int NorthStarContext::GetPluginID()
{
	if (m_gTahoePluginID == INCORRECT_PLUGIN_ID)
	{
		std::string libName = "Northstar64";

#ifdef OSMac_
		libName = "lib" + libName + ".dylib";
		std::string pathToTahoeDll = "/Users/Shared/RadeonProRender/Maya/lib/" + libName;
		if (0 != access(pathToTahoeDll.c_str(), F_OK))
		{
			pathToTahoeDll = "/Users/Shared/RadeonProRender/lib/" + libName;
		}
		m_gTahoePluginID = rprRegisterPlugin(pathToTahoeDll.c_str());
#elif __linux__
		m_gTahoePluginID = rprRegisterPlugin("libTahoe64.so");
#else
		libName += ".dll";
		m_gTahoePluginID = rprRegisterPlugin("Northstar64.dll");
#endif
	}

	return m_gTahoePluginID;
}

rpr_int NorthStarContext::CreateContextInternal(rpr_creation_flags createFlags, rpr_context* pContext)
{
	rpr_int pluginID = GetPluginID();

	if (pluginID == INCORRECT_PLUGIN_ID)
	{
		MGlobal::displayError("Unable to register Radeon ProRender plug-in.");
		return RPR_ERROR_INVALID_PARAMETER;
	}

	rpr_int plugins[] = { pluginID };
	size_t pluginCount = 1;

	auto cachePath = getShaderCachePath();

	// setup CPU thread count
	std::vector<rpr_context_properties> ctxProperties;
	ctxProperties.push_back((rpr_context_properties)RPR_CONTEXT_SAMPLER_TYPE);
	ctxProperties.push_back((rpr_context_properties)RPR_CONTEXT_SAMPLER_TYPE_CMJ);

	int threadCountToOverride = getThreadCountToOverride();
	if ((createFlags & RPR_CREATION_FLAGS_ENABLE_CPU) && threadCountToOverride > 0)
	{
		ctxProperties.push_back((rpr_context_properties)RPR_CONTEXT_CPU_THREAD_LIMIT);
		ctxProperties.push_back((void*)(size_t)threadCountToOverride);
	}

	ctxProperties.push_back((rpr_context_properties)RPR_CONTEXT_PRECOMPILED_BINARY_PATH);
	std::string hipbinPath = GetPathToHipbinFolder();
	ctxProperties.push_back((rpr_context_properties)hipbinPath.c_str());

	ctxProperties.push_back((rpr_context_properties)0);

	int res;
#ifdef RPR_VERSION_MAJOR_MINOR_REVISION
	if (!Globals().useOpenCLContext)
	{
		res = rprCreateContext(RPR_VERSION_MAJOR_MINOR_REVISION, plugins, pluginCount, createFlags, ctxProperties.data(), nullptr, pContext);
	}
	else
	{
		res = rprCreateContext(RPR_VERSION_MAJOR_MINOR_REVISION, plugins, pluginCount, createFlags, ctxProperties.data(), cachePath.asUTF8(), pContext);
	}
#else
	res = rprCreateContext(RPR_API_VERSION, plugins, pluginCount, createFlags, ctxProperties.data(), cachePath.asUTF8(), pContext);
#endif
	return res;
}

void NorthStarContext::setupContextAirVolume(const FireRenderGlobalsData& fireRenderGlobalsData)
{
	frw::Context context = GetContext();
	rpr_int frstatus = RPR_SUCCESS;

	if (!fireRenderGlobalsData.airVolumeSettings.airVolumeEnabled)
	{
		context.SetParameter(RPR_CONTEXT_ATMOSPHERE_VOLUME_DENSITY, 0.0f);
	}
	else
	{
		context.SetParameter(RPR_CONTEXT_ATMOSPHERE_VOLUME_DENSITY, fireRenderGlobalsData.airVolumeSettings.airVolumeDensity);

		context.SetParameter(RPR_CONTEXT_ATMOSPHERE_VOLUME_COLOR,
			fireRenderGlobalsData.airVolumeSettings.airVolumeColor.r,
			fireRenderGlobalsData.airVolumeSettings.airVolumeColor.g,
			fireRenderGlobalsData.airVolumeSettings.airVolumeColor.b,
			0.0f);

		context.SetParameter(RPR_CONTEXT_ATMOSPHERE_VOLUME_RADIANCE_CLAMP, fireRenderGlobalsData.airVolumeSettings.airVolumeDensity);
	}

	if (!fireRenderGlobalsData.airVolumeSettings.fogEnabled)
	{
		context.SetParameter(RPR_CONTEXT_FOG_DISTANCE, -1.0f);
	}
	else
	{
		context.SetParameter(RPR_CONTEXT_FOG_COLOR,
			fireRenderGlobalsData.airVolumeSettings.fogColor.r,
			fireRenderGlobalsData.airVolumeSettings.fogColor.g,
			fireRenderGlobalsData.airVolumeSettings.fogColor.b,
			0.0f);

		context.SetParameter(RPR_CONTEXT_FOG_DISTANCE, fireRenderGlobalsData.airVolumeSettings.fogDistance);

		context.SetParameter(RPR_CONTEXT_FOG_HEIGHT, fireRenderGlobalsData.airVolumeSettings.fogHeight);
	}
}

void NorthStarContext::setupContextCryptomatteSettings(const FireRenderGlobalsData& fireRenderGlobalsData)
{
	frw::Context context = GetContext();
	rpr_context frcontext = context.Handle();

	rpr_int frstatus = RPR_SUCCESS;

	frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_CRYPTOMATTE_EXTENDED, fireRenderGlobalsData.cryptomatteExtendedMode);
	checkStatus(frstatus);

	frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_CRYPTOMATTE_SPLIT_INDIRECT, fireRenderGlobalsData.cryptomatteSplitIndirect);
	checkStatus(frstatus);
}

void NorthStarContext::setupContextContourMode(const FireRenderGlobalsData& fireRenderGlobalsData, int createFlags, bool disableWhiteBalance /*= false*/)
{
	frw::Context context = GetContext();
	rpr_context frcontext = context.Handle();

	rpr_int frstatus = RPR_SUCCESS;
	
	// contour must be set before scene creation
	bool isContourModeOn = fireRenderGlobalsData.contourIsEnabled && !(createFlags & RPR_CREATION_FLAGS_ENABLE_CPU);

	if (!isContourModeOn)
		return;

	frstatus = rprContextSetParameterByKeyString(frcontext, RPR_CONTEXT_GPUINTEGRATOR, "gpucontour");
	checkStatus(frstatus);

	frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_CONTOUR_USE_OBJECTID, fireRenderGlobalsData.contourUseObjectID);
	checkStatus(frstatus);

	frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_CONTOUR_USE_MATERIALID, fireRenderGlobalsData.contourUseMaterialID);
	checkStatus(frstatus);

	frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_CONTOUR_USE_NORMAL, fireRenderGlobalsData.contourUseShadingNormal);
	checkStatus(frstatus);

	frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_CONTOUR_USE_UV, fireRenderGlobalsData.contourUseUV);
	checkStatus(frstatus);

	frstatus = rprContextSetParameterByKey1f(frcontext, RPR_CONTEXT_CONTOUR_LINEWIDTH_OBJECTID, fireRenderGlobalsData.contourLineWidthObjectID);
	checkStatus(frstatus);

	frstatus = rprContextSetParameterByKey1f(frcontext, RPR_CONTEXT_CONTOUR_LINEWIDTH_MATERIALID, fireRenderGlobalsData.contourLineWidthMaterialID);
	checkStatus(frstatus);

	frstatus = rprContextSetParameterByKey1f(frcontext, RPR_CONTEXT_CONTOUR_LINEWIDTH_NORMAL, fireRenderGlobalsData.contourLineWidthShadingNormal);
	checkStatus(frstatus);

	frstatus = rprContextSetParameterByKey1f(frcontext, RPR_CONTEXT_CONTOUR_LINEWIDTH_UV, fireRenderGlobalsData.contourLineWidthUV);
	checkStatus(frstatus);

	frstatus = rprContextSetParameterByKey1f(frcontext, RPR_CONTEXT_CONTOUR_NORMAL_THRESHOLD, fireRenderGlobalsData.contourNormalThreshold);
	checkStatus(frstatus);

	frstatus = rprContextSetParameterByKey1f(frcontext, RPR_CONTEXT_CONTOUR_UV_THRESHOLD, fireRenderGlobalsData.contourUVThreshold);
	checkStatus(frstatus);

	frstatus = rprContextSetParameterByKey1f(frcontext, RPR_CONTEXT_CONTOUR_ANTIALIASING, fireRenderGlobalsData.contourAntialiasing);
	checkStatus(frstatus);

	frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_CONTOUR_DEBUG_ENABLED, fireRenderGlobalsData.contourIsDebugEnabled);
	checkStatus(frstatus);
}

void NorthStarContext::setupContextPostSceneCreation(const FireRenderGlobalsData& fireRenderGlobalsData, bool disableWhiteBalance)
{
	frw::Context context = GetContext();
	rpr_context frcontext = context.Handle();

	rpr_int frstatus = RPR_SUCCESS;

	frstatus = rprContextSetParameterByKey1f(frcontext, RPR_CONTEXT_PDF_THRESHOLD, 0.0000f);
	checkStatus(frstatus);

	if (GetRenderType() == RenderType::Thumbnail)
	{
		updateTonemapping(fireRenderGlobalsData, false);
		return;
	}

	frstatus = rprContextSetParameterByKey1f(frcontext, RPR_CONTEXT_RADIANCE_CLAMP, fireRenderGlobalsData.giClampIrradiance ? fireRenderGlobalsData.giClampIrradianceValue : FLT_MAX);
	checkStatus(frstatus);

	frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_TEXTURE_COMPRESSION, fireRenderGlobalsData.textureCompression);
	checkStatus(frstatus);

	frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_NORMALIZE_LIGHT_INTENSITY_ENABLED, fireRenderGlobalsData.useLegacyRPRToon ? 0 : 1);
	checkStatus(frstatus);

	frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_ADAPTIVE_SAMPLING_TILE_SIZE, fireRenderGlobalsData.adaptiveTileSize);
	checkStatus(frstatus);

	bool velocityAOVMotionBlur = !fireRenderGlobalsData.velocityAOVMotionBlur;
	frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_BEAUTY_MOTION_BLUR, velocityAOVMotionBlur);

	if (GetRenderType() == RenderType::ProductionRender) // production (final) rendering
	{
		frstatus = rprContextSetParameterByKey1f(frcontext, RPR_CONTEXT_ADAPTIVE_SAMPLING_THRESHOLD, fireRenderGlobalsData.adaptiveThreshold);
		checkStatus(frstatus);

		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_RENDER_MODE, fireRenderGlobalsData.renderMode);
		checkStatus(frstatus);

		if (fireRenderGlobalsData.contourIsEnabled)
		{
			setSamplesPerUpdate(1);
		}
		else
		{
			setSamplesPerUpdate(fireRenderGlobalsData.samplesPerUpdate);
		}

		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_MAX_RECURSION, fireRenderGlobalsData.maxRayDepth);
		checkStatus(frstatus);

		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_MAX_DEPTH_DIFFUSE, fireRenderGlobalsData.maxRayDepthDiffuse);
		checkStatus(frstatus);

		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_MAX_DEPTH_GLOSSY, fireRenderGlobalsData.maxRayDepthGlossy);
		checkStatus(frstatus);

		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_MAX_DEPTH_REFRACTION, fireRenderGlobalsData.maxRayDepthRefraction);
		checkStatus(frstatus);

		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_MAX_DEPTH_GLOSSY_REFRACTION, fireRenderGlobalsData.maxRayDepthGlossyRefraction);
		checkStatus(frstatus);

		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_MAX_DEPTH_SHADOW, fireRenderGlobalsData.maxRayDepthShadow);
		checkStatus(frstatus);

		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_ADAPTIVE_SAMPLING_MIN_SPP, fireRenderGlobalsData.completionCriteriaFinalRender.completionCriteriaMinIterations);
		checkStatus(frstatus);

		// if Deep Exr enabled
		if (fireRenderGlobalsData.aovs.IsAOVActive(RPR_AOV_DEEP_COLOR))
		{
			MDistance distance(fireRenderGlobalsData.deepEXRMergeZThreshold, MDistance::uiUnit());

			frstatus = rprContextSetParameterByKey1f(frcontext, RPR_CONTEXT_DEEP_SUBPIXEL_MERGE_Z_THRESHOLD, distance.asMeters());
			frstatus = rprContextSetParameterByKey1f(frcontext, RPR_CONTEXT_DEEP_GPU_ALLOCATION_LEVEL, 4);
			frstatus = rprContextSetParameterByKey1f(frcontext, RPR_CONTEXT_DEEP_COLOR_ENABLED, 1);
		}
	}
	else if (isInteractive())
	{
		setSamplesPerUpdate(1);
        
        SetIterationsPowerOf2Mode(true);

		frstatus = rprContextSetParameterByKey1f(frcontext, RPR_CONTEXT_ADAPTIVE_SAMPLING_THRESHOLD, fireRenderGlobalsData.adaptiveThresholdViewport);
		checkStatus(frstatus);

		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_RENDER_MODE, fireRenderGlobalsData.viewportRenderMode);
		checkStatus(frstatus);

		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_MAX_RECURSION, fireRenderGlobalsData.viewportMaxRayDepth);
		checkStatus(frstatus);

		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_MAX_DEPTH_DIFFUSE, fireRenderGlobalsData.viewportMaxDiffuseRayDepth);
		checkStatus(frstatus);

		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_MAX_DEPTH_GLOSSY, fireRenderGlobalsData.viewportMaxReflectionRayDepth);
		checkStatus(frstatus);

		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_MAX_DEPTH_REFRACTION, fireRenderGlobalsData.viewportMaxReflectionRayDepth);
		checkStatus(frstatus);

		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_MAX_DEPTH_GLOSSY_REFRACTION, fireRenderGlobalsData.viewportMaxReflectionRayDepth);
		checkStatus(frstatus);

		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_MAX_DEPTH_SHADOW, fireRenderGlobalsData.viewportMaxDiffuseRayDepth);
		checkStatus(frstatus);

		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_ADAPTIVE_SAMPLING_MIN_SPP, fireRenderGlobalsData.completionCriteriaViewport.completionCriteriaMinIterations);
		checkStatus(frstatus);
	}

	frstatus = rprContextSetParameterByKey1f(frcontext, RPR_CONTEXT_RAY_CAST_EPSILON, fireRenderGlobalsData.raycastEpsilon);
	checkStatus(frstatus);

	frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_IMAGE_FILTER_TYPE, fireRenderGlobalsData.filterType);
	checkStatus(frstatus);

	frstatus = rprContextSetParameterByKey1f(frcontext, RPR_CONTEXT_IMAGE_FILTER_RADIUS, fireRenderGlobalsData.filterSize);
	checkStatus(frstatus);

	frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_METAL_PERFORMANCE_SHADER, fireRenderGlobalsData.useMPS ? 1 : 0);
	checkStatus(frstatus);

	updateTonemapping(fireRenderGlobalsData, disableWhiteBalance);

	frstatus = rprContextSetParameterByKeyString(frcontext, RPR_CONTEXT_TEXTURE_CACHE_PATH, fireRenderGlobalsData.textureCachePath.asChar());
	checkStatus(frstatus);

	// set dependency for materialX standard surface (could be missing from *.mtlx files)
	frstatus = rprMaterialXAddDependencyMtlx(frcontext, "scripts/standard_surface.mtlx");
	checkStatus(frstatus);

	// SC and RC
	// we should disable built-in shadow catcher composite
	frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_SHADOW_CATCHER_BAKING, fireRenderGlobalsData.shadowCatcherEnabled ? 0 : 1);
	checkStatus(frstatus);
	// we should disable IBL visibility to correctly composite reflection catcher
	frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_IBL_DISPLAY, fireRenderGlobalsData.reflectionCatcherEnabled ? 0 : 1);
	checkStatus(frstatus);

	// OCIO
	const std::map<std::string, std::string>& eVars = EnvironmentVarsWrapper<char>::GetEnvVarsTable();
	auto envOCIOPath = eVars.find("OCIO");
	if (envOCIOPath != eVars.end())
	{
		MFileObject path;
		path.setRawFullName(envOCIOPath->second.c_str());
		MString setupCommand = MString("colorManagementPrefs -e -configFilePath \"") + path.resolvedFullName() + MString("\";");
		MGlobal::executeCommand(setupCommand);
		MGlobal::executeCommand(MString("colorManagementPrefs -e -cmEnabled 1;"));
		MGlobal::executeCommand(MString("colorManagementPrefs -e -cmConfigFileEnabled 1;"));
	}

	MStatus colorManagementStatus;
	int isColorManagementOn = 0;
	colorManagementStatus = MGlobal::executeCommand(MString("colorManagementPrefs -q -cmEnabled;"), isColorManagementOn);

	int isConfigFileEnable = 0;
	colorManagementStatus = MGlobal::executeCommand(MString("colorManagementPrefs -q -cmConfigFileEnabled;"), isConfigFileEnable);

	if ((isColorManagementOn > 0) && (isConfigFileEnable > 0))
	{
		MString configFilePath;
		colorManagementStatus = MGlobal::executeCommand(MString("colorManagementPrefs -q -cfp;"), configFilePath);

		MString renderingSpaceName;
		colorManagementStatus = MGlobal::executeCommand(MString("colorManagementPrefs -q -rsn;"), renderingSpaceName);

		frstatus = rprContextSetParameterByKeyString(frcontext, RPR_CONTEXT_OCIO_CONFIG_PATH, configFilePath.asChar());
		checkStatus(frstatus);

		frstatus = rprContextSetParameterByKeyString(frcontext, RPR_CONTEXT_OCIO_RENDERING_COLOR_SPACE, renderingSpaceName.asChar());
		checkStatus(frstatus);
	}
	else
	{
		frstatus = rprContextSetParameterByKeyString(frcontext, RPR_CONTEXT_OCIO_CONFIG_PATH, "");
		checkStatus(frstatus);

		frstatus = rprContextSetParameterByKeyString(frcontext, RPR_CONTEXT_OCIO_RENDERING_COLOR_SPACE, "");
		checkStatus(frstatus);
	}
}

void NorthStarContext::updateTonemapping(const FireRenderGlobalsData& fireRenderGlobalsData, bool disableWhiteBalance)
{
	frw::Context context = GetContext();
	rpr_context frcontext = context.Handle();

	rpr_int frstatus = RPR_SUCCESS;

	frstatus = rprContextSetParameterByKey1f(frcontext, RPR_CONTEXT_TEXTURE_GAMMA, fireRenderGlobalsData.textureGamma);
	checkStatus(frstatus);

	// Release existing effects
	m_tonemap.reset();

	if (white_balance)
	{
		context.Detach(white_balance);
		white_balance.Reset();
	}

	if (simple_tonemap)
	{
		context.Detach(simple_tonemap);
		simple_tonemap.Reset();
	}

	if (m_tonemap)
	{
		//context.Detach(m_tonemap);
		//m_tonemap.Reset();
	}

	if (m_normalization)
	{
		context.Detach(m_normalization);
		m_normalization.Reset();
	}
	if (gamma_correction)
	{
		context.Detach(gamma_correction);
		gamma_correction.Reset();
	}

	// At least one post effect is required for frame buffer resolve to
	// work, which is required for OpenGL interop. Frame buffer normalization
	// should be applied before other post effects.
	// Also gamma effect requires normalization when tonemapping is not used.
	m_normalization = frw::PostEffect(context, frw::PostEffectTypeNormalization);
	context.Attach(m_normalization);

	// Create new effects
	switch (fireRenderGlobalsData.toneMappingType)
	{
	case 0:      
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
		break;

	case 6:
		//frw::PostEffectTypeSimpleTonemap
		if (!simple_tonemap)
		{
			simple_tonemap = frw::PostEffect(context, frw::PostEffectTypeSimpleTonemap);
			simple_tonemap.SetParameter("tonemap", fireRenderGlobalsData.toneMappingSimpleTonemap);
			simple_tonemap.SetParameter("exposure", fireRenderGlobalsData.toneMappingSimpleExposure);
			simple_tonemap.SetParameter("contrast", fireRenderGlobalsData.toneMappingSimpleContrast);
			context.Attach(simple_tonemap);
		}
		break;

	default:
		break;
	}

	// This block is required for gamma functionality
	if (fireRenderGlobalsData.applyGammaToMayaViews)
	{
		gamma_correction = frw::PostEffect(context, frw::PostEffectTypeGammaCorrection);
		context.Attach(gamma_correction);
	}

	bool wbApplied = fireRenderGlobalsData.toneMappingWhiteBalanceEnabled && !disableWhiteBalance;
	if (wbApplied)
	{
		//frw::PostEffectTypeWhiteBalance
		if (!white_balance)
		{
			white_balance = frw::PostEffect(context, frw::PostEffectTypeWhiteBalance);
			context.Attach(white_balance);

			float temperature = fireRenderGlobalsData.toneMappingWhiteBalanceValue;
			white_balance.SetParameter("colorspace", RPR_COLOR_SPACE_SRGB); // check: Max uses Adobe SRGB here
			white_balance.SetParameter("colortemp", temperature);
		}
	}

	bool setDisplayGamma = fireRenderGlobalsData.applyGammaToMayaViews || simple_tonemap || m_tonemap || wbApplied;
	float displayGammaValue = setDisplayGamma ? fireRenderGlobalsData.displayGamma : 1.0f;
	frstatus = rprContextSetParameterByKey1f(frcontext, RPR_CONTEXT_DISPLAY_GAMMA, displayGammaValue);
	checkStatus(frstatus);
}

bool NorthStarContext::needResolve() const
{
	return bool(white_balance) || bool(simple_tonemap) || bool(m_tonemap) || bool(m_normalization) || bool(gamma_correction);
}

bool NorthStarContext::IsRenderQualitySupported(RenderQuality quality) const
{
	return quality == RenderQuality::RenderQualityFull;
}

bool NorthStarContext::IsDenoiserSupported() const
{
	return true;
}

bool NorthStarContext::ShouldForceRAMDenoiser() const
{
	return true;
}

bool NorthStarContext::IsDisplacementSupported() const
{
	return true;
}

bool NorthStarContext::IsHairSupported() const
{
	return true;
}

bool NorthStarContext::IsVolumeSupported() const
{
	return true;
}

bool NorthStarContext::IsNorthstarVolumeSupported() const
{
	return true;
}

bool NorthStarContext::IsAOVSupported(int aov) const 
{
	if (aov >= RPR_AOV_MAX)
	{
		return false;
	}

	return (aov != RPR_AOV_VIEW_SHADING_NORMAL) && (aov != RPR_AOV_COLOR_RIGHT);
}

bool NorthStarContext::IsPhysicalLightTypeSupported(PLType lightType) const
{
	return true;
}

bool NorthStarContext::MetalContextAvailable() const
{
    return true;
}

bool NorthStarContext::IsDeformationMotionBlurEnabled() const
{
	assert(NorthStarContext::IsGivenContextNorthStar(this));
	return true;
}

void NorthStarContext::SetRenderUpdateCallback(RenderUpdateCallback callback, void* data)
{
	GetScope().Context().SetUpdateCallback((void*)callback, data);
}

void NorthStarContext::SetSceneSyncFinCallback(RenderUpdateCallback callback, void* data)
{
	GetScope().Context().SetSceneSyncFinCallback((void*)callback, data);
}

void NorthStarContext::SetFirstIterationCallback(RenderUpdateCallback callback, void* data)
{
	GetScope().Context().SetFirstIterationCallback((void*)callback, data);
}

void NorthStarContext::SetRenderTimeCallback(RenderUpdateCallback callback, void* data)
{
	GetScope().Context().SetRenderTimeCallback((void*)callback, data);
}


bool NorthStarContext::IsGivenContextNorthStar(const FireRenderContext* pContext)
{
	const NorthStarContext* pNorthStarContext = dynamic_cast<const NorthStarContext*> (pContext);

	if (pNorthStarContext == nullptr)
	{
		return false;
	}

	return true;
}

void NorthStarContext::AbortRender()
{
	GetScope().Context().AbortRender();
}

void NorthStarContext::OnPreRender()
{
	RenderType renderType = GetRenderType();

	if ((renderType != RenderType::ViewportRender) &&
		(renderType != RenderType::IPR))
	{
		return;
	}

	const int previewModeLevel = 2;
	if (m_restartRender)
	{
		m_PreviewMode = true;
		SetPreviewMode(previewModeLevel);
	}

	if (m_currentFrame == 2 && m_PreviewMode)
	{
		SetPreviewMode(0);
		m_PreviewMode = false;
		m_restartRender = true;
	}
	else if (m_currentFrame < 2 && m_PreviewMode)
	{
		SetPreviewMode(previewModeLevel);
	}

}

int NorthStarContext::GetAOVMaxValue() const
{
	assert(NorthStarContext::IsGivenContextNorthStar(this));

	return RPR_AOV_MAX;
}

