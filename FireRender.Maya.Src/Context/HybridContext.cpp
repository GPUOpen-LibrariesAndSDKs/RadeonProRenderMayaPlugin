#include "HybridContext.h"
#include "RadeonProRender_Baikal.h"
#include "ContextCreator.h"

rpr_int HybridContext::m_gHybridPluginID = -1;

HybridContext::HybridContext()
{

}

rpr_int HybridContext::GetPluginID()
{
	if (m_gHybridPluginID == -1)
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

	if (pluginID == -1)
	{
		MGlobal::displayError("Unable to register Radeon ProRender plug-in.");
		return -1;
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

void HybridContext::setupContext(const FireRenderGlobalsData& fireRenderGlobalsData, bool disableWhiteBalance)
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

	frstatus = rprContextSetParameter1u(frcontext, "maxRecursion", maxRecursion);
	checkStatus(frstatus);

	frstatus = frContextSetParameterByKey1u(frcontext, RPR_CONTEXT_Y_FLIP, 1);
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
	frstatus = rprContextSetParameter1f(frcontext, "displaygamma", displayGammaValue);
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
		RPR_AOV_MATERIAL_IDX,
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
	return quality != RenderQuality::RenderQualityFull;
}
