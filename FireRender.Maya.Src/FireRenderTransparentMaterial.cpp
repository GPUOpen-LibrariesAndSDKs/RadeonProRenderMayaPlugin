#include "FireRenderTransparentMaterial.h"
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MDataHandle.h>
#include <maya/MFloatVector.h>

#include "FireMaya.h"

namespace
{
	namespace Attribute
	{
		MObject color;
		MObject	output;
	}
}

// Attributes
void FireMaya::TransparentMaterial::postConstructor()
{
	setMPSafe(true);
}


// creates an instance of the node
void* FireMaya::TransparentMaterial::creator()
{
	return new TransparentMaterial;
}

// initializes attribute information
// call by MAYA when this plug-in was loaded.
MStatus FireMaya::TransparentMaterial::initialize()
{
	MFnNumericAttribute nAttr;
	MFnEnumAttribute eAttr;

	Attribute::color = nAttr.createColor("color", "dc");
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(0.5f, 0.5f, 0.5f));

	// output color
	Attribute::output = nAttr.createColor("outColor", "oc");
	MAKE_OUTPUT(nAttr);

	CHECK_MSTATUS(addAttribute(Attribute::color));
	CHECK_MSTATUS(addAttribute(Attribute::output));

	CHECK_MSTATUS(attributeAffects(Attribute::color, Attribute::output));

	return MS::kSuccess;
}

MStatus FireMaya::TransparentMaterial::compute(const MPlug& plug, MDataBlock& block)
{
	if ((plug == Attribute::output) || (plug.parent() == Attribute::output))
	{
		MFloatVector& surfaceColor = block.inputValue(Attribute::color).asFloatVector();

		// set output color attribute
		MDataHandle outColorHandle = block.outputValue(Attribute::output);
		MFloatVector& outColor = outColorHandle.asFloatVector();
		outColor = surfaceColor;
		outColorHandle.setClean();
		block.setClean(plug);
	}
	else
		return MS::kUnknownParameter;

	return MS::kSuccess;
}

frw::Shader FireMaya::TransparentMaterial::GetShader(Scope& scope)
{
	auto ms = scope.MaterialSystem();

	MFnDependencyNode shaderNode(thisMObject());

	frw::Shader material = frw::Shader(ms, frw::ShaderTypeTransparent);

	material.SetValue("color", scope.GetValue(shaderNode.findPlug(Attribute::color)));
	return material;
}

