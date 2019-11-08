#include "TahoeContext.h"

rpr_int TahoeContext::m_gTahoePluginID = -1;

TahoeContext::TahoeContext()
{

}

rpr_int TahoeContext::GetPluginID()
{
	if (m_gTahoePluginID == -1)
	{
#ifdef OSMac_
		m_gTahoePluginID = rprRegisterPlugin("/Users/Shared/RadeonProRender/lib/libTahoe64.dylib");
#elif __linux__
		m_gTahoePluginID = rprRegisterPlugin("libTahoe64.so");
#else
		m_gTahoePluginID = rprRegisterPlugin("Tahoe64.dll");
#endif
	}

	return m_gTahoePluginID;
}

rpr_int TahoeContext::CreateContextInternal(rpr_creation_flags createFlags, rpr_context* pContext)
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

	// setup CPU thread count
	std::vector<rpr_context_properties> ctxProperties;
#if (RPR_VERSION_MINOR < 34)
	ctxProperties.push_back((rpr_context_properties)RPR_CONTEXT_CREATEPROP_SAMPLER_TYPE);
#else
	ctxProperties.push_back((rpr_context_properties)RPR_CONTEXT_SAMPLER_TYPE);
#endif
	ctxProperties.push_back((rpr_context_properties)RPR_CONTEXT_SAMPLER_TYPE_CMJ);

	int threadCountToOverride = getThreadCountToOverride();
	if ((createFlags & RPR_CREATION_FLAGS_ENABLE_CPU) && threadCountToOverride > 0)
	{
#if (RPR_VERSION_MINOR < 34)
		ctxProperties.push_back((rpr_context_properties)RPR_CONTEXT_CREATEPROP_CPU_THREAD_LIMIT);
#else
		ctxProperties.push_back((rpr_context_properties)RPR_CONTEXT_CPU_THREAD_LIMIT);
#endif
		ctxProperties.push_back((void*)(size_t)threadCountToOverride);
	}

	ctxProperties.push_back((rpr_context_properties)0);

#ifdef RPR_VERSION_MAJOR_MINOR_REVISION
	int res = rprCreateContext(RPR_VERSION_MAJOR_MINOR_REVISION, plugins, pluginCount, createFlags, ctxProperties.data(), cachePath.asUTF8(), pContext);
#else
	int res = rprCreateContext(RPR_API_VERSION, plugins, pluginCount, createFlags, ctxProperties.data(), cachePath.asUTF8(), pContext);
#endif

	return res;
}

void TahoeContext::setupContext(const FireRenderGlobalsData& fireRenderGlobalsData, bool disableWhiteBalance)
{
	frw::Context context = GetContext();
	rpr_context frcontext = context.Handle();

	rpr_int frstatus = RPR_SUCCESS;

	frstatus = rprContextSetParameter1f(frcontext, "pdfthreshold", 0.0000f);
	checkStatus(frstatus);

	if (GetRenderType() == RenderType::Thumbnail)
	{
		updateTonemapping(fireRenderGlobalsData, false);
		return;
	}

	frstatus = rprContextSetParameter1f(frcontext, "radianceclamp", fireRenderGlobalsData.giClampIrradiance ? fireRenderGlobalsData.giClampIrradianceValue : FLT_MAX);
	checkStatus(frstatus);

	frstatus = rprContextSetParameter1u(frcontext, "texturecompression", fireRenderGlobalsData.textureCompression);
	checkStatus(frstatus);

	frstatus = rprContextSetParameter1u(frcontext, "as.tilesize", fireRenderGlobalsData.adaptiveTileSize);
	checkStatus(frstatus);

	frstatus = rprContextSetParameter1f(frcontext, "as.threshold", fireRenderGlobalsData.adaptiveThreshold);
	checkStatus(frstatus);

	if (GetRenderType() == RenderType::ProductionRender) // production (final) rendering
	{
		frstatus = rprContextSetParameter1u(frcontext, "rendermode", fireRenderGlobalsData.renderMode);
		checkStatus(frstatus);

		setSamplesPerUpdate(fireRenderGlobalsData.samplesPerUpdate);

		frstatus = rprContextSetParameter1u(frcontext, "maxRecursion", fireRenderGlobalsData.maxRayDepth);
		checkStatus(frstatus);

		frstatus = rprContextSetParameter1u(frcontext, "maxdepth.diffuse", fireRenderGlobalsData.maxRayDepthDiffuse);
		checkStatus(frstatus);

		frstatus = rprContextSetParameter1u(frcontext, "maxdepth.glossy", fireRenderGlobalsData.maxRayDepthGlossy);
		checkStatus(frstatus);

		frstatus = rprContextSetParameter1u(frcontext, "maxdepth.refraction", fireRenderGlobalsData.maxRayDepthRefraction);
		checkStatus(frstatus);

		frstatus = rprContextSetParameter1u(frcontext, "maxdepth.refraction.glossy", fireRenderGlobalsData.maxRayDepthGlossyRefraction);
		checkStatus(frstatus);

		frstatus = rprContextSetParameter1u(frcontext, "maxdepth.shadow", fireRenderGlobalsData.maxRayDepthShadow);
		checkStatus(frstatus);

		frstatus = rprContextSetParameter1u(frcontext, "as.minspp", fireRenderGlobalsData.completionCriteriaFinalRender.completionCriteriaMinIterations);
		checkStatus(frstatus);
	}
	else if (isInteractive())
	{
		setSamplesPerUpdate(1);

		frstatus = rprContextSetParameter1u(frcontext, "rendermode", fireRenderGlobalsData.viewportRenderMode);
		checkStatus(frstatus);

		frstatus = rprContextSetParameter1u(frcontext, "maxRecursion", fireRenderGlobalsData.viewportMaxRayDepth);
		checkStatus(frstatus);

		frstatus = rprContextSetParameter1u(frcontext, "maxdepth.diffuse", fireRenderGlobalsData.viewportMaxDiffuseRayDepth);
		checkStatus(frstatus);

		frstatus = rprContextSetParameter1u(frcontext, "maxdepth.glossy", fireRenderGlobalsData.viewportMaxReflectionRayDepth);
		checkStatus(frstatus);

		frstatus = rprContextSetParameter1u(frcontext, "maxdepth.refraction", fireRenderGlobalsData.viewportMaxReflectionRayDepth);
		checkStatus(frstatus);

		frstatus = rprContextSetParameter1u(frcontext, "maxdepth.refraction.glossy", fireRenderGlobalsData.viewportMaxReflectionRayDepth);
		checkStatus(frstatus);

		frstatus = rprContextSetParameter1u(frcontext, "maxdepth.shadow", fireRenderGlobalsData.viewportMaxDiffuseRayDepth);
		checkStatus(frstatus);

		frstatus = rprContextSetParameter1u(frcontext, "as.minspp", fireRenderGlobalsData.completionCriteriaViewport.completionCriteriaMinIterations);
		checkStatus(frstatus);
	}

	frstatus = rprContextSetParameter1f(frcontext, "raycastepsilon", fireRenderGlobalsData.raycastEpsilon);
	checkStatus(frstatus);

	frstatus = rprContextSetParameter1u(frcontext, "imagefilter.type", fireRenderGlobalsData.filterType);
	checkStatus(frstatus);

	//

	std::string filterAttrName = "imagefilter.box.radius";
	switch (fireRenderGlobalsData.filterType)
	{
	case 2:
	{
		filterAttrName = "imagefilter.triangle.radius";
		break;
	}
	case 3:
	{
		filterAttrName = "imagefilter.gaussian.radius";
		break;
	}
	case 4:
	{
		filterAttrName = "imagefilter.mitchell.radius";
		break;
	}
	case 5:
	{
		filterAttrName = "imagefilter.lanczos.radius";
		break;
	}
	case 6:
	{
		filterAttrName = "imagefilter.blackmanharris.radius";
		break;
	}
	default:
		break;
	}

	frstatus = rprContextSetParameter1f(frcontext, filterAttrName.c_str(), fireRenderGlobalsData.filterSize);
	checkStatus(frstatus);

	frstatus = rprContextSetParameter1u(frcontext, "metalperformanceshader", fireRenderGlobalsData.useMPS ? 1 : 0);
	checkStatus(frstatus);

	updateTonemapping(fireRenderGlobalsData, disableWhiteBalance);
}

void TahoeContext::updateTonemapping(const FireRenderGlobalsData& fireRenderGlobalsData, bool disableWhiteBalance)
{
	frw::Context context = GetContext();
	rpr_context frcontext = context.Handle();

	rpr_int frstatus = RPR_SUCCESS;

	frstatus = rprContextSetParameter1f(frcontext, "texturegamma", fireRenderGlobalsData.textureGamma);
	checkStatus(frstatus);

	// Disable display gamma correction unless it is being applied
	// to Maya views. It will be always be enabled before file output.
	auto displayGammaValue = fireRenderGlobalsData.applyGammaToMayaViews ? fireRenderGlobalsData.displayGamma : 1.0f;
	frstatus = rprContextSetParameter1f(frcontext, "displaygamma", displayGammaValue);
	checkStatus(frstatus);

	// Release existing effects
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

	if (tonemap)
	{
		context.Detach(tonemap);
		tonemap.Reset();
	}

	if (normalization)
	{
		context.Detach(normalization);
		normalization.Reset();
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
	normalization = frw::PostEffect(context, frw::PostEffectTypeNormalization);
	context.Attach(normalization);

	// Create new effects
	switch (fireRenderGlobalsData.toneMappingType)
	{
	case 0:
		break;

	case 1:
		if (!tonemap)
		{
			tonemap = frw::PostEffect(context, frw::PostEffectTypeToneMap);
			context.Attach(tonemap);
		}
		context.SetParameter("tonemapping.type", RPR_TONEMAPPING_OPERATOR_LINEAR);
		context.SetParameter("tonemapping.linear.scale", fireRenderGlobalsData.toneMappingLinearScale);
		break;

	case 2:
		if (!tonemap)
		{
			tonemap = frw::PostEffect(context, frw::PostEffectTypeToneMap);
			context.Attach(tonemap);
		}
		context.SetParameter("tonemapping.type", RPR_TONEMAPPING_OPERATOR_PHOTOLINEAR);
		context.SetParameter("tonemapping.photolinear.sensitivity", fireRenderGlobalsData.toneMappingPhotolinearSensitivity);
		context.SetParameter("tonemapping.photolinear.fstop", fireRenderGlobalsData.toneMappingPhotolinearFstop);
		context.SetParameter("tonemapping.photolinear.exposure", fireRenderGlobalsData.toneMappingPhotolinearExposure);
		break;

	case 3:
		if (!tonemap)
		{
			tonemap = frw::PostEffect(context, frw::PostEffectTypeToneMap);
			context.Attach(tonemap);
		}
		context.SetParameter("tonemapping.type", RPR_TONEMAPPING_OPERATOR_AUTOLINEAR);
		break;

	case 4:
		if (!tonemap)
		{
			tonemap = frw::PostEffect(context, frw::PostEffectTypeToneMap);
			context.Attach(tonemap);
		}
		context.SetParameter("tonemapping.type", RPR_TONEMAPPING_OPERATOR_MAXWHITE);
		break;

	case 5:
		if (!tonemap)
		{
			tonemap = frw::PostEffect(context, frw::PostEffectTypeToneMap);
			context.Attach(tonemap);
		}
		context.SetParameter("tonemapping.type", RPR_TONEMAPPING_OPERATOR_REINHARD02);
		context.SetParameter("tonemapping.reinhard02.prescale", fireRenderGlobalsData.toneMappingReinhard02Prescale);
		context.SetParameter("tonemapping.reinhard02.postscale", fireRenderGlobalsData.toneMappingReinhard02Postscale);
		context.SetParameter("tonemapping.reinhard02.burn", fireRenderGlobalsData.toneMappingReinhard02Burn);
		break;

	case 6:
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

	if (fireRenderGlobalsData.toneMappingWhiteBalanceEnabled && !disableWhiteBalance)
	{
		if (!white_balance)
		{
			white_balance = frw::PostEffect(context, frw::PostEffectTypeWhiteBalance);
			context.Attach(white_balance);
		}
		float temperature = fireRenderGlobalsData.toneMappingWhiteBalanceValue;
		white_balance.SetParameter("colorspace", RPR_COLOR_SPACE_SRGB); // check: Max uses Adobe SRGB here
		white_balance.SetParameter("colortemp", temperature);
	}
}

bool TahoeContext::needResolve() const
{
	return bool(white_balance) || bool(simple_tonemap) || bool(tonemap) || bool(normalization) || bool(gamma_correction);
}

bool TahoeContext::IsRenderQualitySupported(RenderQuality quality) const
{
	return quality == RenderQuality::RenderQualityFull;
}
