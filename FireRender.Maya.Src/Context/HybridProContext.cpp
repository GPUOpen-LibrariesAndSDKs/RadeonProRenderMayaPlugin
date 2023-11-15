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
#include "HybridProContext.h"
#include "RadeonProRender_Baikal.h"
#include "RadeonProRender_MaterialX.h"
#include "ContextCreator.h"
#include "../VulcanUtils.h"

#include "../FireRenderMaterial.h"
#include "../FireRenderStandardMaterial.h"
#include "../FireRenderDoublesided.h"
#include "../FireRenderPBRMaterial.h"
#include "../FireRenderShadowCatcherMaterial.h"
#include "../FireRenderMaterialXMaterial.h"

rpr_int HybridProContext::m_gHybridProPluginID = INCORRECT_PLUGIN_ID;

HybridProContext::HybridProContext()
{

}

rpr_int HybridProContext::GetPluginID()
{
	if (m_gHybridProPluginID == INCORRECT_PLUGIN_ID)
	{
#ifndef OSMac_
#ifdef __linux__
		m_gHybridProPluginID = rprRegisterPlugin("libHybridPro.so");
#else
		m_gHybridProPluginID = rprRegisterPlugin("HybridPro.dll");
#endif
#endif
	}

	return m_gHybridProPluginID;
}

rpr_int HybridProContext::CreateContextInternal(rpr_creation_flags createFlags, rpr_context* pContext)
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

	bool useCpu = (createFlags & RPR_CREATION_FLAGS_ENABLE_CPU) == RPR_CREATION_FLAGS_ENABLE_CPU;

	DebugPrint("* Creating Context: %d (0x%x) - useThread: %d", createFlags, createFlags, useCpu);

	if (isMetalOn() && !(createFlags & RPR_CREATION_FLAGS_ENABLE_CPU))
	{
		createFlags = createFlags | RPR_CREATION_FLAGS_ENABLE_METAL;
	}

	// fallback to GPU - CPU not supported
	if (useCpu)
	{
		createFlags = createFlags & ~RPR_CREATION_FLAGS_ENABLE_CPU;
		createFlags = createFlags | RPR_CREATION_FLAGS_ENABLE_GPU0;
		MGlobal::displayWarning("CPU mode not supported for HybridPro. Falling back to GPU.");
		MGlobal::displayWarning("If render stamp is enabled, please change render device to GPU");
	}

	// context properties
	std::vector<rpr_context_properties> ctxProperties;
	ctxProperties.push_back((rpr_context_properties)RPR_CONTEXT_CREATEPROP_HYBRID_ENABLE_PER_FACE_MATERIALS);
	const bool perFaceEnabled = true;
	ctxProperties.push_back((rpr_context_properties)&perFaceEnabled);

	// - support old GPUs with small memory
	TimePoint beforeGetMemorySizeCall = GetCurrentChronoTime();
	size_t gpuMemorySize = RprVulkanUtils::GetGpuMemory();

	{
		unsigned long timeSpentOnVUlcanCallsInMiliseconds = TimeDiffChrono<std::chrono::milliseconds>(GetCurrentChronoTime(), beforeGetMemorySizeCall);
		std::string str = getFormattedTime(timeSpentOnVUlcanCallsInMiliseconds);
		str = string_format("Time spent on calls to Vulcan AOI: %s\n", str.c_str());
		MGlobal::displayInfo(MString(str.c_str()));
	}

	if (gpuMemorySize < 10_GB)
	{
		uint32_t meshMemorySizeB = 2056_MB;
		uint32_t stagingMemorySizeB = 32_MB;
		uint32_t scratchMemorySizeB = 16_MB;

		std::string message = "Detected GPU memory size less than 10 GB. Render time may be increased!\n";
		MGlobal::displayWarning(message.c_str());

		ctxProperties.push_back((rpr_context_properties)RPR_CONTEXT_CREATEPROP_HYBRID_STAGING_MEMORY_SIZE);
		ctxProperties.push_back((rpr_context_properties)&stagingMemorySizeB);
		ctxProperties.push_back((rpr_context_properties)RPR_CONTEXT_CREATEPROP_HYBRID_SCRATCH_MEMORY_SIZE);
		ctxProperties.push_back((rpr_context_properties)&scratchMemorySizeB);
		ctxProperties.push_back((rpr_context_properties)RPR_CONTEXT_CREATEPROP_HYBRID_MESH_MEMORY_SIZE);
		ctxProperties.push_back((rpr_context_properties)&meshMemorySizeB);
	}
	else
	{
		std::string message = "Detected GPU memory more than 10 GB. Render time should be optimal\n";
		MGlobal::displayInfo(message.c_str());
	}

	ctxProperties.push_back((rpr_context_properties)0);

#ifdef RPR_VERSION_MAJOR_MINOR_REVISION
	int res = rprCreateContext(RPR_VERSION_MAJOR_MINOR_REVISION, plugins, pluginCount, createFlags, ctxProperties.data(), cachePath.asUTF8(), pContext);
#else
	int res = rprCreateContext(RPR_API_VERSION, plugins, pluginCount, createFlags, ctxProperties.data(), cachePath.asUTF8(), pContext);
#endif

	return res;
}

void HybridProContext::setupContextPostSceneCreation(const FireRenderGlobalsData& fireRenderGlobalsData, bool disableWhiteBalance /*= false*/)
{
	HybridContext::setupContextPostSceneCreation(fireRenderGlobalsData, disableWhiteBalance);

	rpr_int frstatus = RPR_SUCCESS; 
	frw::Context context = GetContext();
	rpr_context frcontext = context.Handle();

	// we should disable IBL visibility to correctly composite reflection catcher
	frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_IBL_DISPLAY, fireRenderGlobalsData.reflectionCatcherEnabled ? 0 : 1);
	checkStatus(frstatus);

#ifdef FORCE_ENABLE_VOLUMES
	rprContextSetParameterByKey1u(frcontext, (rpr_context_info)RPR_CONTEXT_ENABLE_VOLUMES, RPR_TRUE);
#endif

	// set dependency for materialX standard surface (could be missing from *.mtlx files)
	frstatus = rprMaterialXAddDependencyMtlx(frcontext, "scripts/standard_surface.mtlx");
	checkStatus(frstatus);
}

bool HybridProContext::IsAOVSupported(int aov) const
{
	static std::set<int> supportedAOVs = {
		RPR_AOV_COLOR,
		RPR_AOV_COLOR_RIGHT,
		RPR_AOV_WORLD_COORDINATE,
		RPR_AOV_UV,
		RPR_AOV_MATERIAL_ID,
		RPR_AOV_SHADING_NORMAL,
		RPR_AOV_DEPTH,
		RPR_AOV_OBJECT_ID,
		RPR_AOV_BACKGROUND,
		RPR_AOV_EMISSION,
		RPR_AOV_DIFFUSE_ALBEDO,
		RPR_AOV_VIEW_SHADING_NORMAL,
		RPR_AOV_SHADOW_CATCHER,
		RPR_AOV_REFLECTION_CATCHER,
		RPR_AOV_MATTE_PASS,
		RPR_AOV_OPACITY,
	};

	return supportedAOVs.find(aov) != supportedAOVs.end();
}

bool HybridProContext::IsRenderQualitySupported(RenderQuality quality) const
{
	return ((quality == RenderQuality::RenderQualityFull) || (quality == RenderQuality::RenderQualityHybridPro));
}

bool HybridProContext::IsShaderNodeSupported(FireMaya::ShaderNode* shaderNode) const
{
	bool isSupported = HybridContext::IsShaderNodeSupported(shaderNode);
	if (isSupported)
		return true;

	FireMaya::RPRDoubleSided* pPRBMaterial = dynamic_cast<FireMaya::RPRDoubleSided*> (shaderNode);
	if (pPRBMaterial != nullptr)
	{
		return true;
	}

	FireMaya::ShadowCatcherMaterial* pShadowMaterial = dynamic_cast<FireMaya::ShadowCatcherMaterial*> (shaderNode);
	if (pShadowMaterial != nullptr)
	{
		return true;
	}

	FireMaya::MaterialXMaterial* pMaterialX = dynamic_cast<FireMaya::MaterialXMaterial*> (shaderNode);
	if (pMaterialX != nullptr)
	{
		return true;
	}

	return false;
}

int HybridProContext::GetAOVMaxValue() const
{
	return RPR_AOV_MAX;
}


void HybridProContext::setupContextHybridParams(const FireRenderGlobalsData& fireRenderGlobalsData)
{
	frw::Context context = GetContext();
	rpr_context frcontext = context.Handle();

	rpr_int frstatus = RPR_SUCCESS;

	bool isViewportRender = (GetRenderType() == RenderType::ViewportRender) || (GetRenderType() == RenderType::IPR);
	if (isViewportRender)
	{
		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_USE_GMON, fireRenderGlobalsData.viewportUseGmon);
		checkStatus(frstatus);

		frstatus = rprContextSetParameterByKey1f(frcontext, RPR_CONTEXT_GINI_COEFFICIENT_FOR_GMON, fireRenderGlobalsData.viewportGiniCoeffGmon);
		checkStatus(frstatus);

		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_PT_DENOISER, fireRenderGlobalsData.viewportPtDenoiser);
		checkStatus(frstatus);

		if (fireRenderGlobalsData.viewportFSR > 0)
		{
			frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_UPSCALER, RPR_UPSCALER_FSR2);
			checkStatus(frstatus);
			frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_FSR2_QUALITY, fireRenderGlobalsData.viewportFSR);
			checkStatus(frstatus);
		}
		else
		{
			frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_UPSCALER, RPR_UPSCALER_NONE);
			checkStatus(frstatus);
		}

		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_MATERIAL_CACHE, fireRenderGlobalsData.viewportMaterialCache);
		checkStatus(frstatus);
		
		//frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_RESTIR_GI, fireRenderGlobalsData.viewportRestirGI);
		//checkStatus(frstatus);
		//
		//frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_RESTIR_GI_BIAS_CORRECTION, fireRenderGlobalsData.viewportRestirGIBiasCorrection);
		//checkStatus(frstatus);
		
		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_RESERVOIR_SAMPLING, fireRenderGlobalsData.viewportReservoirSampling);
		checkStatus(frstatus);
		
		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_RESTIR_SPATIAL_RESAMPLE_ITERATIONS, fireRenderGlobalsData.viewportRestirSpatialResampleIterations);
		checkStatus(frstatus);
		
		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_RESTIR_MAX_RESERVOIRS_PER_CELL, fireRenderGlobalsData.viewportRestirMaxReservoirsPerCell);
		checkStatus(frstatus);

	}
	else
	{
		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_USE_GMON, fireRenderGlobalsData.productionUseGmon);
		checkStatus(frstatus);
		
		frstatus = rprContextSetParameterByKey1f(frcontext, RPR_CONTEXT_GINI_COEFFICIENT_FOR_GMON, fireRenderGlobalsData.productionGiniCoeffGmon);
		checkStatus(frstatus);
		
		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_PT_DENOISER, fireRenderGlobalsData.productionPtDenoiser);
		checkStatus(frstatus);

		if (fireRenderGlobalsData.productionFSR > 0)
		{
			frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_UPSCALER, RPR_UPSCALER_FSR2);
			checkStatus(frstatus);
			frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_FSR2_QUALITY, fireRenderGlobalsData.productionFSR);
			checkStatus(frstatus);
		}
		else
		{
			frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_UPSCALER, RPR_UPSCALER_NONE);
			checkStatus(frstatus);
		}
		
		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_MATERIAL_CACHE, fireRenderGlobalsData.productionMaterialCache);
		checkStatus(frstatus);
		
		//frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_RESTIR_GI, fireRenderGlobalsData.productionRestirGI);
		//checkStatus(frstatus);
		
		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_RESERVOIR_SAMPLING, fireRenderGlobalsData.productionReservoirSampling);
		checkStatus(frstatus);

		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_RESTIR_SPATIAL_RESAMPLE_ITERATIONS, fireRenderGlobalsData.productionRestirSpatialResampleIterations);
		checkStatus(frstatus);

		frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_RESTIR_MAX_RESERVOIRS_PER_CELL, fireRenderGlobalsData.productionRestirMaxReservoirsPerCell);
		checkStatus(frstatus);
	}
}

