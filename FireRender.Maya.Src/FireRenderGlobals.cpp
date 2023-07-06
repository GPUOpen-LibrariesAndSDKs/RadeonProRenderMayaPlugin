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
#include "FireRenderGlobals.h"
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnMessageAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnStringData.h>

#include "FireMaya.h"
#include "FireRenderUtils.h"
#include "FireRenderAOVs.h"
#include "OptionVarHelpers.h"
#include "attributeNames.h"

#include <thread>
#include <string>
#include <thread>
#include <filesystem>
#include "StartupContextChecker.h"

#define DEFAULT_RENDER_STAMP "Radeon ProRender for Maya %b | %h | Time: %pt | Passes: %pp | Objects: %so | Lights: %sl"

// Empirically gotten value. Same used in blender
const int RECOMMENDED_MIN_ITERATIONS_VIEWPORT = 64;

namespace
{
	namespace Attribute
	{
		MObject completionCriteriaHours;
		MObject completionCriteriaMinutes;
		MObject completionCriteriaSeconds;

		MObject completionCriteriaMaxIterations;
		MObject completionCriteriaMinIterations;

		MObject adaptiveThreshold;
		MObject adaptiveTileSize; //hidden attribute

		MObject textureCompression;

		MObject giClampIrradiance;
		MObject giClampIrradianceValue;

		// max depths
		MObject MaxRayDepth;

		MObject MaxDepthDiffuse;
		MObject MaxDepthGlossy;
		MObject MaxDepthShadow;
		MObject MaxDepthRefraction;
		MObject MaxDepthRefractionGlossy;

		MObject RaycastEpsilon;
		MObject EnableOOC;
		MObject TexCacheSize;

		MObject AAFilter;
		MObject AAGridSize;
		MObject ibl;
		MObject flipIBL;
		MObject sky;
		MObject commandPort;

		MObject applyGammaToMayaViews;
		MObject displayGamma;
		MObject textureGamma;

		MObject toneMappingType;
		MObject toneMappingLinearscale;
		MObject toneMappingPhotolinearSensitivity;
		MObject toneMappingPhotolinearExposure;
		MObject toneMappingPhotolinearFstop;
		MObject toneMappingReinhard02Prescale;
		MObject toneMappingReinhard02Postscale;
		MObject toneMappingReinhard02Burn;
		MObject toneMappingSimpleTonemap;
		MObject toneMappingSimpleExposure;
		MObject toneMappingSimpleContrast;
		MObject toneMappingWhiteBalanceEnabled;
		MObject toneMappingWhiteBalanceValue;

		MObject motionBlur;
		MObject cameraMotionBlur;
		MObject motionSamples;
		MObject motionBlurCameraExposure;
		MObject velocityAOVMotionBlur;
		MObject cameraType;

		// for MacOS only: "Use Metal Performance Shaders"
		MObject useMPS;

		// Denoiser
		MObject denoiserEnabled;
		MObject denoiserType;

			// Denoiser type: Bilateral
		MObject denoiserRadius;

		// _TODO Remove after Bileteral fix in ImageProcLibrary
		MObject limitDenoiserRadius;

			// Denoiser type: Local Weighted Regression
		MObject denoiserSamples;
		MObject denoiserFilterRadius;
		MObject denoiserBandwidth;

			// Denoiser type: EAW
		MObject denoiserColor;
		MObject denoiserDepth;
		MObject denoiserNormal;
		MObject denoiserTrans;

			// Denoiser type: ML
		MObject denoiserColorOnly;
		MObject enable16bitCompute;

		MObject viewportDenoiseUpscaleEnabled;

		// Air Volume
		MObject fogEnabled;
		MObject fogColor;
		MObject fogDistance;
		MObject fogHeight;
		MObject airVolumeEnabled;
		MObject airVolumeDensity;
		MObject airVolumeColor;
		MObject airVolumeClamp;

		// image saving
		MObject renderaGlobalsExrMultilayerEnabled;

		MObject renderQuality;

		MObject tahoeVersion;

		MObject textureCachePath;

		// contour
		MObject contourIsEnabled;

		MObject contourUseObjectID;
		MObject contourUseMaterialID;
		MObject contourUseShadingNormal;
		MObject contourUseUV;

		MObject contourLineWidthObjectID;
		MObject contourLineWidthMaterialID;
		MObject contourLineWidthShadingNormal;
		MObject contourLineWidthUV;

		MObject contourNormalThreshold;
		MObject contourUVThreshold;
		MObject contourAntialiasing;

		MObject contourIsDebugEnabled;

		// Deep EXR
		MObject deepEXRMergeZThreshold;

		// Cryptomatte
		MObject cryptomatteExtendedMode;
		MObject cryptomatteSplitIndirect;

		// Use legacy OpenCL context
		MObject useOpenCLContext;
	}

    struct RenderingDeviceAttributes
    {
        MObject cpuThreadCount;
        MObject overrideCpuThreadCount; // bool
    };

    namespace FinalRenderAttributes
    {
        RenderingDeviceAttributes renderingDevices;

        MObject samplesPerUpdate;
        MObject tileRenderEnabled;
        MObject tileRenderX;
        MObject tileRenderY;

		// hybrid specific
		MObject useGmon;
		MObject giniCoeffGmon;
		MObject ptDenoiser; 
		MObject FSR;
		MObject materialCache;
		MObject restirGI;
		MObject reservoirSampling;
		MObject restirSpatialResampleIterations;
		MObject restirMaxReservoirsPerCell;
    }

	namespace ViewportRenderAttributes
	{
		// System tab (will be kept with Maya's preferences)
		RenderingDeviceAttributes renderingDevices;
		MObject maxRayDepth;
		MObject maxDiffuseRayDepth;
		MObject maxDepthGlossy;

		MObject enableSwatches;
		MObject thumbnailIterCount;
		MObject renderMode;
		MObject motionBlur;

		// Other tabs
		MObject completionCriteriaHours;
		MObject completionCriteriaMinutes;
		MObject completionCriteriaSeconds;
		MObject completionCriteriaMaxIterations;
		MObject completionCriteriaMinIterations;
		MObject renderQuality;

		MObject adaptiveThresholdViewport;

		// hybrid specific
		MObject useGmon;
		MObject giniCoeffGmon;
		MObject ptDenoiser;
		MObject FSR;
		MObject materialCache;
		MObject restirGI;
		MObject restirGIBiasCorrection;
		MObject reservoirSampling;
		MObject restirSpatialResampleIterations;
		MObject restirMaxReservoirsPerCell;
	}

	bool operator==(const MStringArray& a, const MStringArray& b)
	{
		if (a.length() != b.length())
			return false;

		for (auto i = 0u; i < a.length(); i++)
		{
			if (a[i] != b[i])
				return false;
		}
		return true;
	}

	bool operator==(const MIntArray& a, const MIntArray& b)
	{
		if (a.length() != b.length())
			return false;

		for (auto i = 0u; i < a.length(); i++)
		{
			if (a[i] != b[i])
				return false;
		}
		return true;
	}

	const int rayDepthParameterMin = 1;
	const int rayDepthParameterSoftMin = 2;
	const int rayDepthParameterSoftMax = 50;
	const int rayDepthParameterMax = 256;

	const int samplesPerUpdateMin = 1;
	const int samplesPerUpdateMax = 512;
}

MObject FireRenderGlobals::m_useRenderStamp;
MObject FireRenderGlobals::m_renderStampText;
MObject FireRenderGlobals::m_renderStampTextDefault;

FireRenderGlobals::GlobalAttributesList FireRenderGlobals::m_globalAttributesList;


FireRenderGlobals::FireRenderGlobals() : 
	m_attributeChangedCallback(0)
{
}

FireRenderGlobals::~FireRenderGlobals()
{
	// _TODO Remove after fix in ImageProcLibrary
	if (m_attributeChangedCallback != 0)
	{
		MNodeMessage::removeCallback(m_attributeChangedCallback);
	}
}

void FireRenderGlobals::postConstructor()
{
	MStatus status;

	syncGlobalAttributeValues();

	MObject obj = thisMObject();
	m_attributeChangedCallback = MNodeMessage::addAttributeChangedCallback(obj, FireRenderGlobals::onAttributeChanged, nullptr, &status);

	CHECK_MSTATUS(status);

	// try creating folder for texture cache
	MString workspace;
	status = MGlobal::executeCommand(MString("workspace -q -dir;"),	workspace);
	workspace += "/cache";

	namespace fs = std::filesystem;
	if (!fs::is_directory(workspace.asChar()) || !fs::exists(workspace.asChar()))
	{ // Check if src folder exists
		fs::create_directory(workspace.asChar());
	}
}

MStatus FireRenderGlobals::compute(const MPlug & plug, MDataBlock & data)
{
	return MS::kSuccess;
}

void * FireRenderGlobals::creator()
{
	return new FireRenderGlobals;
}

MStatus FireRenderGlobals::initialize()
{
	MStatus status;
	MFnNumericAttribute nAttr;
	MFnEnumAttribute eAttr;
	MFnMessageAttribute mAttr;
	MFnTypedAttribute tAttr;
	MFnStringData sData;

	createFinalRenderAttributes();
	createViewportAttributes();
	createCompletionCriteriaAttributes();
	createTileRenderAttributes();
	createContourEffectAttributes();
	createCryptomatteAttributes();

	Attribute::textureCompression = nAttr.create("textureCompression", "texC", MFnNumericData::kBoolean, false, &status);
	MAKE_INPUT(nAttr);

	Attribute::giClampIrradiance = nAttr.create("giClampIrradiance", "gici", MFnNumericData::kBoolean, true, &status);
	MAKE_INPUT(nAttr);

	Attribute::giClampIrradianceValue = nAttr.create("giClampIrradianceValue", "giciv", MFnNumericData::kFloat, 1.0, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(1.0);
	nAttr.setSoftMax(100.0);
	nAttr.setMax(999999.);

	Attribute::AAFilter = eAttr.create("filter", "f", kMitchellFilter, &status);
	eAttr.addField("Box", kBoxFilter);
	eAttr.addField("Triangle", kTriangleFilter);
	eAttr.addField("Gaussian", kGaussianFilter);
	eAttr.addField("Mitchell", kMitchellFilter);
	eAttr.addField("Lanczos", kLanczosFilter);
	eAttr.addField("BlackmanHarris", kBlackmanHarrisFilter);
	MAKE_INPUT_CONST(eAttr);

	Attribute::AAGridSize = nAttr.create("filterSize", "fs", MFnNumericData::kFloat, 1.5, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0.0);
	nAttr.setMax(10);

	setupProductionRayDepthParameters();

	Attribute::RaycastEpsilon = nAttr.create("raycastEpsilon", "rce", MFnNumericData::kFloat, 0.02f, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0.0f);
	nAttr.setSoftMax(2.0f);
	nAttr.setMax(1000.0f);

	Attribute::EnableOOC = nAttr.create("enableOOC", "ooe", MFnNumericData::kBoolean, false, &status);
	MAKE_INPUT(nAttr);
	nAttr.setReadable(true);

	Attribute::TexCacheSize = nAttr.create("textureCacheSize", "ooc", MFnNumericData::kInt, 512, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(256);
	nAttr.setSoftMax(8192);
	nAttr.setMax(100000);

	Attribute::ibl = mAttr.create("imageBasedLighting", "ibl");
	MAKE_INPUT(mAttr);

	Attribute::flipIBL = nAttr.create("flipIBL", "fibl", MFnNumericData::kBoolean, false, &status);
	MAKE_INPUT(nAttr);
	nAttr.setReadable(true);

	Attribute::sky = mAttr.create("sky", "sky");
	MAKE_INPUT(mAttr);

	Attribute::commandPort = nAttr.create("commandPort", "cp", MFnNumericData::kInt, 0, &status);
	nAttr.setHidden(true);
	nAttr.setStorable(false);

	Attribute::applyGammaToMayaViews = nAttr.create("applyGammaToMayaViews", "agtmv", MFnNumericData::kBoolean, false, &status);

	m_useRenderStamp = nAttr.create("useRenderStamp", "rsu", MFnNumericData::kBoolean, false, &status);
	MAKE_INPUT(nAttr);

	MObject defaultRenderStamp = sData.create(DEFAULT_RENDER_STAMP);

	m_renderStampText = tAttr.create("renderStampText", "rs", MFnData::kString, defaultRenderStamp);
	MAKE_INPUT(tAttr);

	m_renderStampTextDefault = tAttr.create("renderStampTextDefault", "rsd", MFnData::kString, defaultRenderStamp);
	tAttr.setStorable(false); // not saved to file
	tAttr.setHidden(true);    // invisible in UI
	tAttr.setWritable(false);

	Attribute::displayGamma = nAttr.create("displayGamma", "dg", MFnNumericData::kFloat, 2.2, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0.0);
	nAttr.setMax(10.0);

	Attribute::textureGamma = nAttr.create("textureGamma", "tg", MFnNumericData::kFloat, 1.0, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0.0);
	nAttr.setMax(10.0);

	Attribute::toneMappingType = eAttr.create("toneMappingType", "tmt", kNone, &status);
	eAttr.addField("None", kNone);
	eAttr.addField("Linear", kLinear);
	eAttr.addField("Photolinear", kPhotolinear);
	eAttr.addField("Autolinear", kAutolinear);
	eAttr.addField("MaxWhite", kMaxWhite);
	eAttr.addField("Reinhard02", kReinhard02);
	eAttr.addField("Simple", kSimple);
	MAKE_INPUT_CONST(eAttr);

	Attribute::toneMappingWhiteBalanceEnabled = nAttr.create("toneMappingWhiteBalanceEnabled", "tmwbe", MFnNumericData::kBoolean, false, &status);
	MAKE_INPUT(nAttr);

	Attribute::toneMappingWhiteBalanceValue = nAttr.create("toneMappingWhiteBalanceValue", "tmwbv", MFnNumericData::kFloat, 3200.0, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(800);
	nAttr.setMax(12000);

	Attribute::toneMappingSimpleTonemap = nAttr.create("toneMappingSimpleTonemap", "tnst", MFnNumericData::kBoolean, true, &status);
	MAKE_INPUT(nAttr);

	Attribute::toneMappingSimpleExposure = nAttr.create("toneMappingSimpleExposure", "tnse", MFnNumericData::kFloat, 0.0, &status);
	MAKE_INPUT(nAttr);
	nAttr.setSoftMin(-5);
	nAttr.setSoftMax(5);

	Attribute::toneMappingSimpleContrast = nAttr.create("toneMappingSimpleContrast", "tnsc", MFnNumericData::kFloat, 1.0, &status);
	MAKE_INPUT(nAttr);
	nAttr.setSoftMin(0);
	nAttr.setSoftMax(2);

	Attribute::toneMappingLinearscale = nAttr.create("toneMappingLinearScale", "tnls", MFnNumericData::kFloat, 1.0, &status);
	MAKE_INPUT(nAttr);

	Attribute::toneMappingPhotolinearSensitivity = nAttr.create("toneMappingPhotolinearSensitivity", "tnpls", MFnNumericData::kFloat, 1.0, &status);
	MAKE_INPUT(nAttr);

	Attribute::toneMappingPhotolinearExposure = nAttr.create("toneMappingPhotolinearExposure", "tnple", MFnNumericData::kFloat, 1.0, &status);
	MAKE_INPUT(nAttr);

	Attribute::toneMappingPhotolinearFstop = nAttr.create("toneMappingPhotolinearFstop", "tnplf", MFnNumericData::kFloat, 3.8, &status);
	MAKE_INPUT(nAttr);

	Attribute::toneMappingReinhard02Prescale = nAttr.create("toneMappingReinhard02Prescale", "tnrprs", MFnNumericData::kFloat, 0.1, &status);
	MAKE_INPUT(nAttr);

	Attribute::toneMappingReinhard02Postscale = nAttr.create("toneMappingReinhard02Postscale", "tnrp", MFnNumericData::kFloat, 1.0, &status);
	MAKE_INPUT(nAttr);

	Attribute::toneMappingReinhard02Burn = nAttr.create("toneMappingReinhard02Burn", "tnrb", MFnNumericData::kFloat, 30.0, &status);
	MAKE_INPUT(nAttr);

	Attribute::motionBlur = nAttr.create("motionBlur", "mblr", MFnNumericData::kBoolean, 0, &status);
	MAKE_INPUT(nAttr);

	Attribute::motionSamples = nAttr.create("motionSamples", "msc", MFnNumericData::kInt, 2, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(2);
	nAttr.setSoftMax(4);
	nAttr.setMax(8);

	Attribute::cameraMotionBlur = nAttr.create("cameraMotionBlur", "cmb", MFnNumericData::kBoolean, 0, &status);
	MAKE_INPUT(nAttr);

	Attribute::motionBlurCameraExposure = nAttr.create("motionBlurCameraExposure", "mbce", MFnNumericData::kFloat, 0.5f, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0.0);
	nAttr.setMax(1.0);

	Attribute::velocityAOVMotionBlur = nAttr.create("velocityAOVMotionBlur", "vavb", MFnNumericData::kBoolean, 0, &status);
	MAKE_INPUT(nAttr);

	Attribute::cameraType = eAttr.create("cameraType", "camt", kCameraDefault, &status);
	eAttr.addField("Default", kCameraDefault);
	eAttr.addField("Spherical Panorama", kSphericalPanorama);
	eAttr.addField("Spherical Panorama Stereo", kSphericalPanoramaStereo);
	eAttr.addField("Cube Map", kCubeMap);
	eAttr.addField("Cube Map Stereo", kCubeMapStereo);
	MAKE_INPUT_CONST(eAttr);

	FireRenderAOVs aovs;
	aovs.registerAttributes();

    setupRenderDevices();

	Attribute::useMPS = nAttr.create("useMPS", "umps", MFnNumericData::kBoolean, 0, &status);
	MAKE_INPUT(nAttr);

	Attribute::renderaGlobalsExrMultilayerEnabled = nAttr.create("enableExrMultilayer", "eeml", MFnNumericData::kBoolean, 0, &status);
	MAKE_INPUT(nAttr);

	Attribute::renderQuality = eAttr.create("renderQualityFinalRender", "rqfr", (short) RenderQuality::RenderQualityFull, &status);
	eAttr.addField("RPR Final", (short)RenderQuality::RenderQualityFull);
#ifdef WIN32
	eAttr.addField("RPR Interactive", (short)RenderQuality::RenderQualityHybridPro);
#endif
	MAKE_INPUT_CONST(eAttr);

	ViewportRenderAttributes::renderQuality = eAttr.create("renderQualityViewport", "rqv", (short) RenderQuality::RenderQualityNorthStar, &status);
#ifdef WIN32
	eAttr.addField("RPR Interactive", (short)RenderQuality::RenderQualityFull); 
#endif
	eAttr.addField("RPR Final", (short)RenderQuality::RenderQualityNorthStar);
	MAKE_INPUT_CONST(eAttr);

	Attribute::tahoeVersion = eAttr.create("renderVersion", "tahv", 0, &status);
	eAttr.addField("Northstar", 0);
	MAKE_INPUT_CONST(eAttr);
	CHECK_MSTATUS(addAttribute(Attribute::tahoeVersion));

	Attribute::useOpenCLContext = nAttr.create("useOpenCLContext", "uoclc", MFnNumericData::kBoolean, false, &status);
	MAKE_INPUT(nAttr);

	int isCachePathExists = 0;
	MGlobal::executeCommand(MString("optionVar -ex \"RPR_textureCachePath\""), isCachePathExists);
	if (!isCachePathExists) {
		MString workspace;
		status = MGlobal::executeCommand(MString("workspace -q -dir;"), workspace);
		workspace += "cache\"";
		MGlobal::executeCommand(MString("optionVar -sv RPR_textureCachePath \"") + workspace);
	}

	MString savedCachePath;
	MGlobal::executeCommand(MString("optionVar -q \"RPR_textureCachePath\""), savedCachePath);
	MObject textureCachePath = sData.create(savedCachePath);
	Attribute::textureCachePath = tAttr.create("textureCachePath", "tcp", MFnData::kString, textureCachePath);
	tAttr.setUsedAsFilename(true);
	addAsGlobalAttribute(tAttr);

	MObject switchDetailedLogAttribute = nAttr.create("detailedLog", "rdl", MFnNumericData::kBoolean, 0, &status);
	MAKE_INPUT(nAttr);
	nAttr.setStorable(false);
	CHECK_MSTATUS(addAttribute(switchDetailedLogAttribute));

	Attribute::deepEXRMergeZThreshold = nAttr.create("deepEXRMergeZThreshold", "det", MFnNumericData::kFloat, 0.1f, &status);
	MAKE_INPUT(nAttr);

	nAttr.setMin(0);
	nAttr.setSoftMin(0);
	nAttr.setMax(10000);
	nAttr.setSoftMax(10);

	CHECK_MSTATUS(addAttribute(Attribute::deepEXRMergeZThreshold));


	// Needed for QA and CIS in order to switch on detailed sync and render logs

	CHECK_MSTATUS(addAttribute(Attribute::textureCompression));

	CHECK_MSTATUS(addAttribute(Attribute::giClampIrradiance));
	CHECK_MSTATUS(addAttribute(Attribute::giClampIrradianceValue));
	CHECK_MSTATUS(addAttribute(Attribute::RaycastEpsilon));
	CHECK_MSTATUS(addAttribute(Attribute::EnableOOC));
	CHECK_MSTATUS(addAttribute(Attribute::TexCacheSize));
	CHECK_MSTATUS(addAttribute(Attribute::AAFilter));
	CHECK_MSTATUS(addAttribute(Attribute::AAGridSize));
	CHECK_MSTATUS(addAttribute(Attribute::ibl));
	CHECK_MSTATUS(addAttribute(Attribute::flipIBL));
	CHECK_MSTATUS(addAttribute(Attribute::sky));
	CHECK_MSTATUS(addAttribute(Attribute::commandPort));
	CHECK_MSTATUS(addAttribute(Attribute::motionBlur));
	CHECK_MSTATUS(addAttribute(Attribute::motionSamples));
	CHECK_MSTATUS(addAttribute(Attribute::cameraMotionBlur));
	CHECK_MSTATUS(addAttribute(Attribute::motionBlurCameraExposure));
	CHECK_MSTATUS(addAttribute(Attribute::velocityAOVMotionBlur));

	CHECK_MSTATUS(addAttribute(Attribute::applyGammaToMayaViews));
	CHECK_MSTATUS(addAttribute(Attribute::displayGamma));
	CHECK_MSTATUS(addAttribute(Attribute::textureGamma));

	CHECK_MSTATUS(addAttribute(Attribute::toneMappingType));
	CHECK_MSTATUS(addAttribute(Attribute::toneMappingWhiteBalanceEnabled));
	CHECK_MSTATUS(addAttribute(Attribute::toneMappingWhiteBalanceValue));
	CHECK_MSTATUS(addAttribute(Attribute::toneMappingSimpleExposure));
	CHECK_MSTATUS(addAttribute(Attribute::toneMappingSimpleTonemap));
	CHECK_MSTATUS(addAttribute(Attribute::toneMappingSimpleContrast));
	CHECK_MSTATUS(addAttribute(Attribute::toneMappingLinearscale));
	CHECK_MSTATUS(addAttribute(Attribute::toneMappingPhotolinearSensitivity));
	CHECK_MSTATUS(addAttribute(Attribute::toneMappingPhotolinearExposure));
	CHECK_MSTATUS(addAttribute(Attribute::toneMappingPhotolinearFstop));
	CHECK_MSTATUS(addAttribute(Attribute::toneMappingReinhard02Prescale));
	CHECK_MSTATUS(addAttribute(Attribute::toneMappingReinhard02Postscale));
	CHECK_MSTATUS(addAttribute(Attribute::toneMappingReinhard02Burn));

	CHECK_MSTATUS(addAttribute(Attribute::cameraType));

	CHECK_MSTATUS(addAttribute(Attribute::useMPS));

	CHECK_MSTATUS(addAttribute(m_useRenderStamp));
	CHECK_MSTATUS(addAttribute(m_renderStampText));
	CHECK_MSTATUS(addAttribute(m_renderStampTextDefault));

	CHECK_MSTATUS(addAttribute(Attribute::renderaGlobalsExrMultilayerEnabled));

	CHECK_MSTATUS(addAttribute(Attribute::renderQuality));
	CHECK_MSTATUS(addAttribute(ViewportRenderAttributes::renderQuality));

	CHECK_MSTATUS(addAttribute(Attribute::useOpenCLContext));

	createDenoiserAttributes();

	createAirVolumeAttributes();

	// create legacy attributes to avoid errors in mel while opening old scenes
	createLegacyAttributes();

	return status;
}

void FireRenderGlobals::addRenderQualityModes(MFnEnumAttribute& eAttr)
{
	eAttr.addField("Full", (short) RenderQuality::RenderQualityFull);

#ifdef WIN32
	eAttr.addField("High", (short) RenderQuality::RenderQualityHigh);
	eAttr.addField("Medium", (short) RenderQuality::RenderQualityMedium);
	eAttr.addField("Low", (short) RenderQuality::RenderQualityLow);
#endif
}

void FireRenderGlobals::createTileRenderAttributes()
{
	MFnNumericAttribute nAttr;
	MStatus status;

	FinalRenderAttributes::tileRenderEnabled = nAttr.create("tileRenderEnabled", "tre", MFnNumericData::kBoolean, false, &status);
	MAKE_INPUT(nAttr);

	CHECK_MSTATUS(addAttribute(FinalRenderAttributes::tileRenderEnabled));

	const int tileDefaultSize = 128;
	const int tileDefaultSizeMin = 16;
	const int tileDefaultSizeMax = 512;

	FinalRenderAttributes::tileRenderX = nAttr.create("tileRenderX", "trx", MFnNumericData::kInt, tileDefaultSize, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(tileDefaultSizeMin);
	nAttr.setSoftMax(tileDefaultSizeMax);

	CHECK_MSTATUS(addAttribute(FinalRenderAttributes::tileRenderX));

	FinalRenderAttributes::tileRenderY = nAttr.create("tileRenderY", "try", MFnNumericData::kInt, tileDefaultSize, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(tileDefaultSizeMin);
	nAttr.setSoftMax(tileDefaultSizeMax);

	CHECK_MSTATUS(addAttribute(FinalRenderAttributes::tileRenderY));
}

void FireRenderGlobals::createCryptomatteAttributes()
{
	MFnNumericAttribute nAttr;
	MStatus status;

	Attribute::cryptomatteExtendedMode = nAttr.create("cryptomatteExtendedMode", "crmem", MFnNumericData::kBoolean, 0, &status);
	MAKE_INPUT(nAttr);

	Attribute::cryptomatteSplitIndirect = nAttr.create("cryptomatteSplitIndirect", "crsin", MFnNumericData::kBoolean, 0, &status);
	MAKE_INPUT(nAttr);

	CHECK_MSTATUS(addAttribute(Attribute::cryptomatteExtendedMode));
	CHECK_MSTATUS(addAttribute(Attribute::cryptomatteSplitIndirect));
}

void FireRenderGlobals::createContourEffectAttributes()
{
	MFnNumericAttribute nAttr;
	MStatus status;

	Attribute::contourIsEnabled = nAttr.create("contourIsEnabled", "coen", MFnNumericData::kBoolean, 0, &status);
	MAKE_INPUT(nAttr);

	Attribute::contourUseObjectID = nAttr.create("contourUseObjectID", "coob", MFnNumericData::kBoolean, 1, &status);
	MAKE_INPUT(nAttr);

	Attribute::contourUseMaterialID = nAttr.create("contourUseMaterialID", "comt", MFnNumericData::kBoolean, 1, &status);
	MAKE_INPUT(nAttr);

	Attribute::contourUseShadingNormal = nAttr.create("contourUseShadingNormal", "cosn", MFnNumericData::kBoolean, 1, &status);
	MAKE_INPUT(nAttr);

	Attribute::contourUseUV = nAttr.create("contourUseUV", "couv", MFnNumericData::kBoolean, 1, &status);
	MAKE_INPUT(nAttr);

	Attribute::contourLineWidthObjectID = nAttr.create("contourLineWidthObjectID", "cowb", MFnNumericData::kFloat, 1.0f, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(1.0f);
	nAttr.setMax(10.0f);

	Attribute::contourLineWidthMaterialID = nAttr.create("contourLineWidthMaterialID", "cowm", MFnNumericData::kFloat, 1.0f, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(1.0f);
	nAttr.setMax(10.0f);

	Attribute::contourLineWidthShadingNormal = nAttr.create("contourLineWidthShadingNormal", "cows", MFnNumericData::kFloat, 1.0f, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(1.0f);
	nAttr.setMax(10.0f);

	Attribute::contourLineWidthUV = nAttr.create("contourLineWidthUV", "cowu", MFnNumericData::kFloat, 1.0f, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(1.0f);
	nAttr.setMax(10.0f);

	Attribute::contourNormalThreshold = nAttr.create("contourNormalThreshold", "cont", MFnNumericData::kFloat, 45.0f, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0.0f);
	nAttr.setMax(180.0f);

	Attribute::contourUVThreshold = nAttr.create("contourUVThreshold", "cout", MFnNumericData::kFloat, 1.0f, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0.0f);
	nAttr.setMax(1.0f);

	Attribute::contourAntialiasing = nAttr.create("contourAntialiasing", "coaa", MFnNumericData::kFloat, 1.0f, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0.0f);
	nAttr.setMax(1.0f);

	Attribute::contourIsDebugEnabled = nAttr.create("contourIsDebugEnabled", "code", MFnNumericData::kBoolean, 0, &status);
	MAKE_INPUT(nAttr);

	CHECK_MSTATUS(addAttribute(Attribute::contourIsEnabled));
	CHECK_MSTATUS(addAttribute(Attribute::contourUseObjectID));
	CHECK_MSTATUS(addAttribute(Attribute::contourUseMaterialID));
	CHECK_MSTATUS(addAttribute(Attribute::contourUseShadingNormal));
	CHECK_MSTATUS(addAttribute(Attribute::contourUseUV));
	CHECK_MSTATUS(addAttribute(Attribute::contourLineWidthObjectID));
	CHECK_MSTATUS(addAttribute(Attribute::contourLineWidthMaterialID));
	CHECK_MSTATUS(addAttribute(Attribute::contourLineWidthShadingNormal));
	CHECK_MSTATUS(addAttribute(Attribute::contourLineWidthUV));
	CHECK_MSTATUS(addAttribute(Attribute::contourNormalThreshold));
	CHECK_MSTATUS(addAttribute(Attribute::contourUVThreshold));
	CHECK_MSTATUS(addAttribute(Attribute::contourAntialiasing));
	CHECK_MSTATUS(addAttribute(Attribute::contourIsDebugEnabled));
}

void FireRenderGlobals::createCompletionCriteriaAttributes()
{
	MFnNumericAttribute nAttr;
	MStatus status;

	Attribute::completionCriteriaHours = nAttr.create("completionCriteriaHours", "cchr", MFnNumericData::kInt, 0, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0);
	nAttr.setSoftMax(24);
	nAttr.setMax(INT_MAX);

	Attribute::completionCriteriaMinutes = nAttr.create("completionCriteriaMinutes", "ccmn", MFnNumericData::kInt, 0, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0);
	nAttr.setMax(59);

	Attribute::completionCriteriaSeconds = nAttr.create("completionCriteriaSeconds", "ccsc", MFnNumericData::kInt, 0, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0);
	nAttr.setMax(59);

	Attribute::completionCriteriaMaxIterations = nAttr.create("completionCriteriaIterations", "ccit", MFnNumericData::kInt, 256, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0);
	nAttr.setSoftMax(1000);
	nAttr.setMax(INT_MAX);

	Attribute::completionCriteriaMinIterations = nAttr.create("completionCriteriaMinIterations", "ccmt", MFnNumericData::kInt, 64, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(16);
	nAttr.setSoftMax(100);
	nAttr.setMax(INT_MAX);

	Attribute::adaptiveThreshold = nAttr.create("adaptiveThreshold", "at", MFnNumericData::kFloat, 0.05, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0.0);
	nAttr.setMax(1.0);

	Attribute::adaptiveTileSize = nAttr.create("adaptiveTileSize", "ats", MFnNumericData::kInt, 16, &status);
	MAKE_INPUT(nAttr);
	nAttr.setHidden(true);
	nAttr.setMin(1);
	nAttr.setMax(64);

	CHECK_MSTATUS(addAttribute(Attribute::completionCriteriaHours));
	CHECK_MSTATUS(addAttribute(Attribute::completionCriteriaMinutes));
	CHECK_MSTATUS(addAttribute(Attribute::completionCriteriaSeconds));
	CHECK_MSTATUS(addAttribute(Attribute::completionCriteriaMaxIterations));
	CHECK_MSTATUS(addAttribute(Attribute::completionCriteriaMinIterations));
	CHECK_MSTATUS(addAttribute(Attribute::adaptiveThreshold));
	CHECK_MSTATUS(addAttribute(Attribute::adaptiveTileSize));	
}

void FireRenderGlobals::createLegacyAttributes()
{
	MFnEnumAttribute eAttr;
	MFnNumericAttribute nAttr;
	MStatus status;

	MObject attrObj = nAttr.create("samples", "s", MFnNumericData::kShort, 1, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(1);
	nAttr.setMax(32);
	CHECK_MSTATUS(addAttribute(attrObj));

	attrObj = nAttr.create("samplesViewport", "sV", MFnNumericData::kShort, 1, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(1);
	nAttr.setMax(32);
	nAttr.setMax(32);
	CHECK_MSTATUS(addAttribute(attrObj));

	attrObj = eAttr.create("qualityPresetsViewport", "qPsV", 0, &status);
	eAttr.addField("Low", 0);
	eAttr.addField("Medium", 1);
	eAttr.addField("High", 2);
	MAKE_INPUT_CONST(eAttr);
	CHECK_MSTATUS(addAttribute(attrObj));

	attrObj = eAttr.create("qualityPresets", "qPs", 0, &status);
	eAttr.addField("Low", 0);
	eAttr.addField("Medium", 1);
	eAttr.addField("High", 2);
	MAKE_INPUT_CONST(eAttr);
	CHECK_MSTATUS(addAttribute(attrObj));

	// this is production render mode which is not exposed to user. Used in AutoTests on CIS
	attrObj = createRenderModeAttr("renderMode", "rm", eAttr);
	CHECK_MSTATUS(addAttribute(attrObj));

	attrObj = nAttr.create("limitDenoiserRadius", "ldr", MFnNumericData::kBoolean, 0, &status);
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(addAttribute(attrObj));

	// legacy ground attributes
	attrObj = nAttr.create("shadows", "grs", MFnNumericData::kBoolean, true, &status);
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(addAttribute(attrObj));

	attrObj = nAttr.create("reflections", "grref", MFnNumericData::kBoolean, false, &status);
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(addAttribute(attrObj));

	attrObj = nAttr.create("strength", "grstr", MFnNumericData::kFloat, 0.5f, &status);
	nAttr.setMin(0.0);
	nAttr.setMax(1.0);
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(addAttribute(attrObj));

	attrObj = nAttr.create("roughness", "grro", MFnNumericData::kFloat, 0.001f, &status);
	CHECK_MSTATUS(addAttribute(attrObj));

	attrObj = nAttr.create("cellSize", "cs", MFnNumericData::kShort, 4, &status);
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(addAttribute(attrObj));

	//legacy attribute for completion criteria
	attrObj = eAttr.create("completionCriteriaType", "cctp", kIterations, &status);
	eAttr.addField("Iterations", kIterations);
	eAttr.addField("Time", kTime);
	eAttr.addField("Unlimited", kUnlimited);
	MAKE_INPUT_CONST(eAttr);
	CHECK_MSTATUS(addAttribute(attrObj));

	attrObj = eAttr.create("completionCriteriaTypeViewport", "vcct", kIterations, &status);
	eAttr.addField("Iterations", kIterations);
	eAttr.addField("Time", kTime);
	eAttr.addField("Unlimited", kUnlimited);
	MAKE_INPUT_CONST(eAttr);
	addAsGlobalAttribute(eAttr);

	attrObj = nAttr.create("motionBlurScale", "mbs", MFnNumericData::kFloat, 1.0f, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0.0);
	CHECK_MSTATUS(addAttribute(attrObj));
}

void FireRenderGlobals::setupProductionRayDepthParameters()
{
	MFnNumericAttribute nAttr;
	MStatus status;

	Attribute::MaxRayDepth = nAttr.create("maxRayDepth", "mrd", MFnNumericData::kInt, 8, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(rayDepthParameterMin);
	nAttr.setSoftMin(rayDepthParameterSoftMin);
	nAttr.setSoftMax(rayDepthParameterSoftMax);
	nAttr.setMax(rayDepthParameterMax);
	CHECK_MSTATUS(addAttribute(Attribute::MaxRayDepth));

	Attribute::MaxDepthDiffuse = nAttr.create("maxDepthDiffuse", "mdd", MFnNumericData::kInt, 3, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(rayDepthParameterMin);
	nAttr.setSoftMin(rayDepthParameterSoftMin);
	nAttr.setSoftMax(rayDepthParameterSoftMax);
	nAttr.setMax(rayDepthParameterMax);
	CHECK_MSTATUS(addAttribute(Attribute::MaxDepthDiffuse));

	Attribute::MaxDepthGlossy = nAttr.create("maxDepthGlossy", "mdg", MFnNumericData::kInt, 5, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(rayDepthParameterMin);
	nAttr.setSoftMin(rayDepthParameterSoftMin);
	nAttr.setSoftMax(rayDepthParameterSoftMax);
	nAttr.setMax(rayDepthParameterMax);
	CHECK_MSTATUS(addAttribute(Attribute::MaxDepthGlossy));

	Attribute::MaxDepthRefraction = nAttr.create("maxDepthRefraction", "mdr", MFnNumericData::kInt, 5, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(rayDepthParameterMin);
	nAttr.setSoftMin(rayDepthParameterSoftMin);
	nAttr.setSoftMax(rayDepthParameterSoftMax);
	nAttr.setMax(rayDepthParameterMax);
	CHECK_MSTATUS(addAttribute(Attribute::MaxDepthRefraction));

	Attribute::MaxDepthRefractionGlossy = nAttr.create("maxDepthRefractionGlossy", "mdrg", MFnNumericData::kInt, 5, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(rayDepthParameterMin);
	nAttr.setSoftMin(rayDepthParameterSoftMin);
	nAttr.setSoftMax(rayDepthParameterSoftMax);
	nAttr.setMax(rayDepthParameterMax);
	CHECK_MSTATUS(addAttribute(Attribute::MaxDepthRefractionGlossy));

	Attribute::MaxDepthShadow = nAttr.create("maxDepthShadow", "mds", MFnNumericData::kInt, 5, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(rayDepthParameterMin);
	nAttr.setSoftMin(rayDepthParameterSoftMin);
	nAttr.setSoftMax(rayDepthParameterSoftMax);
	nAttr.setMax(rayDepthParameterMax);
	CHECK_MSTATUS(addAttribute(Attribute::MaxDepthShadow));
}

void FireRenderGlobals::setupRenderDevices()
{
	auto deviceList = HardwareResources::GetCompatibleGPUs(false);

	MStringArray oldList, newList;
	MIntArray oldDriversCompatible, newDriversCompatible;

	int driversCompatibleExists;

	// _TODO Refactor names, make const strings
	MGlobal::executeCommand("optionVar -q RPR_DevicesName", oldList);
	MGlobal::executeCommand("optionVar -q RPR_DriversCompatible", oldDriversCompatible);
	MGlobal::executeCommand("optionVar -ex RPR_DriversCompatible", driversCompatibleExists);

	MGlobal::executeCommand("optionVar -rm RPR_DevicesName");
	MGlobal::executeCommand("optionVar -rm RPR_DevicesCertified");
	MGlobal::executeCommand("optionVar -rm RPR_DriversCompatible");

	for (const HardwareResources::Device& device : deviceList)
	{
		MGlobal::executeCommand(MString("optionVar -sva RPR_DevicesName \"") + device.name.c_str() + "\"");
		MGlobal::executeCommand(MString("optionVar -iva RPR_DevicesCertified ") + int(device.isCertified()));
		MGlobal::executeCommand(MString("optionVar -iva RPR_DriversCompatible ") + int(device.isDriverCompatible));
	}

	MGlobal::executeCommand("optionVar -q RPR_DevicesName", newList);
	MGlobal::executeCommand("optionVar -q RPR_DriversCompatible", newDriversCompatible);

	std::vector<std::string> attrNames = { FINAL_RENDER_DEVICES_USING_PARAM_NAME, VIEWPORT_DEVICES_USING_PARAM_NAME };

	for (const std::string& name : attrNames)
	{
		bool hardwareSetupChanged = !(oldList == newList) || !(oldDriversCompatible == newDriversCompatible) || !driversCompatibleExists;
		// First time, or something has changed, so time to reset defaults.
		int varExists = false;
		MGlobal::executeCommand(("optionVar -ex " + name).c_str(), varExists);
		if (hardwareSetupChanged || !varExists)
		{
			MGlobal::executeCommand(("optionVar -rm " + name).c_str());
			int selectedCount = 0;
			for (const HardwareResources::Device& device : deviceList)
			{
				int selected = selectedCount < 1 && device.isCertified() && device.isDriverCompatible;
				MGlobal::executeCommand(MString("optionVar -iva ") + name.c_str() + " " + selected);
				if (selected)
					selectedCount++;
			}
		}
	}
}

void FireRenderGlobals::createAirVolumeAttributes()
{
	MStatus status; 
	MFnNumericAttribute nAttr;

	Attribute::airVolumeEnabled = nAttr.create("airVolumeEnabled", "ave", MFnNumericData::kBoolean, false, &status);
	MAKE_INPUT(nAttr);
	nAttr.setReadable(true);
	CHECK_MSTATUS(addAttribute(Attribute::airVolumeEnabled));

	Attribute::fogEnabled = nAttr.create("fogEnabled", "fge", MFnNumericData::kBoolean, false, &status);
	MAKE_INPUT(nAttr);
	nAttr.setReadable(true);
	CHECK_MSTATUS(addAttribute(Attribute::fogEnabled));

	Attribute::fogColor = nAttr.createColor("fogColor", "foc", &status);
	nAttr.setDefault(1.0, 1.0, 1.0);
	MAKE_INPUT(nAttr);
	nAttr.setReadable(true);
	CHECK_MSTATUS(addAttribute(Attribute::fogColor));

	Attribute::fogDistance = nAttr.create("fogDistance", "fod", MFnNumericData::kFloat, 5000, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0);
	nAttr.setMax(10000000);
	CHECK_MSTATUS(addAttribute(Attribute::fogDistance));

	Attribute::fogHeight = nAttr.create("fogHeight", "foh", MFnNumericData::kFloat, 1.5f, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0.0f);
	nAttr.setMax(10000.0f);
	CHECK_MSTATUS(addAttribute(Attribute::fogHeight));

	Attribute::airVolumeDensity = nAttr.create("airVolumeDensity", "avd", MFnNumericData::kFloat, 0.8f, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0.0f);
	nAttr.setMax(1.0f);
	CHECK_MSTATUS(addAttribute(Attribute::airVolumeDensity));

	Attribute::airVolumeColor = nAttr.createColor("airVolumeColor", "avc", &status);
	nAttr.setDefault(1.0, 1.0, 1.0);
	MAKE_INPUT(nAttr);
	nAttr.setReadable(true);
	CHECK_MSTATUS(addAttribute(Attribute::airVolumeColor));

	Attribute::airVolumeClamp = nAttr.create("airVolumeClamp", "avp", MFnNumericData::kFloat, 0.1f, &status);
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(addAttribute(Attribute::airVolumeClamp));
}

void FireRenderGlobals::createDenoiserAttributes()
{
	MStatus status;
	MFnNumericAttribute nAttr;
	MFnEnumAttribute eAttr;

	Attribute::denoiserEnabled = nAttr.create("denoiserEnabled", "de", MFnNumericData::kBoolean, false, &status);
	MAKE_INPUT(nAttr);
	nAttr.setReadable(true);
	CHECK_MSTATUS(addAttribute(Attribute::denoiserEnabled));

	Attribute::denoiserType = eAttr.create("denoiserType", "dt", kBilateral, &status);
	eAttr.addField("Machine Learning", kML);
	eAttr.addField("Bilateral", kBilateral);
	eAttr.addField("LWR", kLWR);
	eAttr.addField("EAW", kEAW);
	MAKE_INPUT_CONST(eAttr);
	nAttr.setReadable(true);
	CHECK_MSTATUS(addAttribute(Attribute::denoiserType));

	// Bilateral
	Attribute::denoiserRadius = nAttr.create("denoiserRadius", "dr", MFnNumericData::kInt, 1, &status);
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(addAttribute(Attribute::denoiserRadius));
	nAttr.setMin(1);
	nAttr.setMax(10);

	//LWR
	Attribute::denoiserSamples = nAttr.create("denoiserSamples", "ds", MFnNumericData::kInt, 2, &status);
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(addAttribute(Attribute::denoiserSamples));
	nAttr.setMin(2);
	nAttr.setMax(10);

	Attribute::denoiserFilterRadius = nAttr.create("denoiserFilterRadius", "dfr", MFnNumericData::kInt, 1, &status);
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(addAttribute(Attribute::denoiserFilterRadius));
	nAttr.setMin(1);
	nAttr.setMax(10);


	Attribute::denoiserBandwidth = nAttr.create("denoiserBandwidth", "db", MFnNumericData::kFloat, 1.0f, &status);
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(addAttribute(Attribute::denoiserBandwidth));
	nAttr.setMin(0.1f);
	nAttr.setMax(1.0f);

	//EAW
	Attribute::denoiserColor = nAttr.create("denoiserColor", "dc", MFnNumericData::kFloat, 1.0f, &status);
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(addAttribute(Attribute::denoiserColor));
	nAttr.setMin(0.1f);
	nAttr.setMax(1.0f);

	Attribute::denoiserDepth = nAttr.create("denoiserDepth", "dd", MFnNumericData::kFloat, 1.0f, &status);
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(addAttribute(Attribute::denoiserDepth));
	nAttr.setMin(0.1f);
	nAttr.setMax(1.0f);

	Attribute::denoiserNormal = nAttr.create("denoiserNormal", "dn", MFnNumericData::kFloat, 1.0f, &status);
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(addAttribute(Attribute::denoiserNormal));
	nAttr.setMin(0.1f);
	nAttr.setMax(1.0f);

	Attribute::denoiserTrans = nAttr.create("denoiserTrans", "dtrn", MFnNumericData::kFloat, 1.0f, &status);
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(addAttribute(Attribute::denoiserTrans));
	nAttr.setMin(0.1f);
	nAttr.setMax(1.0f);

	//ML
	Attribute::denoiserColorOnly = eAttr.create("denoiserColorOnly", "do", 1, &status);
	eAttr.addField("Color Only", 0);
	eAttr.addField("Color + AOVs", 1);
	MAKE_INPUT_CONST(eAttr);
	eAttr.setReadable(true);
	CHECK_MSTATUS(addAttribute(Attribute::denoiserColorOnly));

	Attribute::enable16bitCompute = nAttr.create("enable16bitCompute", "bc", MFnNumericData::kBoolean, false, &status);
	MAKE_INPUT(nAttr);
	nAttr.setReadable(false);
	CHECK_MSTATUS(addAttribute(Attribute::enable16bitCompute));

	Attribute::viewportDenoiseUpscaleEnabled = nAttr.create("viewportDenoiseUpscaleEnabled", "vdue", MFnNumericData::kBoolean, true, &status);
	MAKE_INPUT(nAttr);
	nAttr.setConnectable(false);
	CHECK_MSTATUS(addAttribute(Attribute::viewportDenoiseUpscaleEnabled));
}

void FireRenderGlobals::addAsGlobalAttribute(MFnAttribute& attr)
{
	MObject attrObj = attr.object();
	
	attr.setStorable(false);
	CHECK_MSTATUS(addAttribute(attrObj));

	m_globalAttributesList.push_back(attrObj);
}

void getMinMaxCpuThreads(int& min, int& max, int& softMax)
{
	// setup hardware cpu cores count
	unsigned int concurentThreadsSupported = std::thread::hardware_concurrency();

	unsigned int minThreadCount = 2;
	unsigned int maxThreadCount = 128;

	if (concurentThreadsSupported < minThreadCount)
	{
		concurentThreadsSupported = minThreadCount;
	}
	else if (concurentThreadsSupported > maxThreadCount)
	{
		concurentThreadsSupported = maxThreadCount;
	}

	min = minThreadCount;
	max = maxThreadCount;
	softMax = concurentThreadsSupported;
}

//System Tab
void FireRenderGlobals::createFinalRenderAttributes()
{
    MStatus status;
    MFnNumericAttribute nAttr;

	int minThreadCount, maxThreadCount, concurentThreadsSupported;
	getMinMaxCpuThreads(minThreadCount, maxThreadCount, concurentThreadsSupported);

	FinalRenderAttributes::renderingDevices.cpuThreadCount = nAttr.create("cpuThreadCountFinalRender", "fctc", MFnNumericData::kInt, minThreadCount);
	MAKE_INPUT(nAttr);
	nAttr.setMin(minThreadCount);
	nAttr.setMax(maxThreadCount);
	nAttr.setSoftMax(concurentThreadsSupported);
	addAsGlobalAttribute(nAttr);

	FinalRenderAttributes::renderingDevices.overrideCpuThreadCount = nAttr.create("overrideCpuThreadCountFinalRender", "fotc", MFnNumericData::kBoolean, false);
	MAKE_INPUT(nAttr);
	addAsGlobalAttribute(nAttr);

    FinalRenderAttributes::samplesPerUpdate = nAttr.create("samplesPerUpdate", "spu", MFnNumericData::kInt, 10, &status);
    MAKE_INPUT(nAttr);
    nAttr.setMin(samplesPerUpdateMin);
	nAttr.setMax(samplesPerUpdateMax / 4);
    nAttr.setMax(samplesPerUpdateMax);
	addAsGlobalAttribute(nAttr);
}

void FireRenderGlobals::createViewportAttributes()
{
	MStatus status;
	MFnNumericAttribute nAttr;

	int minThreadCount, maxThreadCount, concurentThreadsSupported;
	getMinMaxCpuThreads(minThreadCount, maxThreadCount, concurentThreadsSupported);

	ViewportRenderAttributes::renderingDevices.cpuThreadCount = nAttr.create("cpuThreadCountViewport", "vctc", MFnNumericData::kInt, minThreadCount);
	MAKE_INPUT(nAttr);
	nAttr.setMin(minThreadCount);
	nAttr.setMax(maxThreadCount);
	nAttr.setSoftMax(concurentThreadsSupported);
	addAsGlobalAttribute(nAttr);

	ViewportRenderAttributes::renderingDevices.overrideCpuThreadCount = nAttr.create("overrideCpuThreadCountViewport", "votc", MFnNumericData::kBoolean, false);
	MAKE_INPUT(nAttr);
	addAsGlobalAttribute(nAttr);

	// completion criteria
	MFnEnumAttribute eAttr;

	ViewportRenderAttributes::completionCriteriaHours = nAttr.create("completionCriteriaHoursViewport", "vcch", MFnNumericData::kInt, 0, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0.0);
	nAttr.setSoftMax(24);
	nAttr.setMax(INT_MAX);
	CHECK_MSTATUS(addAttribute(ViewportRenderAttributes::completionCriteriaHours));

	ViewportRenderAttributes::completionCriteriaMinutes = nAttr.create("completionCriteriaMinutesViewport", "vccm", MFnNumericData::kInt, 0, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0);
	nAttr.setMax(59);
	CHECK_MSTATUS(addAttribute(ViewportRenderAttributes::completionCriteriaMinutes));

	ViewportRenderAttributes::completionCriteriaSeconds = nAttr.create("completionCriteriaSecondsViewport", "vccs", MFnNumericData::kInt, 0, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0);
	nAttr.setMax(59);
	CHECK_MSTATUS(addAttribute(ViewportRenderAttributes::completionCriteriaSeconds));

	ViewportRenderAttributes::completionCriteriaMaxIterations = nAttr.create("completionCriteriaIterationsViewport", "vcci", MFnNumericData::kInt, 256, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0);
	nAttr.setSoftMax(1000);
	nAttr.setMax(INT_MAX);
	CHECK_MSTATUS(addAttribute(ViewportRenderAttributes::completionCriteriaMaxIterations));
																			  
	ViewportRenderAttributes::completionCriteriaMinIterations = nAttr.create("completionCriteriaMinIterationsViewport", "vcmi", MFnNumericData::kInt, RECOMMENDED_MIN_ITERATIONS_VIEWPORT, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(16);
	nAttr.setSoftMax(100);
	nAttr.setMax(INT_MAX);
	CHECK_MSTATUS(addAttribute(ViewportRenderAttributes::completionCriteriaMinIterations));

	ViewportRenderAttributes::enableSwatches = nAttr.create("enableSwatches", "ses", MFnNumericData::kBoolean, true, &status);
	MAKE_INPUT(nAttr);
	addAsGlobalAttribute(nAttr);

	ViewportRenderAttributes::thumbnailIterCount = nAttr.create("thumbnailIterationCount", "tic", MFnNumericData::kInt, 50, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(1);
	nAttr.setSoftMax(100);
	nAttr.setSoftMax(1000);
	nAttr.setMax(INT_MAX);
	addAsGlobalAttribute(nAttr);

	ViewportRenderAttributes::renderMode = createRenderModeAttr("renderModeViewport", "vrm", eAttr);
	addAsGlobalAttribute(eAttr);

	ViewportRenderAttributes::maxRayDepth = nAttr.create("maxRayDepthViewport", "mrdV", MFnNumericData::kInt, 8, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(rayDepthParameterMin);
	nAttr.setSoftMin(rayDepthParameterSoftMin);
	nAttr.setSoftMax(rayDepthParameterSoftMax);
	nAttr.setMax(rayDepthParameterMax);
	CHECK_MSTATUS(addAttribute(ViewportRenderAttributes::maxRayDepth));
	
	ViewportRenderAttributes::maxDiffuseRayDepth = nAttr.create("maxDepthDiffuseViewport", "mddv", MFnNumericData::kInt, 3, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(rayDepthParameterMin);
	nAttr.setSoftMin(rayDepthParameterSoftMin);
	nAttr.setSoftMax(rayDepthParameterSoftMax);
	nAttr.setMax(rayDepthParameterMax);
	CHECK_MSTATUS(addAttribute(ViewportRenderAttributes::maxDiffuseRayDepth));

	ViewportRenderAttributes::maxDepthGlossy = nAttr.create("maxDepthGlossyViewport", "mdgv", MFnNumericData::kInt, 5, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(rayDepthParameterMin);
	nAttr.setSoftMin(rayDepthParameterSoftMin);
	nAttr.setSoftMax(rayDepthParameterSoftMax);
	nAttr.setMax(rayDepthParameterMax);
	CHECK_MSTATUS(addAttribute(ViewportRenderAttributes::maxDepthGlossy));

	ViewportRenderAttributes::motionBlur = nAttr.create("motionBlurViewport", "vmb", MFnNumericData::kBoolean, false);
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(addAttribute(ViewportRenderAttributes::motionBlur));

	ViewportRenderAttributes::adaptiveThresholdViewport = nAttr.create("adaptiveThresholdViewport", "atv", MFnNumericData::kFloat, 0.05, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0.0);
	nAttr.setMax(1.0);
	CHECK_MSTATUS(addAttribute(ViewportRenderAttributes::adaptiveThresholdViewport));

	ViewportRenderAttributes::useGmon = nAttr.create("useGmon", "vpugm", MFnNumericData::kBoolean, false);
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(addAttribute(ViewportRenderAttributes::useGmon));

	ViewportRenderAttributes::giniCoeffGmon = nAttr.create("giniCoeffGmon", "vpgcg", MFnNumericData::kFloat, 0.5);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0);
	nAttr.setMax(1);
	CHECK_MSTATUS(addAttribute(ViewportRenderAttributes::giniCoeffGmon));

	ViewportRenderAttributes::ptDenoiser = eAttr.create("ptDenoiser", "vpptd", PtDenoiserType::kNoPtDenoiser, &status);
	eAttr.addField("None", PtDenoiserType::kNoPtDenoiser);
	eAttr.addField("SVGF", PtDenoiserType::kSVGF);
	eAttr.addField("ASVGF", PtDenoiserType::kASVGF);
	MAKE_INPUT(eAttr);
	CHECK_MSTATUS(addAttribute(ViewportRenderAttributes::ptDenoiser));

	ViewportRenderAttributes::FSR = eAttr.create("FSR", "vpfsr", FSRType::kNoFSR, &status);
	eAttr.addField("None", FSRType::kNoFSR);
	eAttr.addField("Ultra", FSRType::kUltraFSR);
	eAttr.addField("Quality", FSRType::kQualityFSR);
	eAttr.addField("Balance", FSRType::kBalanceFSR);
	eAttr.addField("Performance", FSRType::kPerformanceFSR);
	eAttr.addField("Ultra Performance", FSRType::kUltraPerformanceFSR);
	MAKE_INPUT(eAttr);
	CHECK_MSTATUS(addAttribute(ViewportRenderAttributes::FSR));

	ViewportRenderAttributes::materialCache = nAttr.create("materialCache", "vpmc", MFnNumericData::kBoolean, true);
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(addAttribute(ViewportRenderAttributes::materialCache));

	ViewportRenderAttributes::restirGI = nAttr.create("restirGI", "vprgi", MFnNumericData::kBoolean, false);
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(addAttribute(ViewportRenderAttributes::restirGI));
	ViewportRenderAttributes::restirGIBiasCorrection = eAttr.create("restirGIBiasCorrection", "vprbc", RestirGIBiasCorrection::kDeterministicMIS, &status);
	eAttr.addField("No Bias Correction", RestirGIBiasCorrection::kNoBiasCorrection);
	eAttr.addField("Uniform Weights", RestirGIBiasCorrection::kUniformWeights);
	eAttr.addField("Stochastic MIS", RestirGIBiasCorrection::kStochasticMIS);
	eAttr.addField("Deterministic MIS", RestirGIBiasCorrection::kDeterministicMIS);
	MAKE_INPUT(eAttr);
	CHECK_MSTATUS(addAttribute(ViewportRenderAttributes::restirGIBiasCorrection));
	
	ViewportRenderAttributes::reservoirSampling = eAttr.create("reservoirSampling", "vprsp", ReservoirSampling::kWorldSpace, &status);
	eAttr.addField("Disabled", ReservoirSampling::kDisabled);
	eAttr.addField("Screen Space", ReservoirSampling::kScreenSpace);
	eAttr.addField("World Space", ReservoirSampling::kWorldSpace);
	MAKE_INPUT(eAttr);
	CHECK_MSTATUS(addAttribute(ViewportRenderAttributes::reservoirSampling));
	
	ViewportRenderAttributes::restirSpatialResampleIterations = eAttr.create("restirSpatialResampleIterations", "vprsri", 3, &status);
	eAttr.addField("1", 1);
	eAttr.addField("3", 3);
	eAttr.addField("5", 5);
	MAKE_INPUT(eAttr);
	CHECK_MSTATUS(addAttribute(ViewportRenderAttributes::restirSpatialResampleIterations));
	
	ViewportRenderAttributes::restirMaxReservoirsPerCell = nAttr.create("restirMaxReservoirsPerCell", "vprmr", MFnNumericData::kInt, 128, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(1);
	nAttr.setMax(512);
	CHECK_MSTATUS(addAttribute(ViewportRenderAttributes::restirMaxReservoirsPerCell));

	ViewportRenderAttributes::useGmon = nAttr.create("finalRender_useGmon", "frugm", MFnNumericData::kBoolean, false);
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(addAttribute(ViewportRenderAttributes::useGmon));

	ViewportRenderAttributes::giniCoeffGmon = nAttr.create("finalRender_giniCoeffGmon", "frgcg", MFnNumericData::kFloat, 0.5);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0);
	nAttr.setMax(1);
	CHECK_MSTATUS(addAttribute(ViewportRenderAttributes::giniCoeffGmon));

	FinalRenderAttributes::ptDenoiser = eAttr.create("finalRender_ptDenoiser", "frptd", PtDenoiserType::kNoPtDenoiser, &status);
	eAttr.addField("None", PtDenoiserType::kNoPtDenoiser);
	eAttr.addField("SVGF", PtDenoiserType::kSVGF);
	eAttr.addField("ASVGF", PtDenoiserType::kASVGF);
	MAKE_INPUT(eAttr);
	CHECK_MSTATUS(addAttribute(FinalRenderAttributes::ptDenoiser)); 

	FinalRenderAttributes::FSR = eAttr.create("finalRender_FSR", "frfsr", FSRType::kNoFSR, &status);
	eAttr.addField("None", FSRType::kNoFSR);
	eAttr.addField("Ultra", FSRType::kUltraFSR);
	eAttr.addField("Quality", FSRType::kQualityFSR);
	eAttr.addField("Balance", FSRType::kBalanceFSR);
	eAttr.addField("Performance", FSRType::kPerformanceFSR);
	eAttr.addField("Ultra Performance", FSRType::kUltraPerformanceFSR);
	MAKE_INPUT(eAttr);
	CHECK_MSTATUS(addAttribute(FinalRenderAttributes::FSR));

	FinalRenderAttributes::materialCache = nAttr.create("finalRender_materialCache", "frmc", MFnNumericData::kBoolean, true);
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(addAttribute(FinalRenderAttributes::materialCache));

	FinalRenderAttributes::restirGI = nAttr.create("finalRender_restirGI", "frrgi", MFnNumericData::kBoolean, false);
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(addAttribute(FinalRenderAttributes::restirGI));
	
	FinalRenderAttributes::reservoirSampling = eAttr.create("finalRender_reservoirSampling", "frrsp", ReservoirSampling::kWorldSpace, &status);
	eAttr.addField("Disabled", ReservoirSampling::kDisabled);
	eAttr.addField("Screen Space", ReservoirSampling::kScreenSpace);
	eAttr.addField("World Space", ReservoirSampling::kWorldSpace);
	MAKE_INPUT(eAttr);
	CHECK_MSTATUS(addAttribute(FinalRenderAttributes::reservoirSampling)); 
	
	FinalRenderAttributes::restirSpatialResampleIterations = eAttr.create("finalRender_restirSpatialResampleIterations", "frrsri", 3, &status);
	eAttr.addField("1", 1);
	eAttr.addField("3", 3);
	eAttr.addField("5", 5);
	MAKE_INPUT(eAttr);
	CHECK_MSTATUS(addAttribute(FinalRenderAttributes::restirSpatialResampleIterations));

	FinalRenderAttributes::restirMaxReservoirsPerCell = nAttr.create("finalRender_restirMaxReservoirsPerCell", "frrmr", MFnNumericData::kInt, 128, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(1);
	nAttr.setMax(512);
	CHECK_MSTATUS(addAttribute(FinalRenderAttributes::restirMaxReservoirsPerCell));
}

/** Return the FR camera mode that matches the given camera type. */
frw::CameraMode FireRenderGlobals::getCameraModeForType(CameraType type, bool defaultIsOrtho)
{
	switch (type)
	{
	case kSphericalPanorama:
		return frw::CameraModePanorama;
	case kSphericalPanoramaStereo:
		return frw::CameraModePanoramaStereo;
	case kCubeMap:
		return frw::CameraModeCubemap;
	case kCubeMapStereo:
		return frw::CameraModeCubemapStereo;
	case kCameraDefault:
	default:
		return defaultIsOrtho ? frw::CameraModeOrthographic : frw::CameraModePerspective;
	}
}

void FireRenderGlobals::onAttributeChanged(MNodeMessage::AttributeMessage msg, MPlug &plug, MPlug &otherPlug, void *clientData)
{
	if ((msg & MNodeMessage::kAttributeSet) == 0)
	{
		return;
	}

	MObject attrObj = plug.attribute();

	bool exists = false;
	for (MObject obj : m_globalAttributesList)
	{
		if (obj == attrObj)
		{
			exists = true;
			break;
		}
	}

	if (exists)
	{		
		updateCorrespondingOptionVar(plug);
	}
}

void FireRenderGlobals::syncGlobalAttributeValues()
{
	MObject obj = thisMObject();

	MFnDependencyNode depNode(obj);

	for (const MObject& attrObj : m_globalAttributesList)
	{
		int exist = 0;

		MString varName = getOptionVarNameForAttributeName(MFnAttribute(attrObj).name());

		MGlobal::executeCommand(getOptionVarMelCommand("-exists", varName, ""), exist);

		MPlug plug = depNode.findPlug(attrObj);
		if (exist > 0)
		{
			updateAttributeFromOptionVar(plug, varName);
		}
		else
		{
			// if optionVar variable does not exist set it to default from attribute
			updateCorrespondingOptionVar(plug);
		}
	}
}

MObject FireRenderGlobals::createRenderModeAttr(const char* attrName, const char* attrNameShort, MFnEnumAttribute& eAttr)
{
	MStatus status;

	MObject objAttr = eAttr.create(attrName, attrNameShort, kGlobalIllumination, &status);
	eAttr.addField("globalIllumination", kGlobalIllumination);
	eAttr.addField("directIllumination", kDirectIllumination);
	eAttr.addField("directIlluminationNoShadow", kDirectIlluminationNoShadow);
	eAttr.addField("wireframe", kWireframe);
	eAttr.addField("materialId", kMaterialId);
	eAttr.addField("position", kPosition);
	eAttr.addField("normal", kNormal);
	eAttr.addField("texcoord", kTexcoord);
	eAttr.addField("ambientOcclusion", kAmbientOcclusion);
	MAKE_INPUT_CONST(eAttr);

	return objAttr;
}
