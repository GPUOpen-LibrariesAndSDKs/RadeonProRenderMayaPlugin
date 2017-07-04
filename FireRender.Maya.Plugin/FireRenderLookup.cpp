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

	MPlug plug = shaderNode.findPlug(Attribute::type);
	if (!plug.isNull())
		type = static_cast<frw::LookupType>(plug.asInt());

	return frw::LookupNode(scope.MaterialSystem(), type);
}
