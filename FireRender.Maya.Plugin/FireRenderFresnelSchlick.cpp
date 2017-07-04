#include "FireRenderFresnelSchlick.h"
#include <maya/MFnNumericAttribute.h>

#include "FireMaya.h"


namespace
{
	namespace Attribute
	{
		MObject reflectance;
		MObject normal;
		MObject inVec;
		MObject	output;
	}
}

void* FireMaya::FresnelSchlick::creator()
{
	return new FresnelSchlick;
}

MStatus FireMaya::FresnelSchlick::initialize()
{
	MFnNumericAttribute nAttr;
	Attribute::reflectance = nAttr.create("reflectance", "k", MFnNumericData::kFloat, 0.0);
	MAKE_INPUT(nAttr);
	nAttr.setSoftMin(0.0);
	nAttr.setSoftMax(1.0);

	Attribute::normal = nAttr.create("normalMap", "nm", MFnNumericData::k3Float, 0.0);
	MAKE_INPUT(nAttr);

	Attribute::inVec = nAttr.create("inVec", "inVec", MFnNumericData::k3Float, 0.0);
	MAKE_INPUT(nAttr);

	Attribute::output = nAttr.create("out", "o", MFnNumericData::k3Float, 0.0);
	MAKE_OUTPUT(nAttr);

	CHECK_MSTATUS(addAttribute(Attribute::reflectance));
	CHECK_MSTATUS(addAttribute(Attribute::normal));
	CHECK_MSTATUS(addAttribute(Attribute::inVec));
	CHECK_MSTATUS(addAttribute(Attribute::output));

	CHECK_MSTATUS(attributeAffects(Attribute::reflectance, Attribute::output));
	CHECK_MSTATUS(attributeAffects(Attribute::normal, Attribute::output));
	CHECK_MSTATUS(attributeAffects(Attribute::inVec, Attribute::output));

	return MS::kSuccess;
}

frw::Value FireMaya::FresnelSchlick::GetValue(Scope& scope)
{
	MFnDependencyNode shaderNode(thisMObject());

	frw::ValueNode valueNode(scope.MaterialSystem(), frw::ValueTypeFresnelSchlick);
	valueNode.SetValue("reflectance", scope.GetValue(shaderNode.findPlug(Attribute::reflectance)));
	valueNode.SetValue("normal", scope.GetConnectedValue(shaderNode.findPlug(Attribute::normal)));
	valueNode.SetValue("invec", scope.GetConnectedValue(shaderNode.findPlug(Attribute::inVec)));

	return valueNode;
}
