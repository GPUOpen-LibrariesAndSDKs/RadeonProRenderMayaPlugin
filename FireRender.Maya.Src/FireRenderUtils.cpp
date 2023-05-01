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
#include "RprTools.h"

#include "FireRenderUtils.h"
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MItDag.h>
#include <maya/MDagPathArray.h>
#include <maya/MFnRenderLayer.h>
#include <maya/MSelectionList.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnMesh.h>
#include <maya/MObjectArray.h>
#include <maya/MIntArray.h>
#include <maya/MFnComponentListData.h>
#include <maya/MFnSingleIndexedComponent.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFileObject.h>
#include <maya/MCommonSystemUtils.h>

#include <cassert>
#include <vector>
#include <time.h>
#include <iostream>

#include "attributeNames.h"
#include "OptionVarHelpers.h"
#include "Context/TahoeContext.h"
#include "FireRenderGlobals.h"
#include "FireRenderThread.h"
#include "Logger.h"

#ifdef WIN32
#include <ShlObj.h>
#include <comdef.h>
#include <Wbemidl.h>
#endif

#ifdef __linux__
#include "float.h"
#endif

#ifdef __APPLE__
#ifndef FLT_MAX
#include <math.h>
#ifdef __FLT_MAX__
#define FLT_MAX __FLT_MAX__
#endif
#endif
#endif

#ifndef MAYA2015
#include <maya/MUuid.h>
#endif

#pragma comment(lib, "wbemuuid.lib")

FireRenderGlobalsData::FireRenderGlobalsData() :
	adaptiveTileSize(1),
	adaptiveThreshold(0.0f),
	adaptiveThresholdViewport(0.0f),
	textureCompression(false),
	giClampIrradiance(true),
	giClampIrradianceValue(1.0),
	samplesPerUpdate(5),
	filterType(0),
	filterSize(2),
	maxRayDepth(2),
	maxRayDepthDiffuse(2),
	maxRayDepthGlossy(2),
	maxRayDepthRefraction(2),
	maxRayDepthGlossyRefraction(2),
	maxRayDepthShadow(2),
	viewportMaxRayDepth(2),
	viewportMaxDiffuseRayDepth(2),
	viewportMaxReflectionRayDepth(2),
	viewportRenderMode(FireRenderGlobals::kGlobalIllumination),
	renderMode(FireRenderGlobals::kGlobalIllumination),
	commandPort(0),
	useRenderStamp(false),
	renderStampText(""),
	toneMappingType(0),
	toneMappingLinearScale(1.0f),
	toneMappingPhotolinearSensitivity(1.0f),
	toneMappingPhotolinearExposure(1.0f),
	toneMappingPhotolinearFstop(1.0f),
	toneMappingReinhard02Prescale(1.0f),
	toneMappingReinhard02Postscale(1.0f),
	toneMappingReinhard02Burn(1.0f),
	toneMappingSimpleExposure(1.0f),
	toneMappingSimpleContrast(1.0f),
	toneMappingSimpleTonemap(true),
	motionBlur(false),
	cameraMotionBlur(false),
	motionBlurCameraExposure(0.0f),
	motionSamples(0),
	tileRenderingEnabled(false),
	tileSizeX(0),
	tileSizeY(0),
	cameraType(0),
	useMPS(false),
	useDetailedContextWorkLog(false),
	deepEXRMergeZThreshold(0.1f),
	contourIsEnabled(false),
	contourUseObjectID(true),
	contourUseMaterialID(true),
	contourUseShadingNormal(true),
	contourUseUV(true),
	contourLineWidthObjectID(1.0f),
	contourLineWidthMaterialID(1.0f),
	contourLineWidthShadingNormal(1.0f),
	contourLineWidthUV(1.0f),
	contourNormalThreshold(45.0f),
	contourUVThreshold(45.0f),
	contourAntialiasing(1.0f),
	contourIsDebugEnabled(false),
	cryptomatteExtendedMode(false),
	cryptomatteSplitIndirect(false),
	useOpenCLContext(false)
{

}

FireRenderGlobalsData::~FireRenderGlobalsData()
{

}

void FireRenderGlobalsData::readFromCurrentScene()
{
	FireMaya::FireRenderThread::RunProcOnMainThread([this]
	{
		MStatus status;

		// Get RadeonProRenderGlobals node
		MObject fireRenderGlobals;
		status = GetRadeonProRenderGlobals(fireRenderGlobals);
		if (status != MS::kSuccess)
		{
			// If not exists, create one
			MGlobal::executeCommand("if (!(`objExists RadeonProRenderGlobals`)){ createNode -n RadeonProRenderGlobals -ss RadeonProRenderGlobals; }");
			// and try to get it again	
			status = GetRadeonProRenderGlobals(fireRenderGlobals);
			if (status != MS::kSuccess)
			{
				MGlobal::displayError("No RadeonProRenderGlobals node found");
				return;
			}
		}

		// Get Fire render globals attributes
		MFnDependencyNode frGlobalsNode(fireRenderGlobals);

/*		MPlug plug = frGlobalsNode.findPlug("completionCriteriaType");
		if (!plug.isNull())
			completionCriteriaFinalRender.completionCriteriaType = plug.asShort();*/

		MPlug plug = frGlobalsNode.findPlug("completionCriteriaHours");
		if (!plug.isNull())
			completionCriteriaFinalRender.completionCriteriaHours = plug.asInt();

		plug = frGlobalsNode.findPlug("completionCriteriaMinutes");
		if (!plug.isNull())
			completionCriteriaFinalRender.completionCriteriaMinutes = plug.asInt();

		plug = frGlobalsNode.findPlug("completionCriteriaSeconds");
		if (!plug.isNull())
			completionCriteriaFinalRender.completionCriteriaSeconds = plug.asInt();

		plug = frGlobalsNode.findPlug("completionCriteriaIterations");
		if (!plug.isNull())
			completionCriteriaFinalRender.completionCriteriaMaxIterations = plug.asInt();

		plug = frGlobalsNode.findPlug("completionCriteriaMinIterations");
		if (!plug.isNull())
			completionCriteriaFinalRender.completionCriteriaMinIterations = plug.asInt();


		/*plug = frGlobalsNode.findPlug("completionCriteriaTypeViewport");
		if (!plug.isNull())
			completionCriteriaViewport.completionCriteriaType = plug.asShort();*/

		plug = frGlobalsNode.findPlug("completionCriteriaHoursViewport");
		if (!plug.isNull())
			completionCriteriaViewport.completionCriteriaHours = plug.asInt();

		plug = frGlobalsNode.findPlug("completionCriteriaMinutesViewport");
		if (!plug.isNull())
			completionCriteriaViewport.completionCriteriaMinutes = plug.asInt();

		plug = frGlobalsNode.findPlug("completionCriteriaSecondsViewport");
		if (!plug.isNull())
			completionCriteriaViewport.completionCriteriaSeconds = plug.asInt();

		plug = frGlobalsNode.findPlug("completionCriteriaIterationsViewport");
		if (!plug.isNull())
			completionCriteriaViewport.completionCriteriaMaxIterations = plug.asInt();

		plug = frGlobalsNode.findPlug("completionCriteriaMinIterationsViewport");
		if (!plug.isNull())
			completionCriteriaViewport.completionCriteriaMinIterations = plug.asInt();


		plug = frGlobalsNode.findPlug("adaptiveTileSize");
		if (!plug.isNull())
			adaptiveTileSize = plug.asInt();

		plug = frGlobalsNode.findPlug("adaptiveThreshold");
		if (!plug.isNull())
			adaptiveThreshold = plug.asFloat();

		plug = frGlobalsNode.findPlug("adaptiveThresholdViewport");
		if (!plug.isNull())
			adaptiveThresholdViewport = plug.asFloat();

		plug = frGlobalsNode.findPlug("textureCompression");
		if (!plug.isNull())
			textureCompression = plug.asBool();

		plug = frGlobalsNode.findPlug("renderModeViewport");
		if (!plug.isNull())
			viewportRenderMode = plug.asInt();

		plug = frGlobalsNode.findPlug("renderMode");
		if (!plug.isNull())
			renderMode = plug.asInt();

		plug = frGlobalsNode.findPlug("textureCachePath");
		if (!plug.isNull())
			textureCachePath = plug.asString();

		plug = frGlobalsNode.findPlug("giClampIrradiance");
		if (!plug.isNull())
			giClampIrradiance = plug.asBool();
		plug = frGlobalsNode.findPlug("giClampIrradianceValue");
		if (!plug.isNull())
			giClampIrradianceValue = plug.asFloat();

		plug = frGlobalsNode.findPlug("samplesPerUpdate");
		if (!plug.isNull())
			samplesPerUpdate = plug.asInt();

		plug = frGlobalsNode.findPlug("filter");
		if (!plug.isNull())
			filterType = plug.asShort();

		plug = frGlobalsNode.findPlug("filterSize");
		if (!plug.isNull())
			filterSize = plug.asShort();

		plug = frGlobalsNode.findPlug("maxRayDepth");
		if (!plug.isNull())
			maxRayDepth = plug.asShort();

		plug = frGlobalsNode.findPlug("maxDepthDiffuse");
		if (!plug.isNull())
			maxRayDepthDiffuse = plug.asShort();

		plug = frGlobalsNode.findPlug("maxDepthGlossy");
		if (!plug.isNull())
			maxRayDepthGlossy = plug.asShort();

		plug = frGlobalsNode.findPlug("maxDepthRefraction");
		if (!plug.isNull())
			maxRayDepthRefraction = plug.asShort();

		plug = frGlobalsNode.findPlug("maxDepthRefractionGlossy");
		if (!plug.isNull())
			maxRayDepthGlossyRefraction = plug.asShort();

		plug = frGlobalsNode.findPlug("maxDepthShadow");
		if (!plug.isNull())
			maxRayDepthShadow = plug.asShort();

		plug = frGlobalsNode.findPlug("maxRayDepthViewport");
		if (!plug.isNull())
			viewportMaxRayDepth = plug.asShort();

		plug = frGlobalsNode.findPlug("maxDepthDiffuseViewport");
		if (!plug.isNull())
			viewportMaxDiffuseRayDepth = plug.asShort();

		plug = frGlobalsNode.findPlug("maxDepthGlossyViewport");
		if (!plug.isNull())
			viewportMaxReflectionRayDepth = plug.asShort();

		// 3 Tile Rendering related parameters
		plug = frGlobalsNode.findPlug("tileRenderEnabled");
		if (!plug.isNull())
			tileRenderingEnabled = plug.asBool();

		plug = frGlobalsNode.findPlug("tileRenderX");
		if (!plug.isNull())
			tileSizeX = plug.asInt();

		plug = frGlobalsNode.findPlug("tileRenderY");
		if (!plug.isNull())
			tileSizeY = plug.asInt();

		// In UI raycast epsilon defined in 1/10 of scene units, convert it to meters
		plug = frGlobalsNode.findPlug("raycastEpsilon");
		if (!plug.isNull())
		{
			const MDistance::Unit sceneUnits = MDistance::uiUnit();
			double epsilonUIValue = plug.asFloat();
			raycastEpsilon = (float)( MDistance((epsilonUIValue * 0.1f), sceneUnits).asMeters());
		}

		plug = frGlobalsNode.findPlug("enableOOC");
		if (!plug.isNull())
			enableOOC = plug.asBool();

		plug = frGlobalsNode.findPlug("textureCacheSize");
		if (!plug.isNull())
			oocTexCache = plug.asInt();

/*		plug = frGlobalsNode.findPlug("maxRayDepthViewport");
		if (!plug.isNull())
			maxRayDepthViewport = plug.asShort();*/

		plug = frGlobalsNode.findPlug("commandPort");
		if (!plug.isNull())
			commandPort = plug.asInt();

		plug = frGlobalsNode.findPlug("applyGammaToMayaViews");
		if (!plug.isNull())
			applyGammaToMayaViews = plug.asBool();

		plug = frGlobalsNode.findPlug("displayGamma");
		if (!plug.isNull())
			displayGamma = plug.asFloat();

		plug = frGlobalsNode.findPlug("useRenderStamp");
		if (!plug.isNull())
			useRenderStamp = plug.asBool();
		plug = frGlobalsNode.findPlug("renderStampText");
		if (!plug.isNull())
			renderStampText = plug.asString();

		plug = frGlobalsNode.findPlug("textureGamma");
		if (!plug.isNull())
			textureGamma = plug.asFloat();

		plug = frGlobalsNode.findPlug("toneMappingType");
		if (!plug.isNull())
			toneMappingType = plug.asShort();

		plug = frGlobalsNode.findPlug("toneMappingWhiteBalanceEnabled");
		if (!plug.isNull()) {
			toneMappingWhiteBalanceEnabled = plug.asBool();
		}

		plug = frGlobalsNode.findPlug("toneMappingWhiteBalanceValue");
		if (!plug.isNull()) {
			toneMappingWhiteBalanceValue = plug.asFloat();
		}

		plug = frGlobalsNode.findPlug("toneMappingLinearScale");
		if (!plug.isNull())
			toneMappingLinearScale = plug.asFloat();

		plug = frGlobalsNode.findPlug("toneMappingPhotolinearSensitivity");
		if (!plug.isNull())
			toneMappingPhotolinearSensitivity = plug.asFloat();

		plug = frGlobalsNode.findPlug("toneMappingPhotolinearExposure");
		if (!plug.isNull())
			toneMappingPhotolinearExposure = plug.asFloat();

		plug = frGlobalsNode.findPlug("toneMappingPhotolinearFstop");
		if (!plug.isNull())
			toneMappingPhotolinearFstop = plug.asFloat();

		plug = frGlobalsNode.findPlug("toneMappingReinhard02Prescale");
		if (!plug.isNull())
			toneMappingReinhard02Prescale = plug.asFloat();

		plug = frGlobalsNode.findPlug("toneMappingReinhard02Postscale");
		if (!plug.isNull())
			toneMappingReinhard02Postscale = plug.asFloat();

		plug = frGlobalsNode.findPlug("toneMappingReinhard02Burn");
		if (!plug.isNull())
			toneMappingReinhard02Burn = plug.asFloat();

		plug = frGlobalsNode.findPlug("toneMappingSimpleTonemap");
		if (!plug.isNull())
			toneMappingSimpleTonemap = plug.asBool();

		plug = frGlobalsNode.findPlug("toneMappingSimpleExposure");
		if (!plug.isNull())
			toneMappingSimpleExposure = plug.asFloat();

		plug = frGlobalsNode.findPlug("toneMappingSimpleContrast");
		if (!plug.isNull())
			toneMappingSimpleContrast = plug.asFloat();

		plug = frGlobalsNode.findPlug("motionBlur");
		if (!plug.isNull())
			motionBlur = plug.asBool();

		plug = frGlobalsNode.findPlug("cameraMotionBlur");
		if (!plug.isNull())
			cameraMotionBlur = plug.asBool();

		plug = frGlobalsNode.findPlug("motionBlurViewport");
		if (!plug.isNull())
			viewportMotionBlur = plug.asBool();

		plug = frGlobalsNode.findPlug("velocityAOVMotionBlur");
		if (!plug.isNull())
			velocityAOVMotionBlur = plug.asBool();
		
		plug = frGlobalsNode.findPlug("motionBlurCameraExposure");
		if (!plug.isNull())
			motionBlurCameraExposure = plug.asFloat();

		plug = frGlobalsNode.findPlug("motionSamples");
		if (!plug.isNull())
			motionSamples = plug.asInt();	

		plug = frGlobalsNode.findPlug("cameraType");
		if (!plug.isNull())
			cameraType = plug.asShort();

		// Use Metal Performance Shaders for MacOS
		plug = frGlobalsNode.findPlug("useMPS");
		if (!plug.isNull())
			useMPS = plug.asBool();

		plug = frGlobalsNode.findPlug("detailedLog");
		if (!plug.isNull())
			useDetailedContextWorkLog = plug.asBool();

		plug = frGlobalsNode.findPlug("contourIsEnabled");
		if (!plug.isNull())
			contourIsEnabled = plug.asBool();

		plug = frGlobalsNode.findPlug("contourUseObjectID");
		if (!plug.isNull())
			contourUseObjectID = plug.asBool();

		plug = frGlobalsNode.findPlug("contourUseMaterialID");
		if (!plug.isNull())
			contourUseMaterialID = plug.asBool();

		plug = frGlobalsNode.findPlug("contourUseShadingNormal");
		if (!plug.isNull())
			contourUseShadingNormal = plug.asBool();

		plug = frGlobalsNode.findPlug("contourUseUV");
		if (!plug.isNull())
			contourUseUV = plug.asBool();

		plug = frGlobalsNode.findPlug("contourLineWidthObjectID");
		if (!plug.isNull())
			contourLineWidthObjectID = plug.asFloat();

		plug = frGlobalsNode.findPlug("contourLineWidthMaterialID");
		if (!plug.isNull())
			contourLineWidthMaterialID = plug.asFloat();

		plug = frGlobalsNode.findPlug("contourLineWidthShadingNormal");
		if (!plug.isNull())
			contourLineWidthShadingNormal = plug.asFloat();

		plug = frGlobalsNode.findPlug("contourLineWidthUV");
		if (!plug.isNull())
			contourLineWidthUV = plug.asFloat();

		plug = frGlobalsNode.findPlug("contourNormalThreshold");
		if (!plug.isNull())
			contourNormalThreshold = plug.asFloat();

		plug = frGlobalsNode.findPlug("contourUVThreshold");
		if (!plug.isNull())
			contourUVThreshold = plug.asFloat();

		plug = frGlobalsNode.findPlug("contourAntialiasing");
		if (!plug.isNull())
			contourAntialiasing = plug.asFloat();

		plug = frGlobalsNode.findPlug("contourIsDebugEnabled");
		if (!plug.isNull())
			contourIsDebugEnabled = plug.asBool();

		plug = frGlobalsNode.findPlug("deepEXRMergeZThreshold");
		if (!plug.isNull())
			deepEXRMergeZThreshold = plug.asFloat();

		plug = frGlobalsNode.findPlug("cryptomatteExtendedMode");
		if (!plug.isNull())
			cryptomatteExtendedMode = plug.asBool();

		plug = frGlobalsNode.findPlug("cryptomatteSplitIndirect");
		if (!plug.isNull())
			cryptomatteSplitIndirect = plug.asBool();

		plug = frGlobalsNode.findPlug("aovShadowCatcher");
		if (!plug.isNull())
			shadowCatcherEnabled = plug.asBool();

		plug = frGlobalsNode.findPlug("aovReflectionCatcher");
		if (!plug.isNull())
			reflectionCatcherEnabled = plug.asBool();

		plug = frGlobalsNode.findPlug("useOpenCLContext");
		if (!plug.isNull())
			useOpenCLContext = plug.asBool();

		aovs.readFromGlobals(frGlobalsNode);

		readDenoiserParameters(frGlobalsNode);

		readAirVolumeParameters(frGlobalsNode);
	});
}

int FireRenderGlobalsData::getThumbnailIterCount(bool* pSwatchesEnabled)
{
	MObject fireRenderGlobals;
	GetRadeonProRenderGlobals(fireRenderGlobals);

	// Get Fire render globals attributes
	MFnDependencyNode frGlobalsNode(fireRenderGlobals);

	if (pSwatchesEnabled != nullptr)
	{
		MPlug plug = frGlobalsNode.findPlug("enableSwatches");
		if (!plug.isNull())
		{
			*pSwatchesEnabled = plug.asBool();
		}
	}

	MPlug plug = frGlobalsNode.findPlug("thumbnailIterationCount");
	if (!plug.isNull())
	{
		return plug.asInt();
	}
	
	return 0;
}

bool FireRenderGlobalsData::isExrMultichannelEnabled()
{
	MObject fireRenderGlobals;
	GetRadeonProRenderGlobals(fireRenderGlobals);

	// Get Fire render globals attributes
	MFnDependencyNode frGlobalsNode(fireRenderGlobals);

	MPlug plug = frGlobalsNode.findPlug("enableExrMultilayer");
	if (!plug.isNull())
	{
		return plug.asBool();
	}

	return false;
}

void FireRenderGlobalsData::readAirVolumeParameters(const MFnDependencyNode& frGlobalsNode)
{
	MPlug plug = frGlobalsNode.findPlug("airVolumeEnabled");
	if (!plug.isNull())
		airVolumeSettings.airVolumeEnabled = plug.asBool();

	plug = frGlobalsNode.findPlug("fogEnabled");
	if (!plug.isNull())
		airVolumeSettings.fogEnabled = plug.asBool();

	plug = frGlobalsNode.findPlug("fogColor", false);
	if (!plug.isNull())
	{
		MDataHandle colorDataHandle;
		MStatus status0 = plug.getValue(colorDataHandle);
		assert(status0 == MStatus::kSuccess);
		float3& color0 = colorDataHandle.asFloat3();
		airVolumeSettings.fogColor = MColor(color0);
	}


	const MDistance::Unit sceneUnits = MDistance::uiUnit();
	double coeff = (MDistance(1.0, sceneUnits)).asMeters();

	plug = frGlobalsNode.findPlug("fogDistance");
	if (!plug.isNull())
		airVolumeSettings.fogDistance = (float) (plug.asFloat() * coeff);

	plug = frGlobalsNode.findPlug("fogHeight");
	if (!plug.isNull())
		airVolumeSettings.fogHeight = (float) (plug.asFloat() * coeff);

	plug = frGlobalsNode.findPlug("airVolumeDensity");
	if (!plug.isNull())
		airVolumeSettings.airVolumeDensity = plug.asFloat();

	plug = frGlobalsNode.findPlug("airVolumeColor", false);
	if (!plug.isNull())
	{
		MDataHandle colorDataHandle;
		MStatus status0 = plug.getValue(colorDataHandle);
		assert(status0 == MStatus::kSuccess);
		float3& color0 = colorDataHandle.asFloat3();
		airVolumeSettings.airVolumeColor = MColor(color0);
	}

	plug = frGlobalsNode.findPlug("airVolumeClamp");
	if (!plug.isNull())
		airVolumeSettings.airVolumeClamp = plug.asFloat();
}

void FireRenderGlobalsData::readDenoiserParameters(const MFnDependencyNode& frGlobalsNode)
{
	MPlug plug = frGlobalsNode.findPlug("denoiserEnabled");
	if (!plug.isNull())
		denoiserSettings.enabled = plug.asBool();

	plug = frGlobalsNode.findPlug("denoiserType");
	if (!plug.isNull())
		denoiserSettings.type = (FireRenderGlobals::DenoiserType) plug.asInt();

	plug = frGlobalsNode.findPlug("denoiserRadius");
	if (!plug.isNull())
		denoiserSettings.radius = plug.asInt();

	plug = frGlobalsNode.findPlug("denoiserSamples");
	if (!plug.isNull())
		denoiserSettings.samples = plug.asInt();

	plug = frGlobalsNode.findPlug("denoiserFilterRadius");
	if (!plug.isNull())
		denoiserSettings.filterRadius = plug.asInt();

	plug = frGlobalsNode.findPlug("denoiserBandwidth");
	if (!plug.isNull())
		denoiserSettings.bandwidth = plug.asFloat();

	plug = frGlobalsNode.findPlug("denoiserColor");
	if (!plug.isNull())
		denoiserSettings.color = plug.asFloat();

	plug = frGlobalsNode.findPlug("denoiserDepth");
	if (!plug.isNull())
		denoiserSettings.depth = plug.asFloat();

	plug = frGlobalsNode.findPlug("denoiserNormal");
	if (!plug.isNull())
		denoiserSettings.normal = plug.asFloat();

	plug = frGlobalsNode.findPlug("denoiserTrans");
	if (!plug.isNull())
		denoiserSettings.trans = plug.asFloat();

	plug = frGlobalsNode.findPlug("denoiserColorOnly");
	if (!plug.isNull())
		denoiserSettings.colorOnly = plug.asInt() == 0;

	plug = frGlobalsNode.findPlug("enable16bitCompute");
	if (!plug.isNull())
		denoiserSettings.enable16bitCompute = plug.asInt() == 1;

	plug = frGlobalsNode.findPlug("viewportDenoiseUpscaleEnabled");
	if (!plug.isNull())
		denoiserSettings.viewportDenoiseUpscaleEnabled = plug.asInt() == 1;	
}

bool FireRenderGlobalsData::isTonemapping(MString name)
{
	name = GetPropertyNameFromPlugName(name);

	if (name == "toneMappingType")
		return true;
	if (name == "applyGammaToMayaViews")
		return true;
	if (name == "displayGamma")
		return true;
	if (name == "textureGamma")
		return true;

	if (name == "toneMappingWhiteBalanceEnabled")
		return true;
	if (name == "toneMappingWhiteBalanceValue")
		return true;

	if (name == "toneMappingLinearScale")
		return true;

	if (name == "toneMappingPhotolinearSensitivity")
		return true;
	if (name == "toneMappingPhotolinearExposure")
		return true;
	if (name == "toneMappingPhotolinearFstop")
		return true;

	if (name == "toneMappingReinhard02Prescale")
		return true;
	if (name == "toneMappingReinhard02Postscale")
		return true;
	if (name == "toneMappingReinhard02Burn")
		return true;

	return false;
}

bool FireRenderGlobalsData::isDenoiser(MString name)
{
	name = GetPropertyNameFromPlugName(name);

	static const std::vector<MString> propNames = { "denoiserEnabled", "viewportDenoiseUpscaleEnabled", "denoiserType", "denoiserRadius", "denoiserSamples",
		"denoiserFilterRadius", "denoiserBandwidth", "denoiserColor", "denoiserDepth", "denoiserNormal", "denoiserTrans" };

	for (const MString& propName : propNames)
	{
		if (propName == name)
		{
			return true;
		}
	}

	return false;
}

bool FireRenderGlobalsData::IsMotionBlur(MString name)
{
	name = GetPropertyNameFromPlugName(name);

	static const std::set<std::string> propNames { "motionBlur", "cameraMotionBlur", "motionBlurCameraExposure", "viewportMotionBlur", "velocityAOVMotionBlur"};

	return propNames.find(name.asChar()) != propNames.end();
}

bool FireRenderGlobalsData::IsAirVolume(MString name)
{
	name = GetPropertyNameFromPlugName(name);

	static const std::set<std::string> propNames{ 
		"airVolumeEnabled", "fogEnabled", "fogColor", "fogDistance", "fogHeight", "airVolumeDensity", "airVolumeColor", "airVolumeClamp" 
	};

	return propNames.find(name.asChar()) != propNames.end();
}

void FireRenderGlobalsData::getCPUThreadSetup(bool& overriden, int& cpuThreadCount, RenderType renderType)
{
	// Apply defaults in case if global node isn't created yet
	overriden = false;
	cpuThreadCount = 0;

	MObject fireRenderGlobals;
	GetRadeonProRenderGlobals(fireRenderGlobals);
	// Get Fire render globals attributes
	MFnDependencyNode frGlobalsNode(fireRenderGlobals);

	MString overrideName = "overrideCpuThreadCountFinalRender";
	MString threadCountName = "cpuThreadCountFinalRender";

	if (renderType != RenderType::ProductionRender)
	{
		overrideName = "overrideCpuThreadCountViewport";
		threadCountName = "cpuThreadCountViewport";
	}

	MPlug plug = frGlobalsNode.findPlug(overrideName);
	if (!plug.isNull())
	{
		overriden = plug.asBool();
	}

	plug = frGlobalsNode.findPlug(threadCountName);
	if (!plug.isNull())
	{
		cpuThreadCount = plug.asInt();
	}
}

int FireMaya::Options::GetContextDeviceFlags(RenderType renderType)
{
	int ret = FireRenderThread::RunOnMainThread<int>([=]
	{
		int ret = 0;
		MIntArray devicesUsing;

		// currently we support only 2 sets of settings. finalRender and viewport (IPR)
		MString cmd = getOptionVarMelCommand("-q", renderType == RenderType::ProductionRender ?
			FINAL_RENDER_DEVICES_USING_PARAM_NAME : VIEWPORT_DEVICES_USING_PARAM_NAME, "");

		MGlobal::executeCommand(cmd, devicesUsing);
		auto allDevices = HardwareResources::GetAllDevices();

		for (auto i = 0u; i < devicesUsing.length(); i++)
		{
			if (devicesUsing[i] && i < allDevices.size())
				ret |= allDevices[i].creationFlag;
		}

		if (!ret || (devicesUsing.length() > (unsigned int) allDevices.size() && devicesUsing[ (unsigned int) allDevices.size()]))
		{
			ret |= RPR_CREATION_FLAGS_ENABLE_CPU;
		}

		return ret;
	});

	return ret;
}


MString GetPropertyNameFromPlugName(const MString& name)
{
	MString outName;
	int dotIndex = name.index('.');
	outName = name.substring(dotIndex + 1, name.length());

	return outName;
}

bool isVisible(MFnDagNode & fnDag, MFn::Type type)
{
	MObject currentLayerObj = MFnRenderLayer::currentLayer();
	MFnRenderLayer renderLayer(currentLayerObj);
	MDagPath p;
	fnDag.getPath(p);

	//check render layer
	if (fnDag.object().hasFn(type))
	{
		if (!renderLayer.inCurrentRenderLayer(p))
		{
			return false;
		}
	}

	// check intermediate
	if (fnDag.isIntermediateObject())
	{
		return false;
	}

	// check renderable
	MPlug primVisPlug = fnDag.findPlug("primaryVisibility");
	if ((!primVisPlug.isNull()) && (!primVisPlug.asBool()))
	{
		return false;
	}

	// check override
	MPlug overridePlug = fnDag.findPlug("overrideEnabled");
	if (!overridePlug.isNull())
	{
		if (overridePlug.asBool())
		{
			MPlug overrideVisPlug = fnDag.findPlug("overrideVisibility");
			if ((!overridePlug.isNull()) && (!overrideVisPlug.asBool()))
			{
				return false;
			}
		}
	}

	// check visibility
	MPlug visPlug = fnDag.findPlug("visibility");
	if ((!visPlug.isNull()) && (!visPlug.asBool()))
	{
		return false;
	}

	// check parents
	for (unsigned int i = 0; i < fnDag.parentCount(); i++)
	{
		MFnDagNode dnParent(fnDag.parent(i));
		if (dnParent.dagRoot() == fnDag.parent(i))
		{
			break;
		}
		if (!isVisible(dnParent, type))
		{
			return false;
		}
	}
	return true;
}

bool isTransformWithInstancedShape(const MObject& node, MDagPath& nodeDagPath, bool& isGPUCacheNode)
{
	MDagPathArray pathArrayToTransform;
	{
		// get path to node. node is a transform
		MFnDagNode transformNode(node);
		transformNode.getAllPaths(pathArrayToTransform);
	}

	int pathArrayLength = pathArrayToTransform.length();
	if (pathArrayLength == 0)
		return false;

	// get shape (referenced by transform)
	MStatus status = pathArrayToTransform[0].extendToShape();

	if (status != MStatus::kSuccess)
	{
		return false;
	}

	MFnDagNode shapeNode(pathArrayToTransform[0].node());
	bool isInstanced = shapeNode.isInstanced();

	MString typeName = shapeNode.typeName();
	if (typeName == "gpuCache")
	{
		isGPUCacheNode = true;
	}
	
	// more than one reference to shape exist => shape is instanced
	if (isInstanced)
	{
		nodeDagPath = pathArrayToTransform[0];
		return true;
	}

	// only one reference => shape is not instanced
	return false;
}

bool isGeometry(const MObject& node)
{
	return
		node.hasFn(MFn::kMesh) ||
		node.hasFn(MFn::kNurbsSurface) ||
		node.hasFn(MFn::kSubdiv);
}

bool isLight(const MObject& node)
{
	return node.hasFn(MFn::kLight) || 
		MFnDependencyNode(node).typeId() == FireMaya::TypeId::FireRenderPhysicalLightLocator;
}

double rad2deg(double radians)
{
	return (180.0 / M_PI)*radians;
}

double deg2rad(double degrees)
{
	return (M_PI / 180.0)*degrees;
}

// Finds Mesh node on the same level as Light transform
bool findMeshShapeForMeshPhysicalLight(const MDagPath&  physicalLightPath, MDagPath& shapeDagPath)
{
	MDagPath path = physicalLightPath;

	// Search for 1 level above and than iterating throught children to find mesh
	MStatus status = path.pop(1);

	if (status != MStatus::kSuccess)
	{
		return false;
	}

	int c = path.childCount();

	for (int i = 0; i < c; i++)
	{
		if (path.child(i).hasFn(MFn::kMesh))
		{
			path.push(path.child(i));
			shapeDagPath = path;
			return true;
		}
	}

	return false;
}

MStatus getVisibleObjects(MDagPathArray& objects, MFn::Type type, bool checkVisibility)
{
	MStatus status = MStatus::kSuccess;
	MItDag itDag(MItDag::kDepthFirst, type, &status);
	if (MStatus::kFailure == status) {
		MGlobal::displayError("MItDag::MItDag");
		return status;
	}

	for (; !itDag.isDone(); itDag.next())
	{
		MDagPath dagPath;
		status = itDag.getPath(dagPath);
		if (MStatus::kSuccess != status)
		{
			MGlobal::displayError("MDagPath::getPath");
			break;
		}

		if (!checkVisibility || dagPath.isVisible())
			objects.append(dagPath);
	}

	return status;
}

void ProcessConnectedShader(const MPlug& shaderPlug, MObjectArray& shaders)
{
	if (shaderPlug.isNull())
		return;

	MPlugArray shaderConnections;
	shaderPlug.connectedTo(shaderConnections, true, false);
	if (shaderConnections.length() == 0)
		return;

	MObject shaderNode = shaderConnections[0].node();
	shaders.append(shaderNode);
}

MObjectArray getConnectedShaders(MDagPath& meshPath)
{
	MObjectArray shaders;
	MFnDependencyNode nodeFn(meshPath.node());
	MPlugArray connections;
	nodeFn.getConnections(connections);
	if (connections.length() == 0)
		return shaders;

	for (MPlug conn : connections)
	{
		MPlugArray outConnections;
		conn.connectedTo(outConnections, false, true);
		if (outConnections.length() == 0)
			continue;
		conn = outConnections[0];
		if (conn.node().apiType() != MFn::kShadingEngine)
		{
			continue;
		}

		MFnDependencyNode sgNode(conn.node());

		MPlug surfShaderPlug = sgNode.findPlug("surfaceShader");
		if (!surfShaderPlug.isNull())
			ProcessConnectedShader(surfShaderPlug, shaders);

		MPlug volShaderPlug = sgNode.findPlug("volumeShader");
		if (!volShaderPlug.isNull())
			ProcessConnectedShader(volShaderPlug, shaders);
	}
	return shaders;
}

MObjectArray getConnectedMeshes(MObject& shaderNode)
{
	MObjectArray meshes;
	MFnDependencyNode nodeFn(shaderNode);

	MPlug outColorPlug = nodeFn.findPlug("outColor");
	MPlugArray connections;
	outColorPlug.connectedTo(connections, false, true);
	if (connections.length() == 0)
		return meshes;

	for (MPlug conn : connections)
	{
		if (conn.node().apiType() != MFn::kShadingEngine)
		{
			continue;
		}
		MFnDependencyNode sgNode(conn.node());
		MPlug membersPlug = sgNode.findPlug("dagSetMembers");
		if (membersPlug.isNull())
			continue;
		for (unsigned int j = 0; j < membersPlug.numConnectedElements(); j++)
		{
			MPlug setMemPlug = membersPlug.connectionByPhysicalIndex(j);
			if (setMemPlug.isNull())
				continue;

			MPlugArray meshConnections;
			setMemPlug.connectedTo(meshConnections, true, false);
			if (meshConnections.length() == 0)
				continue;
			MObject meshNode = meshConnections[0].node();
			meshes.append(meshNode);
		}
	}
	return meshes;
}

MObject getNodeFromUUid(const std::string& uuid)
{
	MStatus status;
	MString id = uuid.c_str();


	int searchType[5] = { MFn::kCamera, MFn::kMesh, MFn::kLight, MFn::kTransform, MFn::kInvalid };

	for (unsigned int i = 0; i < 5; i++)
	{
		MItDag itDagCam(MItDag::kDepthFirst, MFn::Type(searchType[i]), &status);
		for (; !itDagCam.isDone(); itDagCam.next())
		{
			MDagPath dagPath;
			status = itDagCam.getPath(dagPath);
			if (status != MS::kSuccess)
				continue;

			MFnDagNode visTester(dagPath);
#ifndef MAYA2015
			if (visTester.uuid().asString() == id)
			{
				return dagPath.node();
			}
#else
			MFnDependencyNode nodeFn(dagPath.node());
			MPlug uuidPlug = nodeFn.findPlug("fruuid");
			if (uuidPlug.isNull())
			{
				MObject dagPathNode = dagPath.node();
				getNodeUUid(dagPathNode);
				uuidPlug = nodeFn.findPlug("fruuid");
			}
			MString uuid;
			uuidPlug.getValue(uuid);
			if (uuid == id)
			{
				return dagPath.node();
			}
#endif
		}
	}
	return MObject();
}

std::string getNodeUUid(const MObject& node)
{
	MFnDependencyNode nodeFn(node);

	std::string id = nodeFn.uuid().asString().asChar();

	if (node.hasFn(MFn::kDagNode))
	{
		MFnDagNode dagNode(node);
		if (dagNode.isFromReferencedFile())
		{
			// Referenced node name guaranteed to be unique by Maya. User can't change it's name because node is locked.
			std::stringstream sstrm;
			sstrm << id.c_str() << ":" << dagNode.fullPathName();
			id = sstrm.str();
		}
	}

	return id;
}

std::string getNodeUUid(const MDagPath& node)
{
	auto id = getNodeUUid(node.node());

	if (node.isInstanced() && (node.instanceNumber() > 0))
	{
		std::stringstream sstrm;
		sstrm << id.c_str() << ":" << node.instanceNumber();
		id = sstrm.str();
	}

	return id;
}

MDagPath getEnvLightObject(MStatus& status)
{
	MDagPath out;
	MItDag itDag(MItDag::kDepthFirst);
	for (; !itDag.isDone(); itDag.next())
	{
		MDagPath dagPath;
		status = itDag.getPath(dagPath);
		if (MStatus::kSuccess != status) {
			MGlobal::displayError("MDagPath::getPath");
			return out;
		}

		MFnDagNode visTester(dagPath);

		if (visTester.typeId() != FireMaya::TypeId::FireRenderIBL &&
			visTester.typeId() != FireMaya::TypeId::FireRenderEnvironmentLight &&
			visTester.typeName() != "ambientLight")
		{
			continue;
		}

		if (dagPath.isVisible()) {
			return dagPath;
		}
	}
	return out;
}

MDagPath getSkyObject(MStatus& status)
{
	MDagPath out;
	MItDag itDag(MItDag::kDepthFirst);
	for (; !itDag.isDone(); itDag.next())
	{
		MDagPath dagPath;
		status = itDag.getPath(dagPath);
		if (MStatus::kSuccess != status) {
			MGlobal::displayError("MDagPath::getPath");
			return out;
		}

		MFnDagNode visTester(dagPath);

		if (visTester.typeId() != FireMaya::TypeId::FireRenderSkyLocator)
		{
			continue;
		}

		if (dagPath.isVisible()) {
			return dagPath;
		}
	}
	return out;
}

MString getShaderCachePath()
{
#ifdef WIN32
	PWSTR sz = nullptr;
	if (S_OK == ::SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &sz))
	{
		wchar_t suffix[128 + 1] = {};
#ifdef RPR_VERSION_MAJOR_MINOR_REVISION
		wsprintfW(suffix, L"%x", RPR_VERSION_MAJOR_MINOR_REVISION);
#else
		wsprintfW(suffix, L"%x", RPR_API_VERSION);
#endif

		std::wstring cacheFolder(sz);
		cacheFolder += L"\\RadeonProRender\\Maya\\ShaderCache\\API_";
		cacheFolder += suffix;
		switch (SHCreateDirectoryExW(nullptr, cacheFolder.c_str(), nullptr))
		{
		case ERROR_SUCCESS:
		case ERROR_FILE_EXISTS:
		case ERROR_ALREADY_EXISTS:
			cacheFolder += L"\\";
			return cacheFolder.c_str();
		}
	}
	return MGlobal::executeCommandStringResult("getenv FR_SHADER_CACHE_PATH");
#elif defined(OSMac_)
	return "/Users/Shared/RadeonProRender/cache"_ms;
#else
	return MGlobal::executeCommandStringResult("getenv FR_SHADER_CACHE_PATH");
#endif
}

std::string replaceStrChar(std::string str, const std::string& replace, char ch) {

	// set our locator equal to the first appearance of any character in replace
	size_t found = str.find_first_of(replace);

	while (found != std::string::npos) { // While our position in the sting is in range.
		str[found] = ch; // Change the character at position.
		found = str.find_first_of(replace, found + 1); // Relocate again.
	}

	return str; // return our new string.
}

int areShadersCached() 
{
	std::string temp = getShaderCachePath().asChar();
	temp = replaceStrChar(temp, "\\", '/');
	MString test = "source common.mel; getNumberOfCachedShaders(\"" + MString(temp.c_str()) + "\");";
	int result = 0;
	MGlobal::executeCommand(test, result);
	return result != 0;
}

MString getLogFolder()
{
    MString path = MGlobal::executeCommandStringResult("getenv RPR_MAYA_TRACE_PATH");
    if (path.length() == 0)
    {
        static char buff[256 + 1] = {};
        if (!buff[0])
        {
            auto t = time(NULL);
            auto tm = localtime(&t);
            sprintf(buff, "%02d.%02d.%02d-%02d.%02d", tm->tm_year - 100, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min);
        }

#ifdef WIN32
        std::string folderPart(buff);

        PWSTR sz = nullptr;
        if (S_OK == ::SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &sz))
        {
            auto cacheFolder = std::wstring(sz) + L"\\RadeonProRender\\Maya\\Trace\\" + std::wstring(folderPart.begin(), folderPart.end());
            switch (SHCreateDirectoryExW(nullptr, cacheFolder.c_str(), nullptr))
            {
            case ERROR_SUCCESS:
            case ERROR_FILE_EXISTS:
            case ERROR_ALREADY_EXISTS:
                path = cacheFolder.c_str();
            }
        }
        
#elif defined(OSMac_)
        path = "/Users/Shared/RadeonProRender/trace/" + MString(buff);
        MCommonSystemUtils::makeDirectory(path);
#endif
        
    }
    return path;
}

MString getSourceImagesPath()
{
	MString fileRule;
	MString sourceImagesPath = "sourceimages";

	MString fileRuleCommand = "workspace -q -fileRuleEntry \"sourceImages\"";
	MGlobal::executeCommand(fileRuleCommand, fileRule);

	if (fileRule.length() > 0)
	{
		MString directoryCommand = "workspace -en `workspace -q -fileRuleEntry \"sourceImages\"`";
		MGlobal::executeCommand(directoryCommand, sourceImagesPath);
	}

	MFileObject directory;
	directory.setRawFullName(sourceImagesPath);
	return directory.resolvedFullName();
}

MObject getSurfaceShader(MObject& shadingEngine)
{
	MFnDependencyNode sgNode(shadingEngine);
	MPlug surfShaderPlug = sgNode.findPlug("surfaceShader");
	if (surfShaderPlug.isNull())
		return MObject::kNullObj;
	MPlugArray shaderConnections;
	surfShaderPlug.connectedTo(shaderConnections, true, false);
	if (shaderConnections.length() == 0)
		return MObject::kNullObj;
	MObject shaderNode = shaderConnections[0].node();
	return shaderNode;
}

MObject getVolumeShader(MObject& shadingEngine)
{
	MFnDependencyNode sgNode(shadingEngine);
	MPlug surfShaderPlug = sgNode.findPlug("volumeShader");
	if (surfShaderPlug.isNull())
		return MObject::kNullObj;
	MPlugArray shaderConnections;
	surfShaderPlug.connectedTo(shaderConnections, true, false);
	if (shaderConnections.length() == 0)
		return MObject::kNullObj;
	MObject shaderNode = shaderConnections[0].node();
	return shaderNode;
}

MObject getDisplacementShader(MObject& shadingEngine)
{
	MFnDependencyNode sgNode(shadingEngine);
	MPlug surfShaderPlug = sgNode.findPlug("displacementShader");
	if (surfShaderPlug.isNull())
		return MObject::kNullObj;
	MPlugArray shaderConnections;
	surfShaderPlug.connectedTo(shaderConnections, true, false);
	if (shaderConnections.length() == 0)
		return MObject::kNullObj;
	MObject shaderNode = shaderConnections[0].node();
	return shaderNode;
}

MObjectArray GetShadingEngines(MFnDagNode& mesh, uint instanceNum)
{
	MObjectArray shadingEngines;
	MPlug iog = mesh.findPlug("instObjGroups");
	MPlug ogrp = iog.elementByPhysicalIndex(instanceNum < iog.numElements() ? instanceNum : 0);

	// check unique shader
	MPlugArray connections;
	ogrp.connectedTo(connections, false, true);
	if (connections.length() > 0)
	{
		for (auto conn : connections)
		{
			MObject sgNode = conn.node();
			if (sgNode.hasFn(MFn::kShadingEngine))
				shadingEngines.append(sgNode);
		}
	}
	else
	{
		MPlug ogrps = ogrp.child(0);
		for (unsigned int i = 0; i < ogrps.numElements(); i++)
		{
			MPlug grp = ogrps.elementByPhysicalIndex(i);
			MPlug glist = grp.child(0);
			MPlug gId = grp.child(1);

			MObject flistO = glist.asMObject();
			MFnComponentListData lData(flistO);
			if (lData.length() == 0)
			{
				continue;
			}
			MFnSingleIndexedComponent compFn(lData[0]);
			MIntArray elements;
			compFn.getElements(elements);

			MPlugArray gConnections;
			grp.connectedTo(gConnections, false, true);
			if (gConnections.length() > 0)
			{
				for (auto conn : gConnections)
				{
					MObject sgNode = conn.node();
					if (sgNode.hasFn(MFn::kShadingEngine))
						shadingEngines.append(sgNode);
				}
			}
		}
	}

	// if no shader, attach the initialShadingGroup
	if (shadingEngines.length() == 0)
	{
		MSelectionList list;
		list.add("initialShadingGroup");
		MObject node;
		list.getDependNode(0, node);
		shadingEngines.append(node);
	}

	return shadingEngines;
}

int GetFaceMaterials(MFnMesh& mesh, MIntArray& faceList)
{
#ifdef OPTIMIZATION_CLOCK
	std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
#endif

	MPlug iog = mesh.findPlug("instObjGroups");
	MPlug ogrp = iog.elementByPhysicalIndex(0);

	// init to polygon count and set default
	faceList = MIntArray(mesh.numPolygons(), 0);

	MPlugArray connections;
	ogrp.connectedTo(connections, false, true);

	int shadingEngines = 0;
	if (connections.length() == 0)
	{
		MPlug ogrps = ogrp.child(0);
		for (unsigned int i = 0; i < ogrps.numElements(); i++)
		{
			MPlug grp = ogrps.elementByPhysicalIndex(i);
			MPlug glist = grp.child(0);
			MPlug gId = grp.child(1);

			MObject flistO = glist.asMObject();
			MFnComponentListData lData(flistO);
			if (lData.length() == 0)
				continue;

			MFnSingleIndexedComponent compFn(lData[0]);
			MIntArray elements;
			compFn.getElements(elements);

			// we have a group and its elements
			MPlugArray gConnections;
			grp.connectedTo(gConnections, false, true);
			for (auto conn : gConnections)
			{
				// now find first attached shading engine
				if (conn.node().hasFn(MFn::kShadingEngine))
				{
					if (shadingEngines > 0)	// only need to write new values if non-zero
					{
						for (auto el : elements)
							faceList[el] = shadingEngines;
					}

					shadingEngines++;
					break;
				}
			}
		}
	}

#ifdef OPTIMIZATION_CLOCK
	std::chrono::steady_clock::time_point fin = std::chrono::steady_clock::now();
	std::chrono::microseconds elapsed = std::chrono::duration_cast<std::chrono::microseconds>(fin - start);
	FireRenderContext::inGetFaceMaterials += elapsed.count();
#endif

	//if no shaders encountered then consider it 1
	return shadingEngines ? shadingEngines : 1;
}

MString getNameByDagPath(const MDagPath& cameraPath)
{
	MDagPath path = cameraPath;
	path.extendToShape();
	MObject transform = path.transform();
	MFnDependencyNode transfNode(transform);
	return transfNode.name();
}

MDagPath getDefaultCamera()
{
	MDagPath result;
	MStatus status;

	MItDag itDag(MItDag::kDepthFirst, MFn::kCamera, &status);
	if (MStatus::kFailure == status) 
	{
		MGlobal::displayError("getDefaultCamera: no cameras in te scene!");
		return result;
	}

	for (; !itDag.isDone(); itDag.next())
	{
		MDagPath dagPath;
		status = itDag.getPath(dagPath);
		if (MStatus::kSuccess != status) 
		{
			MGlobal::displayError("getDefaultCamera: traverse failed");
			continue;
		}

		MFnDagNode cameraNode(dagPath);
		std::string cameraName = cameraNode.fullPathName().asChar();
		if (cameraName.find("|persp|") != std::string::npos) // "persp" is the name that is hardcoded in PR command script at the moment; if we will change PR script we should change this code as well
		{
			result = dagPath;
			return result;
		}
	}

	MGlobal::displayError("getDefaultCamera: failed to find persp camera!");
	return result;
}

MDagPathArray GetSceneCameras(bool renderableOnly /*= false*/)
{
	MDagPathArray cameras;
	MStatus status;

	MItDag itDag(MItDag::kDepthFirst, MFn::kCamera, &status);
	if (MStatus::kFailure == status) 
	{
		MGlobal::displayError("MItDag::MItDag");
		return cameras;
	}

	for (; !itDag.isDone(); itDag.next())
	{
		MDagPath dagPath;
		status = itDag.getPath(dagPath);

		if (MStatus::kSuccess != status) 
		{
			MGlobal::displayError("MDagPath::getPath");
			continue;
		}

		if (renderableOnly)
		{
			MFnDependencyNode camFn(dagPath.node());
			bool renderable = false;
			MPlug renderablePlug = camFn.findPlug("renderable");
			if (!renderablePlug.isNull())
			{
				renderablePlug.getValue(renderable);
			}

			if (!renderable)
				continue;
		}

		cameras.append(dagPath);
	}

	return cameras;
}

void GetResolutionFromCommonTab(unsigned int& width, unsigned int& height)
{
	MObject defaultResolutionObject;
	MSelectionList slist;
	slist.add("defaultResolution");
	slist.getDependNode(0, defaultResolutionObject);

	MFnDependencyNode globalsNode(defaultResolutionObject);
	MPlug plug = globalsNode.findPlug("width");
	if (!plug.isNull())
	{
		width = plug.asInt();
	}

	plug = globalsNode.findPlug("height");
	if (!plug.isNull())
	{
		height = plug.asInt();
	}
}

MStatus GetDefaultRenderGlobals(MObject& outGlobalsNode)
{
	MSelectionList slist;
	slist.add("defaultRenderGlobals");
	return slist.getDependNode(0, outGlobalsNode);
}
MStatus GetRadeonProRenderGlobals(MObject& outGlobalsNode)
{
	MSelectionList selList;
	selList.add("RadeonProRenderGlobals");
	return selList.getDependNode(0, outGlobalsNode);
}

MPlug GetRadeonProRenderGlobalsPlug(const char* name, MStatus* outStatus)
{
	MObject objGlobals;
	MStatus status = GetRadeonProRenderGlobals(objGlobals);
	CHECK_MSTATUS(status);

	MPlug plug;

	if (status == MStatus::kSuccess)
	{
		MFnDependencyNode node(objGlobals);
		plug = node.findPlug(name, &status);
	}

	if (outStatus != nullptr)
	{
		*outStatus = status;
	}

	return plug;
}

bool IsFlipIBL()
{
	MPlug flipPlug = GetRadeonProRenderGlobalsPlug("flipIBL");

	if (!flipPlug.isNull())
	{
		return flipPlug.asBool();
	}

	return false;
}

MObject findDependNode(MString name)
{
	MSelectionList selectionList;
	selectionList.add(name);
	MObject obj;
	selectionList.getDependNode(0, obj);

	return obj;
}


const HardwareResources& HardwareResources::GetInstance()
{
	static HardwareResources theInstance;
	return theInstance;
}


std::vector<HardwareResources::Device> HardwareResources::GetCompatibleCPUs()
{
	std::vector<HardwareResources::Device> ret;
	auto self = GetInstance();
	for (auto it : self._devices)
	{
		if (it.creationFlag == RPR_CREATION_FLAGS_ENABLE_CPU)
			ret.push_back(it);
	}
	return ret;
}

std::vector<HardwareResources::Device> HardwareResources::GetCompatibleGPUs(bool onlyCertified)
{
	std::vector<HardwareResources::Device> ret;
	auto self = GetInstance();

	for (auto it : self._devices)
	{
		if (it.creationFlag == RPR_CREATION_FLAGS_ENABLE_CPU)
			continue;

		// Check for an incompatible driver.
		if (!self.isDriverCompatible(it.name))
		{
			MString message = "The driver for device '";
			message += it.name.c_str();
			message += "' does not meet the minimum requirements for use with Radeon ProRender.";
			MGlobal::displayWarning(message);

			it.isDriverCompatible = false;
		}

		if (!onlyCertified || it.compatibility == RPRTC_COMPATIBLE)
			ret.push_back(it);
	}

	return ret;
}

std::vector<HardwareResources::Device> HardwareResources::GetAllDevices()
{
	auto self = GetInstance();
	return self._devices;
}

HardwareResources::HardwareResources()
{
	if (std::getenv("RPR_MAYA_DISABLE_GPU"))
	{
		return;
	}

	static const int creationFlagsGPU[] =
	{
		RPR_CREATION_FLAGS_ENABLE_GPU0,
		RPR_CREATION_FLAGS_ENABLE_GPU1,
		RPR_CREATION_FLAGS_ENABLE_GPU2,
		RPR_CREATION_FLAGS_ENABLE_GPU3,
		RPR_CREATION_FLAGS_ENABLE_GPU4,
		RPR_CREATION_FLAGS_ENABLE_GPU5,
		RPR_CREATION_FLAGS_ENABLE_GPU6,
		RPR_CREATION_FLAGS_ENABLE_GPU7,
		RPR_CREATION_FLAGS_ENABLE_CPU,
	};

	static const int nameFlags[] =
	{
		RPR_CONTEXT_GPU0_NAME,
		RPR_CONTEXT_GPU1_NAME,
		RPR_CONTEXT_GPU2_NAME,
		RPR_CONTEXT_GPU3_NAME,
		RPR_CONTEXT_GPU4_NAME,
		RPR_CONTEXT_GPU5_NAME,
		RPR_CONTEXT_GPU6_NAME,
		RPR_CONTEXT_GPU7_NAME,
		RPR_CONTEXT_CPU_NAME
	};

	const int maxDevices = 9;
#ifdef OSMac_
	auto os = RPRTOS_MACOS;
#elif __linux__
	auto os = RPRTOS_LINUX;
#else
	auto os = RPRTOS_WINDOWS;
#endif

	for (int i = 0; i < maxDevices; ++i)
	{
		Device device;
		device.creationFlag = creationFlagsGPU[i];
        rpr_creation_flags additionalFlags = 0;
        bool doWhiteList = true;
        if (isMetalOn() && !(device.creationFlag & RPR_CREATION_FLAGS_ENABLE_CPU))
        {
            additionalFlags = additionalFlags | RPR_CREATION_FLAGS_ENABLE_METAL;
            doWhiteList = false;
        }

		int tahoeID = NorthStarContext::GetPluginID();

        device.creationFlag = device.creationFlag | additionalFlags;
		device.compatibility = rprIsDeviceCompatible(tahoeID, RPR_TOOLS_DEVICE(i), nullptr, doWhiteList, os, additionalFlags );
		if (device.compatibility == RPRTC_INCOMPATIBLE_UNCERTIFIED || device.compatibility == RPRTC_COMPATIBLE)
		{
			// Request device name. Earlier (before RPR 1.255) rprIsDeviceCompatible returned device name, however
			// this functionality was removed. We're doing "assert" here just in case because rprIsDeviceCompatible
			// does exactly the same right before this code.
			
			rpr_int plugins[] = { tahoeID };
			size_t pluginCount = sizeof(plugins) / sizeof(plugins[0]);
			rpr_context temporaryContext = 0;

			std::vector<rpr_context_properties> ctxProperties;
			ctxProperties.push_back((rpr_context_properties)RPR_CONTEXT_PRECOMPILED_BINARY_PATH);
			std::string hipbinPath = GetPathToHipbinFolder();
			ctxProperties.push_back((rpr_context_properties)hipbinPath.c_str());
			ctxProperties.push_back((rpr_context_properties)0);

#ifdef RPR_VERSION_MAJOR_MINOR_REVISION
			rpr_int status = rprCreateContext(RPR_VERSION_MAJOR_MINOR_REVISION, plugins, pluginCount, device.creationFlag , ctxProperties.data(), NULL, &temporaryContext);
#else
			rpr_int status = rprCreateContext(RPR_API_VERSION, plugins, pluginCount, device.creationFlag, NULL, NULL, &temporaryContext);
#endif
			assert(status == RPR_SUCCESS);
			size_t size = 0;
			status = rprContextGetInfo(temporaryContext, nameFlags[i], 0, 0, &size);
			assert(status == RPR_SUCCESS);
			char* deviceName = new char[size];
			status = rprContextGetInfo(temporaryContext, nameFlags[i], size, deviceName, 0);
			assert(status == RPR_SUCCESS);
			status = rprObjectDelete(temporaryContext);
			assert(status == RPR_SUCCESS);

#if defined(OSMac_)
            printf("### Radeon ProRender Device %s\n",deviceName);
#endif
            
			// Register the device.
			device.name = deviceName;
			delete[] deviceName;
			_devices.push_back(device);
		}
	}

	initializeDrivers();
}

int toInt(const std::wstring &s)
{
	wchar_t* end_ptr;
	long val = wcstol(s.c_str(), &end_ptr, 10);
	if (*end_ptr)
		throw "invalid_string";

	if ((val == LONG_MAX || val == LONG_MIN) && errno == ERANGE)
		throw "overflow";

	return val;
}

std::vector<std::wstring> split(const std::wstring &s, wchar_t delim)
{
	std::vector<std::wstring> elems;
	for (size_t p = 0, q = 0; p != s.npos; p = q)
		elems.push_back(s.substr(p + (p != 0), (q = s.find(delim, p + 1)) - p - (p != 0)));

	return elems;
}

void HardwareResources::initializeDrivers()
{
#ifdef _WIN32
	populateDrivers();
	updateDriverCompatibility();
#endif
}

void HardwareResources::updateDriverCompatibility()
{
	std::wstring retmessage = L"";

	// This check only supports Windows.
	// Return true for other operating systems.
	const int Supported_AMD_driverMajor = 15;
	const int Supported_AMD_driverMinor = 301;

	const std::wstring Supported_NVIDIA_driver_string = L"368.39";
	const int Supported_NVIDIA_driver = 36839;

	// Examples of VideoController_DriverVersion:
	// "15.301.2601.0"  --> for AMD
	// "10.18.13.5456"  --> for NVIDIA ( the 5 last figures correspond to the public version name, here it's 354.56 )
	// "9.18.13.697"    --> for NVIDIA ( public version will be 306.97 )

	// Examples of VideoController_Name :
	// "AMD FirePro W8000"
	// "NVIDIA Quadro 4000"
	// "NVIDIA GeForce GT 540M"

	try
	{
		for (Driver& driver : _drivers)
		{
			const std::wstring &driverName = driver.name;
			const std::wstring &deviceName = driver.deviceName;

			// Process AMD drivers.
			if (deviceName.find(L"AMD") != std::string::npos)
			{
				driver.isAMD = true;

				if (driverName.length() >= std::wstring(L"XX.XXX").length() && driverName[2] == '.')
				{
					const std::wstring strVersionMajor = driverName.substr(0, 2);
					const std::wstring strVersionMinor = driverName.substr(3, 3);

					int VersionMajorInt = toInt(strVersionMajor);
					int VersionMinorInt = toInt(strVersionMinor);

					// Driver is incompatible if the major version is too low.
					if (VersionMajorInt < Supported_AMD_driverMajor)
						driver.compatible = false;

					// Driver is incompatible if the major version is okay, but the minor version is too low.
					else if (VersionMajorInt == Supported_AMD_driverMajor && VersionMinorInt < Supported_AMD_driverMinor)
						driver.compatible = false;

					// The driver is compatible.
					else
						break;
				}
			}

			// Process NVIDIA drivers.
			else if (deviceName.find(L"NVIDIA") != std::string::npos)
			{
				driver.isNVIDIA = true;

				int nvidiaPublicDriver = 0;
				bool successParseNVdriver = parseNVIDIADriver(driverName, nvidiaPublicDriver);

				if (successParseNVdriver)
				{
					// Driver is incompatible.
					if (nvidiaPublicDriver < Supported_NVIDIA_driver)
						driver.compatible = false;

					// Driver is compatible.
					else
						break;
				}
			}
		}
	}
	catch (...)
	{
		// NVidia driver parse exception.
	}
}

void HardwareResources::populateDrivers()
{
#ifdef _WIN32
	HRESULT hres;

	// Obtain the initial locator to WMI -------------------------
	IWbemLocator *pLoc = NULL;

	hres = CoCreateInstance(
		CLSID_WbemLocator,
		0,
		CLSCTX_INPROC_SERVER,
		IID_IWbemLocator, (LPVOID *)&pLoc);

	if (FAILED(hres))
		return;


	// Connect to WMI through the IWbemLocator::ConnectServer method

	IWbemServices *pSvc = NULL;

	// Connect to the root\cimv2 namespace with
	// the current user and obtain pointer pSvc
	// to make IWbemServices calls.
	hres = pLoc->ConnectServer(
		_bstr_t(L"ROOT\\CIMV2"), // Object path of WMI namespace
		NULL,                    // User name. NULL = current user
		NULL,                    // User password. NULL = current
		0,                       // Locale. NULL indicates current
		NULL,                    // Security flags.
		0,                       // Authority (for example, Kerberos)
		0,                       // Context object
		&pSvc                    // pointer to IWbemServices proxy
	);

	if (FAILED(hres))
	{
		pLoc->Release();
		return;
	}

	// Set security levels on the proxy -------------------------

	hres = CoSetProxyBlanket(
		pSvc,                        // Indicates the proxy to set
		RPC_C_AUTHN_WINNT,           // RPC_C_AUTHN_xxx
		RPC_C_AUTHZ_NONE,            // RPC_C_AUTHZ_xxx
		NULL,                        // Server principal name
		RPC_C_AUTHN_LEVEL_CALL,      // RPC_C_AUTHN_LEVEL_xxx
		RPC_C_IMP_LEVEL_IMPERSONATE, // RPC_C_IMP_LEVEL_xxx
		NULL,                        // client identity
		EOAC_NONE                    // proxy capabilities
	);

	if (FAILED(hres))
	{
		pSvc->Release();
		pLoc->Release();
		return;
	}

	// Use the IWbemServices pointer to make requests of WMI ----

	// For example, get the name of the operating system
	IEnumWbemClassObject* pEnumerator = NULL;
	hres = pSvc->ExecQuery(
		bstr_t("WQL"),
		bstr_t("SELECT * FROM Win32_VideoController"),
		WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
		NULL,
		&pEnumerator);

	if (FAILED(hres))
	{
		pSvc->Release();
		pLoc->Release();
		return;
	}

	// Get the data from the query in step 6 -------------------

	while (pEnumerator)
	{
		IWbemClassObject *pclsObj = NULL;
		ULONG uReturn = 0;

		HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1,
			&pclsObj, &uReturn);

		if (0 == uReturn)
		{
			break;
		}

		Driver driver;

		VARIANT vtProp_driverversion;
		hr = pclsObj->Get(L"DriverVersion", 0, &vtProp_driverversion, 0, 0);
		driver.name = std::wstring(vtProp_driverversion.bstrVal);
		VariantClear(&vtProp_driverversion);

		VARIANT vtProp_name;
		hr = pclsObj->Get(L"Name", 0, &vtProp_name, 0, 0);
		driver.deviceName = std::wstring(vtProp_name.bstrVal);
		VariantClear(&vtProp_name);

		_drivers.push_back(driver);

		pclsObj->Release(); pclsObj = NULL;
	}

	// Cleanup
	// ========
	pEnumerator->Release(); pEnumerator = NULL;
	pSvc->Release(); pSvc = NULL;
	pLoc->Release(); pLoc = NULL;
#endif
}

bool HardwareResources::parseNVIDIADriver(const std::wstring & rawDriver, int& publicVersionOut)
{
	publicVersionOut = 0;

	try
	{
		std::vector<std::wstring> separatedNumbers = split(rawDriver, L'.');
		if (separatedNumbers.size() != 4)
			return false;

		if (separatedNumbers[2].length() < 1)
			return false;

		const std::wstring &s = separatedNumbers[2];
		std::wstring sLastNumber;
		sLastNumber += s[s.length() - 1];
		int n2_lastNumber = toInt(sLastNumber);
		int n3 = toInt(separatedNumbers[3]);

		publicVersionOut = n3 + n2_lastNumber * 10000;
		return true;
	}
	catch (...)
	{
		// Error parsing NVidia driver.
	}

	return false;
}

std::wstring get_wstring(const std::string & s)
{
	mbstate_t mbState = {};

	const char * cs = s.c_str();
	const size_t wn = std::mbsrtowcs(nullptr, &cs, 0, &mbState);

	if (wn == size_t(-1))
		return L"";

	std::vector<wchar_t> buf(wn + 1);
	const size_t wn_again = std::mbsrtowcs(buf.data(), &cs, wn + 1, &mbState);

	if (wn_again == size_t(-1))
		return L"";

	assert(cs == nullptr);

	return std::wstring(buf.data(), wn);
}

bool HardwareResources::isDriverCompatible(std::string deviceName)
{
	// Convert to wstring for device name comparison.
	std::wstring deviceNameW = get_wstring(deviceName);

	// Find a matching device and return driver compatibility.
	for (Driver& driver : _drivers)
		if (compareDeviceNames(deviceNameW, driver.deviceName))
			return driver.compatible;

	// The driver is considered compatible if not found.
	return true;
}

bool HardwareResources::compareDeviceNames(std::wstring& a, std::wstring& b)
{
	return
		a == b ||
		a.find(b) != std::wstring::npos ||
		b.find(a) != std::wstring::npos ;
}

void CheckGlError()
{
	auto err = glGetError();
	if (err != GL_NO_ERROR)
	{
		DebugPrint("GL ERROR 0x%X\t%s", err, glewGetErrorString(err));
	}
}

bool isMetalOn()
{
#if defined(OSMac_)
    return (NULL == getenv("USE_GPU_OCL"));
#else
    return false;
#endif
}

bool DisconnectFromPlug(MPlug& plug)
{
	if (plug.isNull())
		return false;

	MPlugArray connections;
	plug.connectedTo(connections, true, false);

	if (connections.length())
	{
		MDGModifier modifier;
		for (unsigned int i = 0; i < connections.length(); i++)
		{
			MStatus result = modifier.disconnect(connections[i], plug);

			if (result != MS::kSuccess)
				return false;

			modifier.doIt();
		}
	}

	return true;
}

void SetCameraLookatForMatrix(rpr_camera camera, const MMatrix& matrix)
{
	MPoint eye = MPoint(0, 0, 0, 1) * matrix;
	// convert eye and lookat from cm to m
	eye = eye * GetSceneUnitsConversionCoefficient();
	MVector viewDir = MVector::zNegAxis * matrix;
	MVector upDir = MVector::yAxis * matrix;
	MPoint  lookat = eye + viewDir;
	rpr_int frStatus = rprCameraLookAt(camera,
		static_cast<float>(eye.x), static_cast<float>(eye.y), static_cast<float>(eye.z),
		static_cast<float>(lookat.x), static_cast<float>(lookat.y), static_cast<float>(lookat.z),
		static_cast<float>(upDir.x), static_cast<float>(upDir.y), static_cast<float>(upDir.z));

	checkStatus(frStatus);
}

void setAttribProps(MFnAttribute& attr, const MObject& attrObj)
{
	CHECK_MSTATUS(attr.setKeyable(true));
	CHECK_MSTATUS(attr.setStorable(true));
	CHECK_MSTATUS(attr.setReadable(true));
	CHECK_MSTATUS(attr.setWritable(true));

	CHECK_MSTATUS(MPxNode::addAttribute(attrObj));
}

void CreateBoxGeometry(std::vector<float>& veritces, std::vector<float>& normals, std::vector<int>& vertexIndices, std::vector<int>& normalIndices)
{
	veritces.clear();
	normals.clear();
	vertexIndices.clear();
	normalIndices.clear();

	veritces = {
		-0.5f, -0.5f, -0.5f,
		0.5f, -0.5f, -0.5f,
		0.5f, 0.5f, -0.5f,
		-0.5f, 0.5f, -0.5f,

		-0.5f, -0.5f, 0.5f,
		0.5f, -0.5f, 0.5f,
		0.5f, 0.5f, 0.5f,
		-0.5f, 0.5f, 0.5f,
	};

	normals = {
		0.0f, 0.0f, -1.0f,
		0.0f, 0.0f, 1.0f,

		1.0f, 0.0f, 0.0f,
		-1.0f, 0.0f, 0.0f,

		0.0f, 1.0f, 0.0f,
		0.0f, -1.0f, 0.0f,
	};

	vertexIndices = {
		0, 2, 1,
		0, 3, 2,

		4, 5, 6,
		4, 6, 7,

		0, 1, 5,
		0, 5, 4,

		3, 6, 2,
		3, 7, 6,

		1, 2, 6,
		1, 6, 5,

		0, 7, 3,
		0, 4, 7,
	};

	normalIndices = {
		0, 0, 0,
		0, 0, 0,

		1, 1, 1,
		1, 1, 1,

		5, 5, 5,
		5, 5, 5,

		4, 4, 4,
		4, 4, 4,

		2, 2, 2,
		2, 2, 2,

		3, 3, 3,
		3, 3, 3,
	};
}

void WriteNameAndType(std::vector<MString>& attrData, MObject attr, MObject parent)
{
	MString type = attr.apiTypeStr();

	MFnDependencyNode fnParent(parent);
	MPlug plug = fnParent.findPlug(attr);
	MString name = plug.name();

	// first write name of an attribute than its type
	attrData.push_back(name);
	attrData.push_back(type);
}

std::vector<MString> dumpAttributeNamesDbg(MObject node)
{
	MFnDependencyNode fnNode(node);

	MString fnName = fnNode.name();
	unsigned int attrCount = fnNode.attributeCount();

	std::vector<MString> attrData;

	for (unsigned int idx = 0; idx < attrCount; ++idx)
	{
		MObject attr = fnNode.attribute(idx);
		WriteNameAndType(attrData, attr, node);
	}

	return attrData;
}

RenderQuality GetRenderQualityFromPlug(const char* plugName)
{
	MPlug plug = GetRadeonProRenderGlobalsPlug(plugName);

	if (plug.isNull())
	{
		MGlobal::displayError("Cannot get render type selected. Full render quality will be used");
		return RenderQuality::RenderQualityFull;
	}

	return (RenderQuality)plug.asInt();
}

RenderQuality GetRenderQualityForRenderType(RenderType renderType)
{
	RenderQuality quality = RenderQuality::RenderQualityFull;
	if (renderType == RenderType::ProductionRender)
	{
		quality = GetRenderQualityFromPlug("renderQualityFinalRender");
	}
	else if (renderType == RenderType::ViewportRender || renderType == RenderType::IPR)
	{
		quality = GetRenderQualityFromPlug("renderQualityViewport");
	}

	return quality;
}

bool CheckIsInteractivePossible()
{
	// return true in anticipation that IPR For RPR2 would be fixed for the next Release.
	return true;
}

// Backdoor to enable different AOVs from Render Settings in IPR and Viewport
void EnableAOVsFromRSIfEnvVarSet(FireRenderContext& context, FireRenderAOVs& aovs)
{
	char* result = std::getenv("ENABLE_ADD_AOVS_FOR_INTERACTIVE");

	if (result == nullptr || std::string(result) != "1")
	{
		return;
	}

	aovs.applyToContext(context);
}

template<>
char** GetEnviron(void)
{
	return environ;
}

template<>
wchar_t** GetEnviron(void)
{
#ifndef __APPLE__
	return _wenviron;
#else
	return nullptr;
#endif
}

bool IsUnicodeSystem(void)
{
#ifdef __APPLE__
	return false;
#else
	return GetEnviron<wchar_t*>() != nullptr;
#endif
}

template <>
std::pair<std::string, std::string> GetPathDelimiter(void)
{ 
	return std::make_pair(std::string("\\\\"), std::string("\\")); 
}

template <>
std::pair<std::wstring, std::wstring> GetPathDelimiter(void)
{ 
	return std::make_pair(std::wstring(L"\\\\"), std::wstring(L"\\"));
}

template <>
size_t FindDelimiter(std::string& tmp)
{
	return tmp.find("=");
}

template <>
size_t FindDelimiter(std::wstring& tmp)
{
	return tmp.find(L"=");
}

template <>
std::tuple<std::string, std::string> ProcessEVarSchema(const std::string& eVar)
{
	return std::make_tuple(std::string("%" + eVar + "%"), std::string("${" + eVar + "}"));
}

template <>
std::tuple<std::wstring, std::wstring> ProcessEVarSchema(const std::wstring& eVar)
{
	return std::make_tuple(std::wstring(L"%" + eVar + L"%"), std::wstring(L"${" + eVar + L"}"));
}

// m_createMayaSoftwareCommonGlobalsTab.kExt3 = name.#.ext <= and corresponding pattern on non-English Mayas
void GetUINameFrameExtPattern(std::wstring& nameOut, std::wstring& extOut)
{
	MString command = R"(
		proc string _mayaVarToPlugin() 
		{
			return (uiRes("m_createMayaSoftwareCommonGlobalsTab.kExt3"));
		}
		_mayaVarToPlugin();
	)";
	MString result;
	MStatus res = MGlobal::executeCommand(command, result);
	CHECK_MSTATUS(res);

	nameOut = result.asWChar();
	size_t firstDelimiter = nameOut.find(L".");
	size_t lastDelimiter = nameOut.rfind(L".");

	extOut = nameOut.substr(lastDelimiter + 1, nameOut.length() - 1);
	nameOut = nameOut.substr(0, firstDelimiter);
}

TimePoint GetCurrentChronoTime()
{
	return std::chrono::high_resolution_clock::now();
}

std::string GetPathToHipbinFolder()
{
	MString envScriptPath;
	MStatus status = MGlobal::executeCommand(MString("getenv RPR_SCRIPTS_PATH;"), envScriptPath);
	assert(status == MStatus::kSuccess);
	std::string hipPath = envScriptPath.asChar();
	hipPath += "/../hipbin";

	return hipPath;
}

