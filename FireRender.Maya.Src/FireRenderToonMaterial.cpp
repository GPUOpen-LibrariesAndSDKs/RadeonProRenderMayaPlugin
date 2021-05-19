#include "FireRenderToonMaterial.h"
#include "FireMaya.h"

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

		// ramp
		MObject highlightColor;
		MObject midColor;
		MObject shadowColor;
		MObject rampPosition1;
		MObject rampPosition2;
		MObject rampRange1;
		MObject rampRange2;
	}
}

// Register attribute and make it affecting output color and alpha
#define ADD_ATTRIBUTE(attr) \
	CHECK_MSTATUS(addAttribute(attr)); \
	CHECK_MSTATUS(attributeAffects(attr, Attribute::output));


MStatus FireMaya::ToonMaterial::initialize()
{
	MFnNumericAttribute nAttr;

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

	Attribute::highlightColor = nAttr.createColor("highlightColor", "hc");
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(0.8, 0.8, 0.8));

	Attribute::midColor = nAttr.createColor("midColor", "mc");
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(0.4, 0.4, 0.4));

	Attribute::shadowColor = nAttr.createColor("shadowColor", "sc");
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(0.0, 0.0, 0.0));

	// Mid Level
	Attribute::rampPosition1 = nAttr.create("rampPosition1", "rp1", MFnNumericData::kFloat, 0.5);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0.0);
	nAttr.setMax(1.0);

	// Highlight Level
	Attribute::rampPosition2 = nAttr.create("rampPosition2", "rp2", MFnNumericData::kFloat, 0.8);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0.0);
	nAttr.setMax(1.0);

	// Mid Level Mix
	Attribute::rampRange1 = nAttr.create("rampRange1", "rr1", MFnNumericData::kFloat, 0.1);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0.0);
	nAttr.setMax(1.0);

	// Highlight Level Mix
	Attribute::rampRange2 = nAttr.create("rampRange2", "rr2", MFnNumericData::kFloat, 0.1);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0.0);
	nAttr.setMax(1.0);

	// Adding all attributes to the node type
	addAttribute(Attribute::output);

	ADD_ATTRIBUTE(Attribute::showAdvanced);
	ADD_ATTRIBUTE(Attribute::showMixLevels);

	ADD_ATTRIBUTE(Attribute::color);
	ADD_ATTRIBUTE(Attribute::normal);
	ADD_ATTRIBUTE(Attribute::roughness);

	ADD_ATTRIBUTE(Attribute::highlightColor);
	ADD_ATTRIBUTE(Attribute::midColor);
	ADD_ATTRIBUTE(Attribute::shadowColor);

	ADD_ATTRIBUTE(Attribute::rampPosition1);
	ADD_ATTRIBUTE(Attribute::rampPosition2);

	ADD_ATTRIBUTE(Attribute::rampRange1);
	ADD_ATTRIBUTE(Attribute::rampRange2);

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

frw::Shader FireMaya::ToonMaterial::GetShader(Scope& scope)
{
	MFnDependencyNode shaderNode(thisMObject());

	frw::Shader shader(scope.MaterialSystem(), frw::ShaderTypeToon);

	bool advancedMode = shaderNode.findPlug(Attribute::showAdvanced, false).asBool();
	bool mixLevels = shaderNode.findPlug(Attribute::showMixLevels, false).asBool();

	if (advancedMode)
	{
		shader.SetValue(RPR_MATERIAL_INPUT_COLOR, frw::Value(1.0, 1.0, 1.0, 1.0));

		frw::ToonRampNode toonRamp(scope.MaterialSystem());

		toonRamp.SetValue(RPR_MATERIAL_INPUT_HIGHLIGHT, scope.GetValue(shaderNode.findPlug(Attribute::highlightColor, false)));
		toonRamp.SetValue(RPR_MATERIAL_INPUT_MID, scope.GetValue(shaderNode.findPlug(Attribute::midColor, false)));
		toonRamp.SetValue(RPR_MATERIAL_INPUT_SHADOW, scope.GetValue(shaderNode.findPlug(Attribute::shadowColor, false)));

		toonRamp.SetValue(RPR_MATERIAL_INPUT_POSITION1, scope.GetValue(shaderNode.findPlug(Attribute::rampPosition1, false)));
		toonRamp.SetValue(RPR_MATERIAL_INPUT_POSITION2, scope.GetValue(shaderNode.findPlug(Attribute::rampPosition2, false)));

		if (mixLevels)
		{
			toonRamp.SetValueInt(RPR_MATERIAL_INPUT_INTERPOLATION, RPR_INTERPOLATION_MODE_LINEAR);
			toonRamp.SetValue(RPR_MATERIAL_INPUT_RANGE1, scope.GetValue(shaderNode.findPlug(Attribute::rampRange1, false)));
			toonRamp.SetValue(RPR_MATERIAL_INPUT_RANGE2, scope.GetValue(shaderNode.findPlug(Attribute::rampRange2, false)));
		}
		else
		{
			toonRamp.SetValueInt(RPR_MATERIAL_INPUT_INTERPOLATION, RPR_INTERPOLATION_MODE_NONE);
		}

		shader.SetValue(RPR_MATERIAL_INPUT_DIFFUSE_RAMP, toonRamp);
	}
	else
	{
		shader.SetValue(RPR_MATERIAL_INPUT_COLOR, scope.GetValue(shaderNode.findPlug(Attribute::color, false)));
	}

	shader.SetValue(RPR_MATERIAL_INPUT_ROUGHNESS, scope.GetValue(shaderNode.findPlug(Attribute::roughness, false)));

	frw::Value normalValue = scope.GetValue(shaderNode.findPlug(Attribute::normal, false));

	int type = normalValue.GetNodeType();

	if (type == frw::ValueTypeNormalMap || type == frw::ValueTypeBumpMap)
	{
		shader.SetValue(RPR_MATERIAL_INPUT_NORMAL, normalValue);
	}

	return shader;
}
