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
#include "HybridContext.h"
#include "RadeonProRender_Baikal.h"
#include "ContextCreator.h"

#include "../FireRenderMaterial.h"
#include "../FireRenderStandardMaterial.h"

#include "../FireRenderPBRMaterial.h"

rpr_int HybridContext::m_gHybridPluginID = INCORRECT_PLUGIN_ID;


HybridContext::HybridContext()
{

}

rpr_int HybridContext::GetPluginID()
{
	if (m_gHybridPluginID == INCORRECT_PLUGIN_ID)
	{
#ifdef OSMac_
		m_gHybridPluginID = rprRegisterPlugin("/Users/Shared/RadeonProRender/lib/libHybrid.dylib");
#elif __linux__
		m_gHybridPluginID = rprRegisterPlugin("libHybrid.so");
#else
		m_gHybridPluginID = rprRegisterPlugin("Hybrid.dll");
#endif
	}

	return m_gHybridPluginID;
}

bool HybridContext::IsGLInteropEnabled() const
{
	return false;
}

rpr_int HybridContext::CreateContextInternal(rpr_creation_flags createFlags, rpr_context* pContext)
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

	bool useThread = (createFlags & RPR_CREATION_FLAGS_ENABLE_CPU) == RPR_CREATION_FLAGS_ENABLE_CPU;

	DebugPrint("* Creating Context: %d (0x%x) - useThread: %d", createFlags, createFlags, useThread);

	if (isMetalOn() && !(createFlags & RPR_CREATION_FLAGS_ENABLE_CPU))
	{
		createFlags = createFlags | RPR_CREATION_FLAGS_ENABLE_METAL;
	}

	// setup CPU thread count
	std::vector<rpr_context_properties> ctxProperties;

	ctxProperties.push_back((rpr_context_properties)0);

#ifdef RPR_VERSION_MAJOR_MINOR_REVISION
	int res = rprCreateContext(RPR_VERSION_MAJOR_MINOR_REVISION, plugins, pluginCount, createFlags, ctxProperties.data(), cachePath.asUTF8(), pContext);
#else
	int res = rprCreateContext(RPR_API_VERSION, plugins, pluginCount, createFlags, ctxProperties.data(), cachePath.asUTF8(), pContext);
#endif

	return res;
}

void HybridContext::setupContextPostSceneCreation(const FireRenderGlobalsData& fireRenderGlobalsData, bool disableWhiteBalance)
{
	frw::Context context = GetContext();
	rpr_context frcontext = context.Handle();

	rpr_int frstatus = RPR_SUCCESS;

	// Hybrid does not support "iterations"
	setSamplesPerUpdate(1);

	int maxRecursion = 0;
	if (GetRenderType() == RenderType::ProductionRender) // production (final) rendering
	{
		maxRecursion = fireRenderGlobalsData.maxRayDepth;
	}
	else if (isInteractive())
	{
		maxRecursion = fireRenderGlobalsData.viewportMaxRayDepth;
	}

	frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_MAX_RECURSION, maxRecursion);
	checkStatus(frstatus);

	frstatus = rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_Y_FLIP, 1);
	checkStatus(frstatus);

	SetRenderQuality(GetRenderQualityForRenderType(GetRenderType()));

	updateTonemapping(fireRenderGlobalsData, disableWhiteBalance);
}

void HybridContext::updateTonemapping(const FireRenderGlobalsData& fireRenderGlobalsData, bool disableWhiteBalance)
{
	frw::Context context = GetContext();
	rpr_context frcontext = context.Handle();

	rpr_int frstatus = RPR_SUCCESS;

	float displayGammaValue = fireRenderGlobalsData.applyGammaToMayaViews ? fireRenderGlobalsData.displayGamma : 1.0f;
	frstatus = rprContextSetParameterByKey1f(frcontext, RPR_CONTEXT_DISPLAY_GAMMA, displayGammaValue);
	checkStatus(frstatus);

	m_restartRender = true;
}

bool HybridContext::IsAOVSupported(int aov) const
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
		RPR_AOV_VIEW_SHADING_NORMAL };

	return supportedAOVs.find(aov) != supportedAOVs.end();
}

rpr_int HybridContext::SetRenderQuality(RenderQuality quality)
{
	rpr_uint resolution;

	switch (quality)
	{
	case RenderQuality::RenderQualityHigh:
		resolution = RPR_RENDER_QUALITY_HIGH;
		break;
	case RenderQuality::RenderQualityMedium:
		resolution = RPR_RENDER_QUALITY_MEDIUM;
		break;
	case RenderQuality::RenderQualityLow:
		resolution = RPR_RENDER_QUALITY_LOW;
		break;
	default:
		return RPR_ERROR_INVALID_PARAMETER;
	}
	
	return rprContextSetParameterByKey1u(GetScope().Context().Handle(), RPR_CONTEXT_RENDER_QUALITY, resolution);
}

FireRenderEnvLight* HybridContext::CreateEnvLight(const MDagPath& dagPath)
{
	return CreateSceneObject<FireRenderEnvLightHybrid, NodeCachingOptions::AddPath>(dagPath);
}

FireRenderSky* HybridContext::CreateSky(const MDagPath& dagPath)
{
	return CreateSceneObject<FireRenderSkyHybrid, NodeCachingOptions::AddPath>(dagPath);
}

bool HybridContext::IsRenderQualitySupported(RenderQuality quality) const
{
	return ((quality != RenderQuality::RenderQualityFull) && (quality != RenderQuality::RenderQualityNorthStar));
}

bool HybridContext::IsPhysicalLightTypeSupported(PLType lightType) const
{
	if (lightType == PLTDisk || lightType == PLTSphere)
	{
		return false;
	}

	return true;
}

bool HybridContext::IsShaderSupported(frw::ShaderType type) const 
{
	// Hybrid supports only Uber and Emissive shaders

	return type == frw::ShaderType::ShaderTypeEmissive ||
			type == frw::ShaderType::ShaderTypeStandard;
}

frw::Shader HybridContext::GetDefaultColorShader(frw::Value color)
{
	frw::Shader shader (GetMaterialSystem(), GetContext());
	shader.xSetValue(RPR_MATERIAL_INPUT_UBER_DIFFUSE_COLOR, color);

	return shader;
}

bool HybridContext::IsShaderNodeSupported(FireMaya::ShaderNode* shaderNode) const
{
	FireMaya::Material* pMaterial = dynamic_cast<FireMaya::Material*> (shaderNode);
	FireMaya::StandardMaterial* pUberMaterial = dynamic_cast<FireMaya::StandardMaterial*> (shaderNode);
	FireMaya::FireRenderPBRMaterial* pPRBMaterial = dynamic_cast<FireMaya::FireRenderPBRMaterial*> (shaderNode);

	if (pUberMaterial != nullptr || pPRBMaterial != nullptr)
	{
		return true;
	}

	if (pMaterial != nullptr)
	{
		return pMaterial->GetMayaShaderType() == FireMaya::Material::kEmissive;
	}

	return false;
}