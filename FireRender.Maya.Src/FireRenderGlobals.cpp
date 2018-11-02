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

    struct RenderingDeviceAttributes
    {
        //MObject useGPU;
        //std::vector<MObject> gpuToUse;

        MObject cpuThreadCount;
        MObject overrideCpuThreadCount; // bool
    };

    namespace FinalRenderAttributes
    {
        RenderingDeviceAttributes renderingDevices;

        MObject samplesBetweenRenderUpdate;
        MObject tileRenderEnabled;
        MObject tileRenderX;
        MObject tileRenderY;
    }

	namespace ViewportRenderAttributes
	{
		// System tab (will be kept with Maya's preferences)
		RenderingDeviceAttributes renderingDevices;
		MObject maxRayDepth;
		MObject maxDiffuseRayDepth;
		MObject maxDepthGlossy;

		MObject thumbnailIterCount;
		MObject renderMode;
		MObject motionBlur;

		// Other tabs
		MObject completionCriteriaType;
		MObject completionCriteriaHours;
		MObject completionCriteriaMinutes;
		MObject completionCriteriaSeconds;
		MObject completionCriteriaIterations;
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

	Attribute::renderMode = createRenderModeAttr("renderMode", "rm", eAttr);

	Attribute::giClampIrradiance = nAttr.create("giClampIrradiance", "gici", MFnNumericData::kBoolean, true, &status);
	MAKE_INPUT(nAttr);

	Attribute::giClampIrradianceValue = nAttr.create("giClampIrradianceValue", "giciv", MFnNumericData::kFloat, 1.0, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(1.0);
	nAttr.setSoftMax(100.0f);
	nAttr.setMax(999999.f);

	/*Attribute::AASampleCountProduction = nAttr.create("samples", "s", MFnNumericData::kShort, 1, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(1);
	nAttr.setMax(32);
	Attribute::AASampleCountViewport = nAttr.create("samplesViewport", "sV", MFnNumericData::kShort, 1, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(1);
	nAttr.setMax(32);*/

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

	setupProductionRayDepthParameters();

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
//	CHECK_MSTATUS(addAttribute(Attribute::AASampleCountProduction));
	CHECK_MSTATUS(addAttribute(Attribute::MaxRayDepthProduction));
	CHECK_MSTATUS(addAttribute(Attribute::RaycastEpsilon));
	CHECK_MSTATUS(addAttribute(Attribute::EnableOOC));
	CHECK_MSTATUS(addAttribute(Attribute::TexCacheSize));
//	CHECK_MSTATUS(addAttribute(Attribute::AASampleCountViewport));
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

	CHECK_MSTATUS(addAttribute(m_useRenderStamp));
	CHECK_MSTATUS(addAttribute(m_renderStampText));
	CHECK_MSTATUS(addAttribute(m_renderStampTextDefault));

	createDenoiserAttributes();

	return status;
}

void FireRenderGlobals::setupProductionRayDepthParameters()
{
	int min = 0;
	int softMin = 2;
	int softMax = 50;

	MFnNumericAttribute nAttr;
	MStatus status;

	Attribute::MaxRayDepthProduction = nAttr.create("maxRayDepth", "mrd", MFnNumericData::kInt, 8, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0);
	nAttr.setSoftMin(softMin);
	nAttr.setSoftMax(softMax);
	CHECK_MSTATUS(addAttribute(Attribute::MaxRayDepthProduction));

	Attribute::MaxDepthDiffuse = nAttr.create("maxDepthDiffuse", "mdd", MFnNumericData::kInt, 3, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0);
	nAttr.setSoftMin(softMin);
	nAttr.setSoftMax(softMax);
	CHECK_MSTATUS(addAttribute(Attribute::MaxDepthDiffuse));

	Attribute::MaxDepthGlossy = nAttr.create("maxDepthGlossy", "mdg", MFnNumericData::kInt, 5, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0);
	nAttr.setSoftMin(softMin);
	nAttr.setSoftMax(softMax);
	CHECK_MSTATUS(addAttribute(Attribute::MaxDepthGlossy));

	Attribute::MaxDepthRefraction = nAttr.create("maxDepthRefraction", "mdr", MFnNumericData::kInt, 5, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0);
	nAttr.setSoftMin(softMin);
	nAttr.setSoftMax(softMax);
	CHECK_MSTATUS(addAttribute(Attribute::MaxDepthRefraction));

	Attribute::MaxDepthRefractionGlossy = nAttr.create("maxDepthRefractionGlossy", "mdrg", MFnNumericData::kInt, 5, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0);
	nAttr.setSoftMin(softMin);
	nAttr.setSoftMax(softMax);
	CHECK_MSTATUS(addAttribute(Attribute::MaxDepthRefractionGlossy));

	Attribute::MaxDepthShadow = nAttr.create("maxDepthShadow", "mds", MFnNumericData::kInt, 5, &status);
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

	int driversCompatibleExists;

	// _TODO Refactor names, make const strings
	MGlobal::executeCommand("optionVar -q RPR_DevicesName", oldList);
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

	std::vector<std::string> attrNames = { FINAL_RENDER_DEVICES_USING_PARAM_NAME, VIEWPORT_DEVICES_USING_PARAM_NAME };

	for (std::string name : attrNames)
	{
		bool hardwareChanged = !(oldList == newList) || !(oldDriversCompatible == newDriversCompatible) || !driversCompatibleExists;
		// First time, or something has changed, so time to reset defaults.
		int varExists = false;
		MGlobal::executeCommand(("optionVar -ex " + name).c_str(), varExists);
		if (hardwareChanged || !varExists)
		{
			MGlobal::executeCommand(("optionVar -rm " + name).c_str());
			int selectedCount = 0;
			for (auto device : deviceList)
			{
				int selected = selectedCount < 1 && device.isCertified() && device.isDriverCompatible;
				MGlobal::executeCommand(MString("optionVar -iva ") + name.c_str() + " " + selected);
				if (selected)
					selectedCount++;
			}
		}
	}
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
}

void FireRenderGlobals::addAsGlobalAttribute(MFnAttribute& attr)
{
	MObject attrObj = attr.object();
	
	attr.setStorable(false);
	attr.setConnectable(false);
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

    FinalRenderAttributes::samplesBetweenRenderUpdate = nAttr.create("samplesBetweenRenderUpdate", "sbr", MFnNumericData::kFloat, 10.0, &status);
    MAKE_INPUT(nAttr);
    nAttr.setMin(0.1f);
    nAttr.setSoftMax(100.0f);
	addAsGlobalAttribute(nAttr);

    FinalRenderAttributes::tileRenderEnabled = nAttr.create("tileRenderEnabled", "tre", MFnNumericData::kBoolean, false, &status);
    MAKE_INPUT(nAttr);
	addAsGlobalAttribute(nAttr);

    const int tileDefaultSize = 128;
    const int tileDefaultSizeMin = 16;
    const int tileDefaultSizeMax = 512;

    FinalRenderAttributes::tileRenderX = nAttr.create("tileRenderX", "trx", MFnNumericData::kInt, tileDefaultSize, &status);
    MAKE_INPUT(nAttr);
    nAttr.setMin(tileDefaultSizeMin);
    nAttr.setSoftMax(tileDefaultSizeMax);
	nAttr.setStorable(false);
	nAttr.setConnectable(false);
	addAsGlobalAttribute(nAttr);

    FinalRenderAttributes::tileRenderY = nAttr.create("tileRenderY", "try", MFnNumericData::kInt, tileDefaultSize, &status);
    MAKE_INPUT(nAttr);
    nAttr.setMin(tileDefaultSizeMin);
    nAttr.setSoftMax(tileDefaultSizeMax);
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

	ViewportRenderAttributes::completionCriteriaType = eAttr.create("completionCriteriaTypeViewport", "vcct", kIterations, &status);
	eAttr.addField("Iterations", kIterations);
	eAttr.addField("Time", kTime);
	eAttr.addField("Unlimited", kUnlimited);
	MAKE_INPUT_CONST(eAttr);
	addAsGlobalAttribute(eAttr);

	ViewportRenderAttributes::completionCriteriaHours = nAttr.create("completionCriteriaHoursViewport", "vcch", MFnNumericData::kInt, 0, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0.0);
	nAttr.setSoftMax(24.0);
	nAttr.setMax(INT_MAX);
	addAsGlobalAttribute(nAttr);

	ViewportRenderAttributes::completionCriteriaMinutes = nAttr.create("completionCriteriaMinutesViewport", "vccm", MFnNumericData::kInt, 0, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0.0);
	nAttr.setMax(59.0);
	addAsGlobalAttribute(nAttr);

	ViewportRenderAttributes::completionCriteriaSeconds = nAttr.create("completionCriteriaSecondsViewport", "vccs", MFnNumericData::kInt, 10, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(1.0);
	nAttr.setMax(59.0);
	addAsGlobalAttribute(nAttr);

	ViewportRenderAttributes::completionCriteriaIterations = nAttr.create("completionCriteriaIterationsViewport", "vcci", MFnNumericData::kInt, 100, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(1.0);
	nAttr.setSoftMax(1000);
	nAttr.setMax(INT_MAX);
	addAsGlobalAttribute(nAttr);

	ViewportRenderAttributes::thumbnailIterCount = nAttr.create("thumbnailIterationCount", "tic", MFnNumericData::kInt, 50, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(1);
	nAttr.setSoftMax(100);
	nAttr.setMax(INT_MAX);
	addAsGlobalAttribute(nAttr);

	ViewportRenderAttributes::renderMode = createRenderModeAttr("renderModeViewport", "vrm", eAttr);
	addAsGlobalAttribute(eAttr);

	int softMin = 2;
	int softMax = 50;

	ViewportRenderAttributes::maxRayDepth = nAttr.create("maxRayDepthViewport", "vmrd", MFnNumericData::kInt, 8, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(softMin);
	nAttr.setSoftMin(softMin);
	nAttr.setSoftMax(softMax);
	addAsGlobalAttribute(nAttr);

	ViewportRenderAttributes::maxDiffuseRayDepth = nAttr.create("maxDepthDiffuseViewport", "mddv", MFnNumericData::kInt, 3, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(softMin);
	nAttr.setSoftMin(softMin);
	nAttr.setSoftMax(softMax);
	addAsGlobalAttribute(nAttr);

	ViewportRenderAttributes::maxDepthGlossy = nAttr.create("maxDepthGlossyViewport", "mdgv", MFnNumericData::kInt, 5, &status);
	MAKE_INPUT(nAttr);
	nAttr.setMin(softMin);
	nAttr.setSoftMin(softMin);
	nAttr.setSoftMax(softMax);
	addAsGlobalAttribute(nAttr);

	ViewportRenderAttributes::motionBlur = nAttr.create("motionBlurViewport", "vmb", MFnNumericData::kBoolean, false);
	MAKE_INPUT(nAttr);
	addAsGlobalAttribute(nAttr);
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

	for (MObject attrObj : m_globalAttributesList)
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
