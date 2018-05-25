#include "FireRenderBlendValue.h"

#include <maya/MFloatVector.h>

namespace
{
	namespace Attribute
	{
		MObject	inputA;
		MObject	inputB;
		MObject weight;

		MObject	output;
	}
}


MStatus FireMaya::BlendValue::initialize()
{
	MFnNumericAttribute nAttr;

	Attribute::inputA = nAttr.createColor("inputA", "a");
	MAKE_INPUT(nAttr);
	Attribute::inputB = nAttr.createColor("inputB", "b");
	MAKE_INPUT(nAttr);
	Attribute::weight = nAttr.create("weight", "w", MFnNumericData::kFloat, 0.5f);
	MAKE_INPUT(nAttr);

	nAttr.setSoftMin(0);
	nAttr.setSoftMax(1);

	Attribute::output = nAttr.createColor("out", "o");
	MAKE_OUTPUT(nAttr);

	CHECK_MSTATUS(addAttribute(Attribute::inputA));
	CHECK_MSTATUS(addAttribute(Attribute::inputB));
	CHECK_MSTATUS(addAttribute(Attribute::weight));
	CHECK_MSTATUS(addAttribute(Attribute::output));

	CHECK_MSTATUS(attributeAffects(Attribute::inputA, Attribute::output));
	CHECK_MSTATUS(attributeAffects(Attribute::inputB, Attribute::output));
	CHECK_MSTATUS(attributeAffects(Attribute::weight, Attribute::output));

	return MS::kSuccess;
}


void* FireMaya::BlendValue::creator()
{
	return new BlendValue;
}

frw::Value FireMaya::BlendValue::GetValue(Scope& scope)
{
	MFnDependencyNode shaderNode(thisMObject());

	auto a = scope.GetValue(shaderNode.findPlug(Attribute::inputA));
	auto b = scope.GetValue(shaderNode.findPlug(Attribute::inputB));
	auto t = scope.GetValue(shaderNode.findPlug(Attribute::weight));

	return scope.MaterialSystem().ValueBlend(a, b, t);
}
