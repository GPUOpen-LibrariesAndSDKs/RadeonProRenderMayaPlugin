#include "FireRenderGradient.h"

namespace
{
	namespace Attribute
	{
		MObject	uv;
		MObject	color0;
		MObject	color1;
		MObject	output;
	}
}

void* FireMaya::Gradient::creator()
{
	return new Gradient;
}

MStatus FireMaya::Gradient::initialize()
{
	MFnNumericAttribute nAttr;
	Attribute::uv = nAttr.create("uvCoord", "uv", MFnNumericData::k2Float);
	MAKE_INPUT(nAttr);

	Attribute::color0 = nAttr.createColor("color0", "c0");
	MAKE_INPUT(nAttr);
	Attribute::color1 = nAttr.createColor("color1", "c1");
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(1.f, 1.f, 1.f));

	Attribute::output = nAttr.createColor("out", "o");
	MAKE_OUTPUT(nAttr);

	CHECK_MSTATUS(addAttribute(Attribute::uv));
	CHECK_MSTATUS(addAttribute(Attribute::color0));
	CHECK_MSTATUS(addAttribute(Attribute::color1));
	CHECK_MSTATUS(addAttribute(Attribute::output));

	CHECK_MSTATUS(attributeAffects(Attribute::uv, Attribute::output));
	CHECK_MSTATUS(attributeAffects(Attribute::color0, Attribute::output));
	CHECK_MSTATUS(attributeAffects(Attribute::color1, Attribute::output));

	return MS::kSuccess;
}

frw::Value FireMaya::Gradient::GetValue(Scope& scope)
{
	MFnDependencyNode shaderNode(thisMObject());

	frw::ValueNode valueNode(scope.MaterialSystem(), frw::ValueTypeGradientMap);

	valueNode.SetValue("uv", scope.GetConnectedValue(shaderNode.findPlug(Attribute::uv, false)));
	valueNode.SetValue("color0", scope.GetValue(shaderNode.findPlug(Attribute::color0, false)));
	valueNode.SetValue("color1", scope.GetValue(shaderNode.findPlug(Attribute::color1, false)));

	return valueNode;
}