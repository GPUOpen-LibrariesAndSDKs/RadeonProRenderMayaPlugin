/**********************************************************************
Copyright 2020 Advanced Micro Devices, Inc
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
********************************************************************/
#include "FireRenderLookup.h"
#include <maya/MFnEnumAttribute.h>

namespace
{
	namespace Attribute
	{
		MObject type;

		MObject	output;
		MObject outputW;
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
	eAttr.addField("Random Primitive Color", frw::LookupTypePrimitiveRandomColor);
	eAttr.addField("Object ID", frw::LookupTypeObjectID);
	MAKE_INPUT_CONST(eAttr);

	Attribute::output = nAttr.createPoint("out", "o");
	MAKE_OUTPUT(nAttr);
	Attribute::outputW = nAttr.create("outW", "w", MFnNumericData::kFloat);
	MAKE_OUTPUT(nAttr);

	CHECK_MSTATUS(addAttribute(Attribute::type));
	CHECK_MSTATUS(addAttribute(Attribute::output));
	CHECK_MSTATUS(addAttribute(Attribute::outputW));

	CHECK_MSTATUS(attributeAffects(Attribute::type, Attribute::output));
	CHECK_MSTATUS(attributeAffects(Attribute::type, Attribute::outputW));

	return MStatus::kSuccess;
}


void* FireMaya::Lookup::creator()
{
	return new Lookup;
}

frw::Value FireMaya::Lookup::GetValue(const Scope& scope) const
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
		return frw::PrimvarLookupNode(scope.MaterialSystem(), 0); // we use primvar channel with number zero to store vertex colors
	}

	return frw::LookupNode(scope.MaterialSystem(), type);
}
