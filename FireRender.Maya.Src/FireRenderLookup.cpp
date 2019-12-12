#include "FireRenderLookup.h"
#include <maya/MFnEnumAttribute.h>

namespace
{
	namespace Attribute
	{
		MObject type;

		MObject	output;
	}
}



MStatus FireMaya::Lookup::initialize()
{
	MFnNumericAttribute nAttr;
	MFnEnumAttribute eAttr;

	Attribute::type = eAttr.create("type", "t", frw::LookupTypeUV0);
	eAttr.addField("UV0", frw::LookupTypeUV0);
	eAttr.addField("UV1", frw::LookupTypeUV1);
	eAttr.addField("Incident", frw::LookupTypeIncident);
	eAttr.addField("Normal", frw::LookupTypeNormal);
	eAttr.addField("World XYZ", frw::LookupTypePosition);
	eAttr.addField("Out Vector", frw::LookupTypeOutVector);
	eAttr.addField("Object XYZ", frw::LookupTypePositionLocal);
	eAttr.addField("Vertex Color", frw::LookupTypeVertexColor);
	eAttr.addField("Random Color", frw::LookupTypeShapeRandomColor);
	MAKE_INPUT_CONST(eAttr);

	Attribute::output = nAttr.createPoint("out", "o");
	MAKE_OUTPUT(nAttr);

	CHECK_MSTATUS(addAttribute(Attribute::type));
	CHECK_MSTATUS(addAttribute(Attribute::output));

	CHECK_MSTATUS(attributeAffects(Attribute::type, Attribute::output));

	return MStatus::kSuccess;
}


void* FireMaya::Lookup::creator()
{
	return new Lookup;
}

frw::Value FireMaya::Lookup::GetValue(Scope& scope)
{
	MFnDependencyNode shaderNode(thisMObject());

	auto type = frw::LookupTypeUV0;

	MPlug plug = shaderNode.findPlug(Attribute::type, false);
	if (!plug.isNull())
	{
		type = static_cast<frw::LookupType>(plug.asInt());
	}

	if (type == frw::LookupTypeVertexColor)
	{
		return CombineVertexColor(scope.MaterialSystem());
	}

	return frw::LookupNode(scope.MaterialSystem(), type);
}

frw::Value FireMaya::Lookup::CombineVertexColor(const frw::MaterialSystem& materialSystem) const
{
	frw::LookupNode redChannel(materialSystem, frw::LookupTypeVertexValue0);
	frw::LookupNode greenChannel(materialSystem, frw::LookupTypeVertexValue1);
	frw::LookupNode blueChannel(materialSystem, frw::LookupTypeVertexValue2);
	frw::LookupNode alphaChannel(materialSystem, frw::LookupTypeVertexValue3);

	frw::ArithmeticNode redVector(materialSystem, frw::Operator::OperatorMultiply, redChannel, { 1.0f, 0.0f, 0.0f, 0.0f });
	frw::ArithmeticNode greenVector(materialSystem, frw::Operator::OperatorMultiply, greenChannel, { 0.0f, 1.0f, 0.0f, 0.0f });
	frw::ArithmeticNode blueVector(materialSystem, frw::Operator::OperatorMultiply, blueChannel, { 0.0f, 0.0f, 1.0f, 0.0f });
	frw::ArithmeticNode alphaVector(materialSystem, frw::Operator::OperatorMultiply, alphaChannel, { 0.0f, 0.0f, 0.0f, 1.0f });

	frw::ArithmeticNode add1(materialSystem, frw::Operator::OperatorAdd, redVector, greenVector);
	frw::ArithmeticNode add2(materialSystem, frw::Operator::OperatorAdd, add1, blueVector);
	return frw::ArithmeticNode(materialSystem, frw::Operator::OperatorAdd, add2, alphaVector);
}
