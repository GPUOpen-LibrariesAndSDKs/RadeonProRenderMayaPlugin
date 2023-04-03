#include "FireRenderToonMaterial.h"
#include "FireMaya.h"
#include "FireRenderUtils.h"
#include "Context/FireRenderContext.h"
#include <maya/MSelectionList.h>
#include <maya/MUuid.h>
#include <maya/MDGMessage.h>
#include <maya/MNodeMessage.h>
#include <maya/MFnMessageAttribute.h>
#include <maya/MRampAttribute.h>
#include "MayaStandardNodesSupport/RampNodeConverter.h"
#include "MayaStandardNodesSupport/NodeProcessingUtils.h"

namespace
{
	namespace Attribute
	{
		MObject output;

		MObject showAdvanced;
		MObject showMixLevels;

		// toon closure
		MObject color;
		MObject normal;
		MObject roughness;

		MObject transparencyEnable;
		MObject transparencyLevel;

		// ramp
		MObject input5Ramp;
		MObject enable5Colors;

		MObject highlightColor;
		MObject highlightColor2;
		MObject midColor;
		MObject shadowColor;
		MObject shadowColor2;

		MObject rampPositionShadow;
		MObject rampPosition1;
		MObject rampPosition2;
		MObject rampPositionHighlight;

		MObject rampRangeShadow;
		MObject rampRange1;
		MObject rampRange2;
		MObject rampRangeHighlight;

		// light linking
		MObject enableLightLinking;
		MObject linkedLightObject;

		// mid color as albedo
		MObject enableMidColorAsAlbedo;
	}
}

// Register attribute and make it affecting output color and alpha
#define ADD_ATTRIBUTE(attr) \
	CHECK_MSTATUS(addAttribute(attr)); \
	CHECK_MSTATUS(attributeAffects(attr, Attribute::output));


MStatus FireMaya::ToonMaterial::initialize()
{
	MFnNumericAttribute nAttr;
	MFnMessageAttribute msgAttr;
	MRampAttribute rAttr;

	Attribute::output = nAttr.createColor("outColor", "oc");
	MAKE_OUTPUT(nAttr);

	Attribute::showAdvanced = nAttr.create("showAdvanced", "sa", MFnNumericData::kBoolean, false);
	MAKE_INPUT(nAttr);
	nAttr.setConnectable(false);

	Attribute::showMixLevels = nAttr.create("showMixLevels", "sml", MFnNumericData::kBoolean, false);
	MAKE_INPUT(nAttr);
	nAttr.setConnectable(false);

	Attribute::color = nAttr.createColor("color", "c");
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(0.8, 0.8, 0.8));

	Attribute::normal = nAttr.createColor("normal", "n");
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(1.0f, 1.0f, 1.0f));

	Attribute::roughness = nAttr.create("roughness", "r", MFnNumericData::kFloat, 1.0);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0.0);
	nAttr.setMax(1.0);

	// transparency
	Attribute::transparencyLevel = nAttr.create("transparencyLevel", "trl", MFnNumericData::kFloat, 1.0);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0.0);
	nAttr.setMax(1.0);

	Attribute::transparencyEnable = nAttr.create("transparencyEnable", "et", MFnNumericData::kBoolean, 0);
	MAKE_INPUT_CONST(nAttr);
	
	// RAMP	
	MStatus status;
	Attribute::input5Ramp = rAttr.createColorRamp("input5Ramp", "ir", &status);
	CHECK_MSTATUS(status);
	status = MPxNode::addAttribute(Attribute::input5Ramp);
	CHECK_MSTATUS(status);

	Attribute::enable5Colors = nAttr.create("enable5Colors", "e5c", MFnNumericData::kBoolean, 0);
	MAKE_INPUT_CONST(nAttr);

	// Colors
	Attribute::highlightColor = nAttr.createColor("highlightColor", "hc");
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(0.8, 0.8, 0.8));

	Attribute::highlightColor2 = nAttr.createColor("highlightColor2", "hc2");
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(0.8, 0.8, 0.8));

	Attribute::midColor = nAttr.createColor("midColor", "mc");
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(0.4, 0.4, 0.4));

	Attribute::shadowColor = nAttr.createColor("shadowColor", "sc");
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(0.0, 0.0, 0.0));

	Attribute::shadowColor2 = nAttr.createColor("shadowColor2", "sc2");
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(0.0, 0.0, 0.0));

	// Levels - positions
	// Mid Level

	Attribute::rampPositionShadow = nAttr.create("rampPositionShadow", "rps", MFnNumericData::kFloat, 0.2);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0.0);
	nAttr.setMax(1.0);

	Attribute::rampPosition1 = nAttr.create("rampPosition1", "rp1", MFnNumericData::kFloat, 0.5);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0.0);
	nAttr.setMax(1.0);

	// Highlight Level
	Attribute::rampPosition2 = nAttr.create("rampPosition2", "rp2", MFnNumericData::kFloat, 0.8);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0.0);
	nAttr.setMax(1.0);

	Attribute::rampPositionHighlight = nAttr.create("rampPositionHighlight", "rph", MFnNumericData::kFloat, 0.9);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0.0);
	nAttr.setMax(1.0);


	// Ranges
	// Mid Level Mix

	Attribute::rampRangeShadow = nAttr.create("rampRangeShadow", "rrs", MFnNumericData::kFloat, 0.1);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0.0);
	nAttr.setMax(1.0);

	Attribute::rampRange1 = nAttr.create("rampRange1", "rr1", MFnNumericData::kFloat, 0.1);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0.0);
	nAttr.setMax(1.0);

	// Highlight Level Mix
	Attribute::rampRange2 = nAttr.create("rampRange2", "rr2", MFnNumericData::kFloat, 0.1);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0.0);
	nAttr.setMax(1.0);

	Attribute::rampRangeHighlight = nAttr.create("rampRangeHighlight", "rrh", MFnNumericData::kFloat, 0.1);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0.0);
	nAttr.setMax(1.0);

	Attribute::enableLightLinking = nAttr.create("enableLightLinking", "ell", MFnNumericData::kBoolean, 0);
	MAKE_INPUT(nAttr);
	nAttr.setConnectable(false);

	Attribute::enableMidColorAsAlbedo = nAttr.create("enableMidColorAsAlbedo", "emcaa", MFnNumericData::kBoolean, 0);
	MAKE_INPUT(nAttr);
	nAttr.setConnectable(false);
	
	Attribute::linkedLightObject = msgAttr.create("linkedLightObject", "llo");
	MAKE_INPUT(msgAttr);
	nAttr.setConnectable(false);
	
	// Adding all attributes to the node type
	addAttribute(Attribute::output);

	ADD_ATTRIBUTE(Attribute::showAdvanced);
	ADD_ATTRIBUTE(Attribute::showMixLevels);

	ADD_ATTRIBUTE(Attribute::color);
	ADD_ATTRIBUTE(Attribute::normal);
	ADD_ATTRIBUTE(Attribute::roughness);

	ADD_ATTRIBUTE(Attribute::transparencyEnable);
	ADD_ATTRIBUTE(Attribute::transparencyLevel);

	ADD_ATTRIBUTE(Attribute::enable5Colors);
	ADD_ATTRIBUTE(Attribute::highlightColor);
	ADD_ATTRIBUTE(Attribute::highlightColor2);
	ADD_ATTRIBUTE(Attribute::midColor);
	ADD_ATTRIBUTE(Attribute::shadowColor);
	ADD_ATTRIBUTE(Attribute::shadowColor2);
	
	ADD_ATTRIBUTE(Attribute::rampPositionShadow);
	ADD_ATTRIBUTE(Attribute::rampPosition1);
	ADD_ATTRIBUTE(Attribute::rampPosition2);
	ADD_ATTRIBUTE(Attribute::rampPositionHighlight);

	ADD_ATTRIBUTE(Attribute::rampRangeShadow);
	ADD_ATTRIBUTE(Attribute::rampRange1);
	ADD_ATTRIBUTE(Attribute::rampRange2);
	ADD_ATTRIBUTE(Attribute::rampRangeHighlight);

	ADD_ATTRIBUTE(Attribute::enableLightLinking);
	ADD_ATTRIBUTE(Attribute::enableMidColorAsAlbedo);
	
	ADD_ATTRIBUTE(Attribute::linkedLightObject);

	return MStatus::kSuccess;
}

// creates an instance of the node
void* FireMaya::ToonMaterial::creator()
{
	return new ToonMaterial;
}

MStatus FireMaya::ToonMaterial::compute(const MPlug& plug, MDataBlock& block)
{
	if ((plug == Attribute::output) || (plug.parent() == Attribute::output))
	{
		block.setClean(plug);

		return MS::kSuccess;
	}
	else
	{
		return MS::kUnknownParameter;
	}
}

using CtrlPointDataT = std::tuple<MString, MPlug>;

template <typename T>
bool ProcessRampElementAttribute(MPlug& rampPlug, std::vector<RampCtrlPoint<T>>& out)
{
	MStatus status;
	MObject node = rampPlug.attribute();
	auto& ctrlPoint = out.back();

	MFn::Type dataType = node.apiType();
	switch (dataType)
	{
	case MFn::kAttribute3Float:
	{
		// color value
		// - this is compouind attribute of 3 floats RGB
		std::get<MPlug>(ctrlPoint.ctrlPointData) = rampPlug;
		std::get<MString>(ctrlPoint.ctrlPointData) = rampPlug.name(&status);
		assert(status == MStatus::kSuccess);
		break;
	}
	case MFn::kNumericAttribute:
	{
		// control point position
		float positionValue = rampPlug.asFloat(&status);
		assert(status == MStatus::kSuccess);
		ctrlPoint.position = positionValue;
		break;
	}
	case MFn::kEnumAttribute:
	{
		// this is interpolation; ignored
		break;
	}
	default:
		return false;
	}

	return true;
}

template <typename T>
bool ReadRampCtrlPoints(MPlug rampPlug, std::vector<RampCtrlPoint<T>>& out)
{
	out.clear();

	assert(!rampPlug.isNull());

	MStatus status;
	bool isArray = rampPlug.isArray(&status);
	if (!isArray)
		return false;

	unsigned int count = rampPlug.numElements();
	if (count == 0)
		return false;

	out.reserve(count);

	// this is executed for each element of array plug rampPlug
	auto func = [](MPlug& rampElementPlug, std::vector<RampCtrlPoint<T>>& out)->bool
	{
		out.emplace_back();
		RampCtrlPoint<T>& ctrlPoint = out.back();
		ctrlPoint.method = InterpolationMethod::kLinear;
		ctrlPoint.index = (unsigned int)out.size() - 1;
		return MayaStandardNodeConverters::ForEachPlugInCompoundPlug<std::vector<RampCtrlPoint<T>>>(rampElementPlug, out, ProcessRampElementAttribute<T>);
	};

	// creates ramp control point from attributes data in ramp attribute rampPlug
	using outT = decltype(out);
	bool res = MayaStandardNodeConverters::ForEachPlugInArrayPlug<outT>(rampPlug, out, func);
	if (!res)
		return false;

	// sort values by position (maya can returns control point in random order)
	std::sort(out.begin(), out.end(), [](auto first, auto second)->bool {
		return (first.position < second.position); });

	return true;
}

frw::Shader FireMaya::ToonMaterial::GetShader(Scope& scope)
{
	MFnDependencyNode shaderNode(thisMObject());

	frw::Shader shader(scope.MaterialSystem(), frw::ShaderTypeToon);

	bool advancedMode = shaderNode.findPlug(Attribute::showAdvanced, false).asBool();
	bool mixLevels = shaderNode.findPlug(Attribute::showMixLevels, false).asBool();

	shader.SetValue(RPR_MATERIAL_INPUT_COLOR, scope.GetValue(shaderNode.findPlug(Attribute::color, false)));

	if (advancedMode)
	{
		frw::ToonRampNode toonRamp(scope.MaterialSystem());

		int use5ColorsParam = shaderNode.findPlug(Attribute::enable5Colors, false).asBool() ? 1 : 0;
		toonRamp.SetValueInt(RPR_MATERIAL_INPUT_TOON_5_COLORS, use5ColorsParam);

		// read input ramp
		MPlug ctrlPointsPlug = shaderNode.findPlug(Attribute::input5Ramp, false);
		std::vector<RampCtrlPoint<CtrlPointDataT>> rampCtrlPoints;
		const bool success = ReadRampCtrlPoints<CtrlPointDataT>(ctrlPointsPlug, rampCtrlPoints);
		assert(success);
		if (!success)
			return frw::Shader();

		// set shader parameters from ramp
		if ((use5ColorsParam != 0) && (rampCtrlPoints.size() == 5))
		{
			toonRamp.SetValue(RPR_MATERIAL_INPUT_HIGHLIGHT2, scope.GetValue(std::get<MPlug>(rampCtrlPoints[4].ctrlPointData)));
			toonRamp.SetValue(RPR_MATERIAL_INPUT_HIGHLIGHT, scope.GetValue(std::get<MPlug>(rampCtrlPoints[3].ctrlPointData)));
			toonRamp.SetValue(RPR_MATERIAL_INPUT_MID, scope.GetValue(std::get<MPlug>(rampCtrlPoints[2].ctrlPointData)));
			toonRamp.SetValue(RPR_MATERIAL_INPUT_SHADOW, scope.GetValue(std::get<MPlug>(rampCtrlPoints[1].ctrlPointData)));
			toonRamp.SetValue(RPR_MATERIAL_INPUT_SHADOW2, scope.GetValue(std::get<MPlug>(rampCtrlPoints[0].ctrlPointData)));

			toonRamp.SetValue(RPR_MATERIAL_INPUT_POSITION_SHADOW, frw::Value(rampCtrlPoints[1].position));
			toonRamp.SetValue(RPR_MATERIAL_INPUT_POSITION1, frw::Value(rampCtrlPoints[2].position));
			toonRamp.SetValue(RPR_MATERIAL_INPUT_POSITION2, frw::Value(rampCtrlPoints[3].position));
			toonRamp.SetValue(RPR_MATERIAL_INPUT_POSITION_HIGHLIGHT, frw::Value(rampCtrlPoints[4].position));
		}
		else
		{
			toonRamp.SetValue(RPR_MATERIAL_INPUT_HIGHLIGHT, scope.GetValue(std::get<MPlug>(rampCtrlPoints[2].ctrlPointData)));
			toonRamp.SetValue(RPR_MATERIAL_INPUT_MID, scope.GetValue(std::get<MPlug>(rampCtrlPoints[1].ctrlPointData)));
			toonRamp.SetValue(RPR_MATERIAL_INPUT_SHADOW, scope.GetValue(std::get<MPlug>(rampCtrlPoints[0].ctrlPointData)));

			toonRamp.SetValue(RPR_MATERIAL_INPUT_POSITION_SHADOW, frw::Value(rampCtrlPoints[0].position));
			toonRamp.SetValue(RPR_MATERIAL_INPUT_POSITION1, frw::Value(rampCtrlPoints[1].position));
			toonRamp.SetValue(RPR_MATERIAL_INPUT_POSITION2, frw::Value(rampCtrlPoints[2].position));
		}

		if (mixLevels)
		{
			toonRamp.SetValueInt(RPR_MATERIAL_INPUT_INTERPOLATION, RPR_INTERPOLATION_MODE_LINEAR);

			toonRamp.SetValue(RPR_MATERIAL_INPUT_RANGE_SHADOW, scope.GetValue(shaderNode.findPlug(Attribute::rampRangeShadow, false)));
			toonRamp.SetValue(RPR_MATERIAL_INPUT_RANGE1, scope.GetValue(shaderNode.findPlug(Attribute::rampRange1, false)));
			toonRamp.SetValue(RPR_MATERIAL_INPUT_RANGE2, scope.GetValue(shaderNode.findPlug(Attribute::rampRange2, false)));
			toonRamp.SetValue(RPR_MATERIAL_INPUT_RANGE_HIGHLIGHT, scope.GetValue(shaderNode.findPlug(Attribute::rampRangeHighlight, false)));
		}
		else
		{
			toonRamp.SetValueInt(RPR_MATERIAL_INPUT_INTERPOLATION, RPR_INTERPOLATION_MODE_NONE);
		}

		shader.SetValue(RPR_MATERIAL_INPUT_DIFFUSE_RAMP, toonRamp);
	}

	shader.SetValue(RPR_MATERIAL_INPUT_ROUGHNESS, scope.GetValue(shaderNode.findPlug(Attribute::roughness, false)));

	frw::Value normalValue = scope.GetValue(shaderNode.findPlug(Attribute::normal, false));

	const int type = normalValue.GetNodeType();

	if (type == frw::ValueTypeNormalMap || type == frw::ValueTypeBumpMap || type == frw::ValueTypeBevel)
	{
		shader.SetValue(RPR_MATERIAL_INPUT_NORMAL, normalValue);
	}

	// If transperency on
	if (shaderNode.findPlug(Attribute::transparencyEnable, false).asBool())
	{
		frw::Shader transparentShader = frw::Shader(scope.MaterialSystem(), frw::ShaderTypeTransparent);

		frw::Value transparancyLevel = scope.GetValue(shaderNode.findPlug(Attribute::transparencyLevel, false));
		return scope.MaterialSystem().ShaderBlend(shader, transparentShader, transparancyLevel);
	}

	if (shaderNode.findPlug(Attribute::enableLightLinking, false).asBool())
	{
		linkLight(scope, shader);
	}

	shader.SetValueInt(RPR_MATERIAL_INPUT_MID_IS_ALBEDO, shaderNode.findPlug(Attribute::enableMidColorAsAlbedo, false).asBool());
	
	return shader;
}

void FireMaya::ToonMaterial::postConstructor()
{
	ShaderNode::postConstructor();
}

void FireMaya::ToonMaterial::linkLight(Scope& scope, frw::Shader& shader)
{
	const RenderType renderType = scope.GetIContextInfo()->GetRenderType();
	if (renderType == RenderType::Thumbnail || renderType == RenderType::Undefined)
	{
		return; // skip if render mode is swatch
	}

	MFnDependencyNode shaderNode(thisMObject());
	
	MObject light;
	MPlug linkedLightObjectPlug = shaderNode.findPlug("linkedLightObject", false);
	if (!linkedLightObjectPlug.isNull())
	{
		MPlugArray plugArray;

		if (linkedLightObjectPlug.connectedTo(plugArray, true, false))
		{
			if (plugArray.length() > 0)
			{
				light = plugArray[0].node();
			}
		}
	}

	if (light.isNull())
	{
		MGlobal::displayError("Unable to find linked light!");
		return;
	}

	MString name = MFnDependencyNode(light).name();

	FireRenderContext* pContext = dynamic_cast<FireRenderContext*>(scope.GetIContextInfo());
	if (!pContext)
	{
		return;
	}

	frw::Light rprLight = pContext->GetLightSceneObjectFromMObject(light);

	if (!rprLight.IsValid())
	{
		return;
	}

	pContext->GetScope().SetLastLinkedLight(light);
	shader.LinkLight(rprLight);
}

bool checkIsLight(MObject& node)
{
	MFnDependencyNode depNode(node);
	MString type = depNode.typeName();
	return node.hasFn(MFn::kLight) || type == "RPRPhysicalLight" || type == "RPRIES" || type == "RPRIBL";
}


FireMaya::ToonMaterial::~ToonMaterial()
{
}
