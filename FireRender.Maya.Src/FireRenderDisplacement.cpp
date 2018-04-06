#include "FireRenderDisplacement.h"

#include "FireRenderTexture.h"

#include <maya/MFnNumericAttribute.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnTypedAttribute.h>

#include <maya/MFnGenericAttribute.h>
#include <maya/MFnUnitAttribute.h>

// Attributes
namespace
{
	namespace Attribute
	{
		MObject map;
		MObject minHeight;
		MObject maxHeight;
		MObject subdivision;
		MObject creaseWeight;
		MObject boundary;

		MObject output;
	}
}

void* FireMaya::Displacement::creator()
{
	return new FireMaya::Displacement;
}

MStatus FireMaya::Displacement::initialize()
{
	MFnNumericAttribute nAttr;
	MFnEnumAttribute eAttr;

	Attribute::map = nAttr.createColor("map", "m");
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(0.644f, 0.644f, 0.644f));

	Attribute::minHeight = nAttr.create("minHeight", "miH", MFnNumericData::kFloat, 0.0);
	MAKE_INPUT(nAttr);
	nAttr.setMin(-1.0f);
	nAttr.setSoftMax(1.0f);
	nAttr.setMax(1.0f);

	Attribute::maxHeight = nAttr.create("maxHeight", "maH", MFnNumericData::kFloat, 0.0);
	MAKE_INPUT(nAttr);
	nAttr.setMin(-1.0f);
	nAttr.setSoftMax(1.0f);
	nAttr.setMax(1.0f);

	Attribute::subdivision = nAttr.create("subdivision", "sub", MFnNumericData::kInt, 0.0);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0.0f);
	nAttr.setSoftMax(10.0f);

	Attribute::creaseWeight = nAttr.create("creaseWeight", "cw", MFnNumericData::kFloat, 6.0);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0.0f);
	nAttr.setSoftMax(1000.0f);
	nAttr.setMax(9999999.0f);

	Attribute::boundary = eAttr.create("boundary", "b", 0);
	eAttr.addField("Edge", kDisplacement_EdgeOnly);
	eAttr.addField("Edge And Corner", kDisplacement_EdgeAndCorner);
	MAKE_INPUT_CONST(eAttr);



	// output color
	Attribute::output = nAttr.createColor("outColor", "oc");
	MAKE_OUTPUT(nAttr);

	////
	CHECK_MSTATUS(addAttribute(Attribute::map));
	CHECK_MSTATUS(addAttribute(Attribute::minHeight));
	CHECK_MSTATUS(addAttribute(Attribute::maxHeight));
	CHECK_MSTATUS(addAttribute(Attribute::subdivision));
	CHECK_MSTATUS(addAttribute(Attribute::creaseWeight));
	CHECK_MSTATUS(addAttribute(Attribute::boundary));

	CHECK_MSTATUS(addAttribute(Attribute::output));

	//////
	CHECK_MSTATUS(attributeAffects(Attribute::map, Attribute::output));
	CHECK_MSTATUS(attributeAffects(Attribute::minHeight, Attribute::output));
	CHECK_MSTATUS(attributeAffects(Attribute::maxHeight, Attribute::output));
	CHECK_MSTATUS(attributeAffects(Attribute::subdivision, Attribute::output));
	CHECK_MSTATUS(attributeAffects(Attribute::creaseWeight, Attribute::output));
	CHECK_MSTATUS(attributeAffects(Attribute::boundary, Attribute::output));

	return MS::kSuccess;
}

frw::Shader FireMaya::Displacement::GetShader(Scope& scope)
{
	return 0;
}

bool FireMaya::Displacement::getValues(frw::Value &map, Scope &scope, float &minHeight, float &maxHeight, int &subdivision, float &creaseWeight, int &boundary)
{
	MFnDependencyNode shaderNode(thisMObject());
	frw::Value color = scope.GetConnectedValue(shaderNode.findPlug(Attribute::map));

	map = color;
	bool haveMap = color.IsNode();

	MPlug plug = shaderNode.findPlug("minHeight");
	if (!plug.isNull())
		plug.getValue(minHeight);

	plug = shaderNode.findPlug("maxHeight");
	if (!plug.isNull())
		plug.getValue(maxHeight);

	plug = shaderNode.findPlug("subdivision");
	if (!plug.isNull())
		plug.getValue(subdivision);

	plug = shaderNode.findPlug("creaseWeight");
	if (!plug.isNull())
		plug.getValue(creaseWeight);

	plug = shaderNode.findPlug("boundary");
	if (!plug.isNull())
	{
		int n = 0;
		if (MStatus::kSuccess == plug.getValue(n))
		{
			Type b = static_cast<Type>(n);
			if (b == kDisplacement_EdgeAndCorner)
			{
				boundary = RPR_SUBDIV_BOUNDARY_INTERFOP_TYPE_EDGE_AND_CORNER;
			}
			else
			{
				boundary = RPR_SUBDIV_BOUNDARY_INTERFOP_TYPE_EDGE_ONLY;
			}
		}
	}

	return haveMap;
}