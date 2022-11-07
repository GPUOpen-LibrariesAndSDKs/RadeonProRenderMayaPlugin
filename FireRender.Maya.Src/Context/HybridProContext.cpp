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
#include "ContextCreator.h"

#include "../FireRenderMaterial.h"
#include "../FireRenderStandardMaterial.h"

#include "../FireRenderPBRMaterial.h"

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
		RPR_AOV_VIEW_SHADING_NORMAL };

	return supportedAOVs.find(aov) != supportedAOVs.end();
}

bool HybridProContext::IsRenderQualitySupported(RenderQuality quality) const
{
	return ((quality == RenderQuality::RenderQualityFull) || (quality == RenderQuality::RenderQualityHybridPro));
}

