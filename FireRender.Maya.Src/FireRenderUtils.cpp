#include <GL/glew.h>
#include <../RprTools.h>

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
#include <cassert>
#include <vector>

#include "FireRenderContext.h"
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
	completionCriteriaType(0),
	completionCriteriaHours(0),
	completionCriteriaMinutes(0),
	completionCriteriaSeconds(0),
	completionCriteriaIterations(0),
	textureCompression(false),
	cellsizeProduction(1.0f),
	cellsizeViewport(1.0f),
	iterations(64),
	mode(0),
	giClampIrradiance(true),
	giClampIrradianceValue(1.0),
	samplesProduction(2),
	samplesViewport(2),
	filterType(0),
	filterSize(2),
	maxRayDepthProduction(2),
	maxRayDepthViewport(2),
	commandPort(0),
	useGround(false),
	groundHeight(0.0f),
	groundRadius(1.0f),
	useRenderStamp(false),
	renderStampText(""),
	shadows(false),
	reflections(false),
	strength(0.5f),
	roughness(0.0f),
	toneMappingType(0),
	toneMappingLinearScale(1.0f),
	toneMappingPhotolinearSensitivity(1.0f),
	toneMappingPhotolinearExposure(1.0f),
	toneMappingPhotolinearFstop(1.0f),
	toneMappingReinhard02Prescale(1.0f),
	toneMappingReinhard02Postscale(1.0f),
	toneMappingReinhard02Burn(1.0f),
	motionBlur(false),
	motionBlurCameraExposure(0.0f),
	motionBlurScale(0.0f),
	cameraType(0)
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
		MPlug plug = frGlobalsNode.findPlug("iterations");
		if (!plug.isNull())
			iterations = plug.asInt();


		plug = frGlobalsNode.findPlug("completionCriteriaType");
		if (!plug.isNull())
			completionCriteriaType = plug.asShort();

		plug = frGlobalsNode.findPlug("completionCriteriaHours");
		if (!plug.isNull())
			completionCriteriaHours = plug.asInt();

		plug = frGlobalsNode.findPlug("completionCriteriaMinutes");
		if (!plug.isNull())
			completionCriteriaMinutes = plug.asInt();

		plug = frGlobalsNode.findPlug("completionCriteriaSeconds");
		if (!plug.isNull())
			completionCriteriaSeconds = plug.asInt();

		plug = frGlobalsNode.findPlug("completionCriteriaIterations");
		if (!plug.isNull())
			completionCriteriaIterations = plug.asInt();


		plug = frGlobalsNode.findPlug("textureCompression");
		if (!plug.isNull())
			textureCompression = plug.asBool();


		plug = frGlobalsNode.findPlug("renderMode");
		if (!plug.isNull())
			mode = plug.asShort();

		plug = frGlobalsNode.findPlug("giClampIrradiance");
		if (!plug.isNull())
			giClampIrradiance = plug.asBool();
		plug = frGlobalsNode.findPlug("giClampIrradianceValue");
		if (!plug.isNull())
			giClampIrradianceValue = plug.asFloat();

		plug = frGlobalsNode.findPlug("samples");
		if (!plug.isNull())
			samplesProduction = plug.asShort();

		plug = frGlobalsNode.findPlug("samplesViewport");
		if (!plug.isNull())
			samplesViewport = plug.asShort();

		plug = frGlobalsNode.findPlug("cellSize");
		if (!plug.isNull())
			cellsizeProduction = plug.asFloat();

		plug = frGlobalsNode.findPlug("cellSizeViewport");
		if (!plug.isNull())
			cellsizeViewport = plug.asFloat();

		plug = frGlobalsNode.findPlug("filter");
		if (!plug.isNull())
			filterType = plug.asShort();

		plug = frGlobalsNode.findPlug("filterSize");
		if (!plug.isNull())
			filterSize = plug.asShort();

		plug = frGlobalsNode.findPlug("maxRayDepth");
		if (!plug.isNull())
			maxRayDepthProduction = plug.asShort();

		// In UI raycast epsilon defined in millimeters, convert it to meters
		plug = frGlobalsNode.findPlug("raycastEpsilon");
		if (!plug.isNull())
			raycastEpsilon = plug.asFloat() / 1000.f;

		plug = frGlobalsNode.findPlug("maxRayDepthViewport");
		if (!plug.isNull())
			maxRayDepthViewport = plug.asShort();

		plug = frGlobalsNode.findPlug("commandPort");
		if (!plug.isNull())
			commandPort = plug.asInt();

		plug = frGlobalsNode.findPlug("applyGammaToMayaViews");
		if (!plug.isNull())
			applyGammaToMayaViews = plug.asBool();

		plug = frGlobalsNode.findPlug("displayGamma");
		if (!plug.isNull())
			displayGamma = plug.asFloat();

		plug = frGlobalsNode.findPlug("useGround");
		if (!plug.isNull())
			useGround = plug.asBool();

		plug = frGlobalsNode.findPlug("groundHeight");
		if (!plug.isNull())
			groundHeight = plug.asFloat();

		plug = frGlobalsNode.findPlug("groundRadius");
		if (!plug.isNull())
			groundRadius = plug.asFloat();

		plug = frGlobalsNode.findPlug("useRenderStamp");
		if (!plug.isNull())
			useRenderStamp = plug.asBool();
		plug = frGlobalsNode.findPlug("renderStampText");
		if (!plug.isNull())
			renderStampText = plug.asString();

		plug = frGlobalsNode.findPlug("shadows");
		if (!plug.isNull())
			shadows = plug.asBool();

		plug = frGlobalsNode.findPlug("reflections");
		if (!plug.isNull())
			reflections = plug.asBool();

		plug = frGlobalsNode.findPlug("strength");
		if (!plug.isNull())
			strength = plug.asFloat();

		plug = frGlobalsNode.findPlug("roughness");
		if (!plug.isNull())
			roughness = plug.asFloat();

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

		plug = frGlobalsNode.findPlug("motionBlur");
		if (!plug.isNull())
			motionBlur = plug.asBool();

		plug = frGlobalsNode.findPlug("motionBlurCameraExposure");
		if (!plug.isNull())
			motionBlurCameraExposure = plug.asFloat();

		plug = frGlobalsNode.findPlug("motionBlurScale");
		if (!plug.isNull())
			motionBlurScale = plug.asFloat();

		plug = frGlobalsNode.findPlug("cameraType");
		if (!plug.isNull())
			cameraType = plug.asShort();

		aovs.readFromGlobals(frGlobalsNode);

		readDenoiserParameters(frGlobalsNode);
	});
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
}

short FireRenderGlobalsData::getMaxRayDepth(const FireRenderContext& context) const
{
	if (context.isInteractive())
		return maxRayDepthViewport;
	else
		return maxRayDepthProduction;
}

rpr_uint FireRenderGlobalsData::getCellSize(const FireRenderContext& context) const
{
	if (context.isInteractive())
		return static_cast<rpr_uint>(cellsizeViewport);
	else
		return static_cast<rpr_uint>(cellsizeProduction);
}

short FireRenderGlobalsData::getSamples(const FireRenderContext& context) const
{
	if (context.isInteractive())
		return samplesViewport;
	else
		return samplesProduction;
}

void FireRenderGlobalsData::updateTonemapping(FireRenderContext& inContext, bool disableWhiteBalance)
{
	frw::Context context = inContext.GetContext();
	rpr_context frcontext = context.Handle();

	rpr_int frstatus = RPR_SUCCESS;

	frstatus = rprContextSetParameter1f(frcontext, "texturegamma", textureGamma);
	checkStatus(frstatus);

	// Disable display gamma correction unless it is being applied
	// to Maya views. It will be always be enabled before file output.
	auto displayGammaValue = applyGammaToMayaViews ? displayGamma : 1.0f;
	frstatus = rprContextSetParameter1f(frcontext, "displaygamma", displayGammaValue);
	checkStatus(frstatus);

	// Release existing effects
	if (inContext.white_balance)
	{
		context.Detach(inContext.white_balance);
		inContext.white_balance.Reset();
	}

	if (inContext.simple_tonemap)
	{
		context.Detach(inContext.simple_tonemap);
		inContext.simple_tonemap.Reset();
	}

	if (inContext.tonemap)
	{
		context.Detach(inContext.tonemap);
		inContext.tonemap.Reset();
	}

	if (inContext.normalization)
	{
		context.Detach(inContext.normalization);
		inContext.normalization.Reset();
	}

	if (inContext.gamma_correction)
	{
		context.Detach(inContext.gamma_correction);
		inContext.gamma_correction.Reset();
	}

	// At least one post effect is required for frame buffer resolve to
	// work, which is required for OpenGL interop. Frame buffer normalization
	// should be applied before other post effects.
	// Also gamma effect requires normalization when tonemapping is not used.
	//if (inContext.isGLInteropActive() || (applyGammaToMayaViews && toneMappingType == 0))
	//{
	inContext.normalization = frw::PostEffect(context, frw::PostEffectTypeNormalization);
	context.Attach(inContext.normalization);
	//}

	// Create new effects
	switch (toneMappingType)
	{
	case 0:
		break;

	case 1:
		if (!inContext.tonemap)
		{
			inContext.tonemap = frw::PostEffect(context, frw::PostEffectTypeToneMap);
			context.Attach(inContext.tonemap);
		}
		context.SetParameter("tonemapping.type", RPR_TONEMAPPING_OPERATOR_LINEAR);
		context.SetParameter("tonemapping.linear.scale", toneMappingLinearScale);
		break;

	case 2:
		if (!inContext.tonemap)
		{
			inContext.tonemap = frw::PostEffect(context, frw::PostEffectTypeToneMap);
			context.Attach(inContext.tonemap);
		}
		context.SetParameter("tonemapping.type", RPR_TONEMAPPING_OPERATOR_PHOTOLINEAR);
		context.SetParameter("tonemapping.photolinear.sensitivity", toneMappingPhotolinearSensitivity);
		context.SetParameter("tonemapping.photolinear.fstop", toneMappingPhotolinearFstop);
		context.SetParameter("tonemapping.photolinear.exposure", toneMappingPhotolinearExposure);
		break;

	case 3:
		if (!inContext.tonemap)
		{
			inContext.tonemap = frw::PostEffect(context, frw::PostEffectTypeToneMap);
			context.Attach(inContext.tonemap);
		}
		context.SetParameter("tonemapping.type", RPR_TONEMAPPING_OPERATOR_AUTOLINEAR);
		break;

	case 4:
		if (!inContext.tonemap)
		{
			inContext.tonemap = frw::PostEffect(context, frw::PostEffectTypeToneMap);
			context.Attach(inContext.tonemap);
		}
		context.SetParameter("tonemapping.type", RPR_TONEMAPPING_OPERATOR_MAXWHITE);
		break;

	case 5:
		if (!inContext.tonemap)
		{
			inContext.tonemap = frw::PostEffect(context, frw::PostEffectTypeToneMap);
			context.Attach(inContext.tonemap);
		}
		context.SetParameter("tonemapping.type", RPR_TONEMAPPING_OPERATOR_REINHARD02);
		context.SetParameter("tonemapping.reinhard02.prescale", toneMappingReinhard02Prescale);
		context.SetParameter("tonemapping.reinhard02.postscale", toneMappingReinhard02Postscale);
		context.SetParameter("tonemapping.reinhard02.burn", toneMappingReinhard02Burn);
		break;

	default:
		break;
	}

	// This block is required for gamma functionality
	if (applyGammaToMayaViews)
	{
		inContext.gamma_correction = frw::PostEffect(context, frw::PostEffectTypeGammaCorrection);
		context.Attach(inContext.gamma_correction);
	}

	if (toneMappingWhiteBalanceEnabled && !disableWhiteBalance)
	{
		if (!inContext.white_balance)
		{
			inContext.white_balance = frw::PostEffect(context, frw::PostEffectTypeWhiteBalance);
			context.Attach(inContext.white_balance);
		}
		float temperature = toneMappingWhiteBalanceValue;
		inContext.white_balance.SetParameter("colorspace", RPR_COLOR_SPACE_SRGB); // check: Max uses Adobe SRGB here
		inContext.white_balance.SetParameter("colortemp", temperature);
	}
}

void FireRenderGlobalsData::setupContext(FireRenderContext& inContext, bool disableWhiteBalance)
{
	frw::Context context = inContext.GetContext();
	rpr_context frcontext = context.Handle();

	rpr_int frstatus = RPR_SUCCESS;

	frstatus = rprContextSetParameter1u(frcontext, "texturecompression", textureCompression);
	checkStatus(frstatus);

	frstatus = rprContextSetParameter1u(frcontext, "rendermode", mode);
	checkStatus(frstatus);

	frstatus = rprContextSetParameter1f(frcontext, "radianceclamp", giClampIrradiance ? giClampIrradianceValue : FLT_MAX);
	checkStatus(frstatus);

	frstatus = rprContextSetParameter1u(frcontext, "maxRecursion", getMaxRayDepth(inContext));
	checkStatus(frstatus);

	frstatus = rprContextSetParameter1u(frcontext, "imagefilter.type", filterType);
	checkStatus(frstatus);

	frstatus = rprContextSetParameter1u(frcontext, "aacellsize", getCellSize(inContext));
	checkStatus(frstatus);

	frstatus = rprContextSetParameter1u(frcontext, "aasamples", getSamples(inContext));
	checkStatus(frstatus);

	frstatus = rprContextSetParameter1f(frcontext, "pdfthreshold", 0.0000f);
	checkStatus(frstatus);

	//

	std::string filterAttrName = "imagefilter.box.radius";
	switch (filterType)
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

	frstatus = rprContextSetParameter1f(frcontext, filterAttrName.c_str(), filterSize);
	checkStatus(frstatus);

	updateTonemapping(inContext, disableWhiteBalance);
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

	static const std::vector<MString> propNames = { "denoiserEnabled", "denoiserType", "denoiserRadius", "denoiserSamples",
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

int FireMaya::Options::GetContextDeviceFlags()
{
	int ret = FireRenderThread::RunOnMainThread<int>([]
	{
		int ret = 0;
		MIntArray devicesUsing;
		MGlobal::executeCommand("optionVar -q RPR_DevicesSelected", devicesUsing);
		auto allDevices = HardwareResources::GetAllDevices();
		
		for (auto i = 0u; i < devicesUsing.length(); i++)
		{
			if (devicesUsing[i] && i < allDevices.size())
				ret |= allDevices[i].creationFlag;
		}

		if (!ret)
			ret = RPR_CREATION_FLAGS_ENABLE_CPU;

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

bool isGeometry(const MObject& node)
{
	return
		node.hasFn(MFn::kMesh) ||
		node.hasFn(MFn::kNurbsSurface) ||
		node.hasFn(MFn::kSubdiv);
}

bool isLight(const MObject& node)
{
	return node.hasFn(MFn::kLight);
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
		if (surfShaderPlug.isNull())
			continue;
		MPlugArray shaderConnections;
		surfShaderPlug.connectedTo(shaderConnections, true, false);
		if (shaderConnections.length() == 0)
			continue;
		MObject shaderNode = shaderConnections[0].node();
		shaders.append(shaderNode);
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
#ifndef MAYA2015
	return nodeFn.uuid().asString().asChar();
#else
	auto ptr = reinterpret_cast<const int*>(&node);
	char buf[64] = {};
	sprintf(buf, "%08X%08X", ptr[1], ptr[0]);
	return buf;
#endif

}

std::string getNodeUUid(const MDagPath& node)
{
	auto id = getNodeUUid(node.node());
	if (node.isInstanced())
	{
		char buf[1024] = {};
		int written = snprintf(buf, 1024, "%s:%u", id.c_str(), node.instanceNumber());
		assert(written >= 0 && written < 1024);
		id = buf;
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
		wsprintfW(suffix, L"%x", RPR_API_VERSION);

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

int areShadersCached() {
	std::string temp = getShaderCachePath().asChar();
	temp = replaceStrChar(temp, "\\", '/');
	MString test = "source common.mel; getNumberOfCachedShaders(\"" + MString(temp.c_str()) + "\");";
	int result = 0;
	MGlobal::executeCommand(test, result);
	return result != 0;
}

MString getLogFolder()
{
	auto path = MGlobal::executeCommandStringResult("getenv RPR_MAYA_TRACE_PATH");
	if (path.length() == 0)
	{
#ifdef WIN32
		PWSTR sz = nullptr;
		if (S_OK == ::SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &sz))
		{
			static wchar_t buff[256 + 1] = {};
			if (!buff[0])
			{
				auto t = time(NULL);
				auto tm = localtime(&t);
				wsprintfW(buff, L"%02d.%02d.%02d-%02d.%02d", tm->tm_year - 100, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min);
			}

			auto cacheFolder = std::wstring(sz) + L"\\RadeonProRender\\Maya\\Trace\\" + buff;
			switch (SHCreateDirectoryExW(nullptr, cacheFolder.c_str(), nullptr))
			{
			case ERROR_SUCCESS:
			case ERROR_FILE_EXISTS:
			case ERROR_ALREADY_EXISTS:
				path = cacheFolder.c_str();
			}
		}
#elif defined(OSMac_)
		path = "/Users/Shared/RadeonProRender/trace";
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

	//if no shaders encountered then consider it 1
	return shadingEngines ? shadingEngines : 1;
}

MDagPathArray getRenderableCameras()
{
	MDagPathArray cameras;
	MStatus status;
	MItDag itDag(MItDag::kDepthFirst, MFn::kCamera, &status);
	if (MStatus::kFailure == status) {
		MGlobal::displayError("MItDag::MItDag");
		return cameras;
	}

	for (; !itDag.isDone(); itDag.next())
	{
		MDagPath dagPath;
		status = itDag.getPath(dagPath);
		if (MStatus::kSuccess != status) {
			MGlobal::displayError("MDagPath::getPath");
			return cameras;
		}

		MFnDependencyNode camFn(dagPath.node());
		bool renderable = false;
		MPlug renderablePlug = camFn.findPlug("renderable");
		if (!renderablePlug.isNull())
		{
			renderablePlug.getValue(renderable);
		}

		if (renderable)
		{
			cameras.append(dagPath);
		}

	}
	return cameras;
}

MStatus GetRadeonProRenderGlobals(MObject& outGlobalsNode)
{
	MSelectionList selList;
	selList.add("RadeonProRenderGlobals");
	MObject fireRenderGlobals;
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
	auto dll = "/Users/Shared/RadeonProRender/Maya/lib/libTahoe64.dylib";
    if (0 != access(dll,F_OK))
    {
        dll = "/Users/Shared/RadeonProRender/lib/libTahoe64.dylib";
    }
#elif __linux__
	auto os = RPRTOS_LINUX;
	auto dll = "libTahoe64.so";
#else
	auto os = RPRTOS_WINDOWS;
	auto dll = "Tahoe64.dll";
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
        device.creationFlag = device.creationFlag | additionalFlags;
		device.compatibility = rprIsDeviceCompatible(dll, RPR_TOOLS_DEVICE(i), nullptr, doWhiteList, os, additionalFlags );
		if (device.compatibility == RPRTC_INCOMPATIBLE_UNCERTIFIED || device.compatibility == RPRTC_COMPATIBLE)
		{
			// Request device name. Earlier (before RPR 1.255) rprIsDeviceCompatible returned device name, however
			// this functionality was removed. We're doing "assert" here just in case because rprIsDeviceCompatible
			// does exactly the same right before this code.
			if (g_tahoePluginID == -1)
				g_tahoePluginID = rprRegisterPlugin(dll);
			rpr_int plugins[] = { g_tahoePluginID };
			size_t pluginCount = sizeof(plugins) / sizeof(plugins[0]);
			rpr_context temporaryContext = 0;
			rpr_int status = rprCreateContext(RPR_API_VERSION, plugins, pluginCount, device.creationFlag , NULL, NULL, &temporaryContext);
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
