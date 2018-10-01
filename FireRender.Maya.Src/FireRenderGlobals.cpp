#include "FireRenderGlobals.h"
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnMessageAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnStringData.h>
#include "FireMaya.h"
#include "FireRenderUtils.h"
#include "FireRenderAOVs.h"

#include <string>
#include <thread>

#define DEFAULT_RENDER_STAMP "Radeon ProRender for Maya %b | %h | Time: %pt | Passes: %pp | Objects: %so | Lights: %sl"

namespace
{
	namespace Attribute
	{
		MObject completionCriteriaType;
		MObject completionCriteriaHours;
		MObject completionCriteriaMinutes;
		MObject completionCriteriaSeconds;
		MObject completionCriteriaIterations;

		MObject textureCompression;

		MObject renderMode;

		MObject giClampIrradiance;
		MObject giClampIrradianceValue;

		MObject AASampleCountProduction;

		// max depths
		MObject MaxRayDepthProduction;

		MObject MaxDepthDiffuse;
		MObject MaxDepthGlossy;
		MObject MaxDepthShadow;
		MObject MaxDepthRefraction;
		MObject MaxDepthRefractionGlossy;

		MObject RaycastEpsilon;
		MObject EnableOOC;
		MObject TexCacheSize;

		MObject AASampleCountViewport;
		MObject MaxRayDepthViewport;

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
		MObject toneMappingWhiteBalanceEnabled;
		MObject toneMappingWhiteBalanceValue;

		MObject motionBlur;
		MObject motionBlurCameraExposure;
		MObject motionBlurScale;
		MObject cameraType;

		MObject qualityPresetsViewport;
		MObject qualityPresetsProduction;

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
}


MObject FireRenderGlobals::m_useGround;
MObject FireRenderGlobals::m_groundHeight;
MObject FireRenderGlobals::m_groundRadius;
MObject FireRenderGlobals::m_groundShadows;
MObject FireRenderGlobals::m_groundReflections;
MObject FireRenderGlobals::m_groundStrength;
MObject FireRenderGlobals::m_groundRoughness;

MObject FireRenderGlobals::m_useRenderStamp;
MObject FireRenderGlobals::m_renderStampText;
MObject FireRenderGlobals::m_renderStampTextDefault;

// _TODO Remove after fix in ImageProcLibrary
MCallbackId FireRenderGlobals::m_attributeChangedCallback;

FireRenderGlobals::FireRenderGlobals()
{
	m_attributeChangedCallback = 0;
}

FireRenderGlobals::~FireRenderGlobals()
{
	// _TODO Remove after fix in ImageProcLibrary
	if (m_attributeChangedCallback != 0)
	{
		MNodeMessage::removeCallback(m_attributeChangedCallback);
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

	Attribute::completionCriteriaType = eAttr.create("completionCriteriaType", "cctp", kIterations, &status);
	eAttr.addField("Iterations", kIterations);
	eAttr.addField("Time", kTime);
	eAttr.addField("Unlimited", kUnlimited);
	MAKE_INPUT_CONST(eAttr);

	Attribute::completionCriteriaHours = nAttr.create("completionCriteriaHours", "cchr", MFnNumericData::kInt, 0, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0.0);
	nAttr.setSoftMax(24.0);
	nAttr.setMax(INT_MAX);

	Attribute::completionCriteriaMinutes = nAttr.create("completionCriteriaMinutes", "ccmn", MFnNumericData::kInt, 0, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0.0);
	nAttr.setMax(59.0);

	Attribute::completionCriteriaSeconds = nAttr.create("completionCriteriaSeconds", "ccsc", MFnNumericData::kInt, 10, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(1.0);
	nAttr.setMax(59.0);

	Attribute::completionCriteriaIterations = nAttr.create("completionCriteriaIterations", "ccit", MFnNumericData::kInt, 100, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(1.0);
	nAttr.setSoftMax(1000);
	nAttr.setMax(INT_MAX);


	Attribute::textureCompression = nAttr.create("textureCompression", "texC", MFnNumericData::kBoolean, false, &status);
	MAKE_INPUT(nAttr);

	Attribute::renderMode = eAttr.create("renderMode", "rm", kGlobalIllumination, &status);
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

	Attribute::giClampIrradiance = nAttr.create("giClampIrradiance", "gici", MFnNumericData::kBoolean, true, &status);
	MAKE_INPUT(nAttr);

	Attribute::giClampIrradianceValue = nAttr.create("giClampIrradianceValue", "giciv", MFnNumericData::kFloat, 1.0, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(1.0);
	nAttr.setSoftMax(100.0f);
	nAttr.setMax(999999.f);

	Attribute::AASampleCountProduction = nAttr.create("samples", "s", MFnNumericData::kShort, 1, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(1);
	nAttr.setMax(32);
	Attribute::AASampleCountViewport = nAttr.create("samplesViewport", "sV", MFnNumericData::kShort, 1, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(1);
	nAttr.setMax(32);

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
	nAttr.setMin(1);
	nAttr.setMax(10);

	setupRayDepthParameters();

	Attribute::RaycastEpsilon = nAttr.create("raycastEpsilon", "rce", MFnNumericData::kFloat, 0.02f, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0.0f);
	nAttr.setSoftMax(2.0f);
	nAttr.setMax(10.0f);

	Attribute::EnableOOC = nAttr.create("enableOOC", "ooe", MFnNumericData::kBoolean, false, &status);
	MAKE_INPUT(nAttr);
	nAttr.setReadable(true);

	Attribute::TexCacheSize = nAttr.create("textureCacheSize", "ooc", MFnNumericData::kInt, 512, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(256);
	nAttr.setSoftMax(8192);
	nAttr.setMax(100000);

	Attribute::MaxRayDepthViewport = nAttr.create("maxRayDepthViewport", "mrdV", MFnNumericData::kShort, 5, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0);
	nAttr.setMax(50);

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
	m_useGround = nAttr.create("useGround", "gru", MFnNumericData::kBoolean, false, &status);
	MAKE_INPUT(nAttr);
	m_groundHeight = nAttr.create("groundHeight", "grh", MFnNumericData::kFloat, 0.0f, &status);
	MAKE_INPUT(nAttr);
	m_groundRadius = nAttr.create("groundRadius", "grr", MFnNumericData::kFloat, 1000.0f, &status);
	nAttr.setMin(0.0);

	m_useRenderStamp = nAttr.create("useRenderStamp", "rsu", MFnNumericData::kBoolean, false, &status);
	MAKE_INPUT(nAttr);
	MObject defaultRenderStamp = sData.create(DEFAULT_RENDER_STAMP);
	m_renderStampText = tAttr.create("renderStampText", "rs", MFnData::kString, defaultRenderStamp);
	MAKE_INPUT(tAttr);
	m_renderStampTextDefault = tAttr.create("renderStampTextDefault", "rsd", MFnData::kString, defaultRenderStamp);
	tAttr.setStorable(false); // not saved to file
	tAttr.setHidden(true);    // invisible in UI
	tAttr.setWritable(false);

	MAKE_INPUT(nAttr);
	m_groundShadows = nAttr.create("shadows", "grs", MFnNumericData::kBoolean, true, &status);
	MAKE_INPUT(nAttr);
	m_groundReflections = nAttr.create("reflections", "grref", MFnNumericData::kBoolean, false, &status);
	MAKE_INPUT(nAttr);
	m_groundStrength = nAttr.create("strength", "grstr", MFnNumericData::kFloat, 0.5f, &status);
	nAttr.setMin(0.0);
	nAttr.setMax(1.0);
	MAKE_INPUT(nAttr);
	m_groundRoughness = nAttr.create("roughness", "grro", MFnNumericData::kFloat, 0.001f, &status);
	nAttr.setMin(0.0);
	MAKE_INPUT(nAttr);

	MAKE_INPUT(nAttr);

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
	MAKE_INPUT_CONST(eAttr);

	Attribute::toneMappingWhiteBalanceEnabled = nAttr.create("toneMappingWhiteBalanceEnabled", "tmwbe", MFnNumericData::kBoolean, false, &status);
	MAKE_INPUT(nAttr);

	Attribute::toneMappingWhiteBalanceValue = nAttr.create("toneMappingWhiteBalanceValue", "tmwbv", MFnNumericData::kFloat, 3200.0, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(800);
	nAttr.setMax(12000);

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

	Attribute::motionBlurCameraExposure = nAttr.create("motionBlurCameraExposure", "mbce", MFnNumericData::kFloat, 0.1f, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0.0);

	Attribute::motionBlurScale = nAttr.create("motionBlurScale", "mbs", MFnNumericData::kFloat, 1.0f, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0.0);

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

	Attribute::qualityPresetsViewport = eAttr.create("qualityPresetsViewport", "qPsV", 0, &status);
	eAttr.addField("Low", 0);
	eAttr.addField("Medium", 1);
	eAttr.addField("High", 2);
	MAKE_INPUT_CONST(eAttr);

	Attribute::qualityPresetsProduction = eAttr.create("qualityPresets", "qPs", 0, &status);
	eAttr.addField("Low", 0);
	eAttr.addField("Medium", 1);
	eAttr.addField("High", 2);
	MAKE_INPUT_CONST(eAttr);

	Attribute::useMPS = nAttr.create("useMPS", "umps", MFnNumericData::kBoolean, 0, &status);
	MAKE_INPUT(nAttr);

	CHECK_MSTATUS(addAttribute(Attribute::completionCriteriaType));
	CHECK_MSTATUS(addAttribute(Attribute::completionCriteriaHours));
	CHECK_MSTATUS(addAttribute(Attribute::completionCriteriaMinutes));
	CHECK_MSTATUS(addAttribute(Attribute::completionCriteriaSeconds));
	CHECK_MSTATUS(addAttribute(Attribute::completionCriteriaIterations));

	CHECK_MSTATUS(addAttribute(Attribute::textureCompression));

	CHECK_MSTATUS(addAttribute(Attribute::renderMode));
	CHECK_MSTATUS(addAttribute(Attribute::giClampIrradiance));
	CHECK_MSTATUS(addAttribute(Attribute::giClampIrradianceValue));
	CHECK_MSTATUS(addAttribute(Attribute::AASampleCountProduction));
	CHECK_MSTATUS(addAttribute(Attribute::MaxRayDepthProduction));
	CHECK_MSTATUS(addAttribute(Attribute::RaycastEpsilon));
	CHECK_MSTATUS(addAttribute(Attribute::EnableOOC));
	CHECK_MSTATUS(addAttribute(Attribute::TexCacheSize));
	CHECK_MSTATUS(addAttribute(Attribute::AASampleCountViewport));
	CHECK_MSTATUS(addAttribute(Attribute::MaxRayDepthViewport));
	CHECK_MSTATUS(addAttribute(Attribute::AAFilter));
	CHECK_MSTATUS(addAttribute(Attribute::AAGridSize));
	CHECK_MSTATUS(addAttribute(Attribute::ibl));
	CHECK_MSTATUS(addAttribute(Attribute::flipIBL));
	CHECK_MSTATUS(addAttribute(Attribute::sky));
	CHECK_MSTATUS(addAttribute(Attribute::commandPort));
	CHECK_MSTATUS(addAttribute(Attribute::motionBlur));
	CHECK_MSTATUS(addAttribute(Attribute::motionBlurCameraExposure));
	CHECK_MSTATUS(addAttribute(Attribute::motionBlurScale));

	CHECK_MSTATUS(addAttribute(Attribute::applyGammaToMayaViews));
	CHECK_MSTATUS(addAttribute(Attribute::displayGamma));
	CHECK_MSTATUS(addAttribute(Attribute::textureGamma));

	CHECK_MSTATUS(addAttribute(Attribute::toneMappingType));
	CHECK_MSTATUS(addAttribute(Attribute::toneMappingWhiteBalanceEnabled));
	CHECK_MSTATUS(addAttribute(Attribute::toneMappingWhiteBalanceValue));
	CHECK_MSTATUS(addAttribute(Attribute::toneMappingLinearscale));
	CHECK_MSTATUS(addAttribute(Attribute::toneMappingPhotolinearSensitivity));
	CHECK_MSTATUS(addAttribute(Attribute::toneMappingPhotolinearExposure));
	CHECK_MSTATUS(addAttribute(Attribute::toneMappingPhotolinearFstop));
	CHECK_MSTATUS(addAttribute(Attribute::toneMappingReinhard02Prescale));
	CHECK_MSTATUS(addAttribute(Attribute::toneMappingReinhard02Postscale));
	CHECK_MSTATUS(addAttribute(Attribute::toneMappingReinhard02Burn));

	CHECK_MSTATUS(addAttribute(Attribute::cameraType));

	CHECK_MSTATUS(addAttribute(Attribute::qualityPresetsProduction));
	CHECK_MSTATUS(addAttribute(Attribute::qualityPresetsViewport));
	CHECK_MSTATUS(addAttribute(Attribute::useMPS));

	CHECK_MSTATUS(addAttribute(m_useGround));
	CHECK_MSTATUS(addAttribute(m_groundHeight));
	CHECK_MSTATUS(addAttribute(m_groundRadius));
	CHECK_MSTATUS(addAttribute(m_groundShadows));
	CHECK_MSTATUS(addAttribute(m_groundReflections));
	CHECK_MSTATUS(addAttribute(m_groundStrength));
	CHECK_MSTATUS(addAttribute(m_groundRoughness));

	CHECK_MSTATUS(addAttribute(m_useRenderStamp));
	CHECK_MSTATUS(addAttribute(m_renderStampText));
	CHECK_MSTATUS(addAttribute(m_renderStampTextDefault));

	createDenoiserAttributes();

	return status;
}

void FireRenderGlobals::setupRayDepthParameters()
{
	int min = 0;
	int softMin = 2;
	int softMax = 50;

	MFnNumericAttribute nAttr;
	MStatus status;

	Attribute::MaxRayDepthProduction = nAttr.create("maxRayDepth", "mrd", MFnNumericData::kShort, 8, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0);
	nAttr.setSoftMin(softMin);
	nAttr.setSoftMax(softMax);
	CHECK_MSTATUS(addAttribute(Attribute::MaxRayDepthProduction));

	Attribute::MaxDepthDiffuse = nAttr.create("maxDepthDiffuse", "mdd", MFnNumericData::kShort, 3, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0);
	nAttr.setSoftMin(softMin);
	nAttr.setSoftMax(softMax);
	CHECK_MSTATUS(addAttribute(Attribute::MaxDepthDiffuse));

	Attribute::MaxDepthGlossy = nAttr.create("maxDepthGlossy", "mdg", MFnNumericData::kShort, 5, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0);
	nAttr.setSoftMin(softMin);
	nAttr.setSoftMax(softMax);
	CHECK_MSTATUS(addAttribute(Attribute::MaxDepthGlossy));

	Attribute::MaxDepthRefraction = nAttr.create("maxDepthRefraction", "mdr", MFnNumericData::kShort, 5, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0);
	nAttr.setSoftMin(softMin);
	nAttr.setSoftMax(softMax);
	CHECK_MSTATUS(addAttribute(Attribute::MaxDepthRefraction));

	Attribute::MaxDepthRefractionGlossy = nAttr.create("maxDepthRefractionGlossy", "mdrg", MFnNumericData::kShort, 5, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0);
	nAttr.setSoftMin(softMin);
	nAttr.setSoftMax(softMax);
	CHECK_MSTATUS(addAttribute(Attribute::MaxDepthRefractionGlossy));

	Attribute::MaxDepthShadow = nAttr.create("maxDepthShadow", "mds", MFnNumericData::kShort, 5, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0);
	nAttr.setSoftMin(softMin);
	nAttr.setSoftMax(softMax);
	CHECK_MSTATUS(addAttribute(Attribute::MaxDepthShadow));
}

void FireRenderGlobals::setupRenderDevices()
{
	auto deviceList = HardwareResources::GetCompatibleGPUs(false);

	MStringArray oldList, newList;
	MIntArray oldDriversCompatible, newDriversCompatible;

	int allowUncertified;
	int driversCompatibleExists;

	MGlobal::executeCommand("optionVar -q RPR_DevicesName", oldList);
	MGlobal::executeCommand("optionVar -q RPR_AllowUncertified", allowUncertified);
	MGlobal::executeCommand("optionVar -q RPR_DriversCompatible", oldDriversCompatible);
	MGlobal::executeCommand("optionVar -ex RPR_DriversCompatible", driversCompatibleExists);

	MGlobal::executeCommand("optionVar -rm RPR_DevicesName");
	MGlobal::executeCommand("optionVar -rm RPR_DevicesCertified");
	MGlobal::executeCommand("optionVar -rm RPR_DriversCompatible");

	for (auto device : deviceList)
	{
		MGlobal::executeCommand(MString("optionVar -sva RPR_DevicesName \"") + device.name.c_str() + "\"");
		MGlobal::executeCommand(MString("optionVar -iva RPR_DevicesCertified ") + int(device.isCertified()));
		MGlobal::executeCommand(MString("optionVar -iva RPR_DriversCompatible ") + int(device.isDriverCompatible));
	}

	MGlobal::executeCommand("optionVar -q RPR_DevicesName", newList);
	MGlobal::executeCommand("optionVar -q RPR_DriversCompatible", newDriversCompatible);

	// First time, or something has changed, so time to reset defaults.
	if (!(oldList == newList) || !(oldDriversCompatible == newDriversCompatible) || !driversCompatibleExists)
	{
		MGlobal::executeCommand("optionVar -rm RPR_DevicesSelected");
		int selectedCount = 0;
		for (auto device : deviceList)
		{
			int selected = selectedCount < 1 && device.isCertified() && device.isDriverCompatible;
			MGlobal::executeCommand(MString("optionVar -iva RPR_DevicesSelected ") + selected);
			if (selected)
				selectedCount++;
		}
	}

	// setup hardware cpu cores count
	unsigned concurentThreadsSupported = std::thread::hardware_concurrency();

	int minThreadCount = 2;
	int maxThreadCount = 128;

	if (concurentThreadsSupported < minThreadCount)
	{
		concurentThreadsSupported = minThreadCount;
	}
	else if (concurentThreadsSupported > maxThreadCount)
	{
		concurentThreadsSupported = maxThreadCount;
	}

	if (!FireRenderGlobals::isOptionVarExist("RPR_CPUThreadCount"))
	{
		FireRenderGlobals::setOptionVarInt("RPR_CPUThreadCount", concurentThreadsSupported);
	}

	MGlobal::executeCommand(MString("optionVar -iv RPR_HardwareCoresCount ") + concurentThreadsSupported);
}

bool FireRenderGlobals::isOptionVarExist(std::string varName)
{
	int val;
	MGlobal::executeCommand(MString("optionVar -ex ") + varName.c_str(), val);
	return val > 0;
}

void FireRenderGlobals::setOptionVarInt(std::string varName, int val)
{
	MGlobal::executeCommand(MString("optionVar -iv ") + varName.c_str() + " " + std::to_string(val).c_str());
}

bool FireRenderGlobals::isOverrideThreadCount()
{
	int val;
	MGlobal::executeCommand("optionVar -q RPR_OverrideCPUThreadCount", val);

	return val > 0;
}

int FireRenderGlobals::getCPUThreadCount()
{


	int val;
	MGlobal::executeCommand("optionVar -q RPR_CPUThreadCount", val);

	return val;
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
	eAttr.addField("Bilateral", kBilateral);
	eAttr.addField("LWR", kLWR);
	eAttr.addField("EAW", kEAW);
	MAKE_INPUT_CONST(eAttr);
	nAttr.setReadable(true);
	CHECK_MSTATUS(addAttribute(Attribute::denoiserType));

	// Radio buttons attributes
	/*Attribute::denoiserIsBilateral = nAttr.create("denoiserIsBilateral", "dib", MFnNumericData::kBoolean, true, &status);
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(addAttribute(Attribute::denoiserIsBilateral));

	Attribute::denoiserIsLWR = nAttr.create("denoiserIsLWR", "dil", MFnNumericData::kBoolean, false, &status);
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(addAttribute(Attribute::denoiserIsLWR));

	Attribute::denoiserIsEAW = nAttr.create("denoiserIsEAW", "die", MFnNumericData::kBoolean, false, &status);
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(addAttribute(Attribute::denoiserIsEAW));
	// Radio buttons end*/


	//_TODO Will be removed after ImageProcLibrary would be fixed
	int limit = 0;
	status = MGlobal::executeCommand("source \"createFireRenderGlobalsTab.mel\"; isLimitBileteralRadius()", limit);

	// Bilateral
	Attribute::denoiserRadius = nAttr.create("denoiserRadius", "dr", MFnNumericData::kInt, 1, &status);
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(addAttribute(Attribute::denoiserRadius));
	nAttr.setMin(1);
	nAttr.setMax(limit ? 6 : 10);

	//_TODO Will be removed after ImageProcLibrary would be fixed
	Attribute::limitDenoiserRadius = nAttr.create("limitDenoiserRadius", "ldr", MFnNumericData::kBoolean, 0, &status);
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(addAttribute(Attribute::limitDenoiserRadius));
	MObject obj;
	GetRadeonProRenderGlobals(obj);
	m_attributeChangedCallback = MNodeMessage::addAttributeChangedCallback(obj, FireRenderGlobals::onAttributeChanged, nullptr, &status);
	CHECK_MSTATUS(status);
	////////

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
}

void FireRenderGlobals::onAttributeChanged(MNodeMessage::AttributeMessage msg, MPlug &plug, MPlug &otherPlug, void *clientData)
{
	if (plug.name() == "RadeonProRenderGlobals.limitDenoiserRadius")
	{
		bool limit = false;
		if (plug.getValue(limit) == MStatus::kSuccess)
		{
			MFnNumericAttribute(Attribute::denoiserRadius).setMax(limit ? 6 : 10);
		}
	}
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
