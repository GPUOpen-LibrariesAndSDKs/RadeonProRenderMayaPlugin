#include "FireRenderNoise.h"

namespace
{
	namespace Attribute
	{
		MObject	uv;
		MObject	color;
		MObject	output;
	}
}


void* FireMaya::Noise::creator()
{
	return new FireMaya::Noise;
}

MStatus FireMaya::Noise::initialize()
{
	MFnNumericAttribute nAttr;

	Attribute::uv = nAttr.create("uvCoord", "uv", MFnNumericData::k2Float);
	MAKE_INPUT(nAttr);

	Attribute::color = nAttr.createColor("color", "c");
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(0.644f, 0.644f, 0.644f));

	Attribute::output = nAttr.createColor("out", "o");
	MAKE_OUTPUT(nAttr);

	CHECK_MSTATUS(addAttribute(Attribute::uv));
	CHECK_MSTATUS(addAttribute(Attribute::color));
	CHECK_MSTATUS(addAttribute(Attribute::output));

	CHECK_MSTATUS(attributeAffects(Attribute::uv, Attribute::output));
	CHECK_MSTATUS(attributeAffects(Attribute::color, Attribute::output));

	return MS::kSuccess;
}

frw::Value FireMaya::Noise::GetValue(const Scope& scope) const
{
	MFnDependencyNode shaderNode(thisMObject());

	frw::ValueNode valueNode(scope.MaterialSystem(), frw::ValueTypeNoiseMap);
	valueNode.SetValue(RPR_MATERIAL_INPUT_UV, scope.GetConnectedValue(shaderNode.findPlug(Attribute::uv, false)));
	valueNode.SetValue(RPR_MATERIAL_INPUT_COLOR, scope.GetValue(shaderNode.findPlug(Attribute::color, false)));

	return valueNode;
}