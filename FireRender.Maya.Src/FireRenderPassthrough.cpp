#include "FireRenderPassthrough.h"

namespace
{
	namespace Attribute
	{
		MObject	color;
		MObject	output;
	}
}


void* FireMaya::Passthrough::creator()
{
	return new FireMaya::Passthrough;
}

MStatus FireMaya::Passthrough::initialize()
{
	MFnNumericAttribute nAttr;

	Attribute::color = nAttr.createColor("color", "c");
	MAKE_INPUT(nAttr);

	Attribute::output = nAttr.createColor("out", "o");
	MAKE_OUTPUT(nAttr);

	CHECK_MSTATUS(addAttribute(Attribute::color));
	CHECK_MSTATUS(addAttribute(Attribute::output));

	CHECK_MSTATUS(attributeAffects(Attribute::color, Attribute::output));

	return MS::kSuccess;
}

frw::Value FireMaya::Passthrough::GetValue(Scope& scope)
{
	MFnDependencyNode shaderNode(thisMObject());

	frw::ValueNode valueNode(scope.MaterialSystem(), frw::ValueTypePassthrough);
	valueNode.SetValue("color", scope.GetValue(shaderNode.findPlug(Attribute::color)));

	return valueNode;
}