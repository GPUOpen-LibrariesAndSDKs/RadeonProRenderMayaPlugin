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
	nAttr.setDefault(1.0f, 1.0f, 1.0f);
	MAKE_INPUT(nAttr);

	Attribute::output = nAttr.createColor("outColor", "oc");
	MAKE_OUTPUT(nAttr);

	CHECK_MSTATUS(addAttribute(Attribute::color));
	CHECK_MSTATUS(addAttribute(Attribute::output));

	CHECK_MSTATUS(attributeAffects(Attribute::color, Attribute::output));

	return MS::kSuccess;
}

frw::Shader FireMaya::Passthrough::GetShader(Scope& scope)
{
	MFnDependencyNode shaderNode(thisMObject());

	frw::Shader shader(scope.MaterialSystem(), frw::ShaderTypeFlatColor);
	shader.SetValue("color", scope.GetValue(shaderNode.findPlug(Attribute::color)));

	return shader;
}
