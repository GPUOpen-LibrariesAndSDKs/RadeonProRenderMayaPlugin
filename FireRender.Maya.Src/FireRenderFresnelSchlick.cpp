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

	Attribute::output = nAttr.create("out", "o", MFnNumericData::kFloat, 0.0);
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

frw::Value FireMaya::FresnelSchlick::GetValue(const Scope& scope) const
{
	MFnDependencyNode shaderNode(thisMObject());

	frw::ValueNode valueNode(scope.MaterialSystem(), frw::ValueTypeFresnelSchlick);
	valueNode.SetValue(RPR_MATERIAL_INPUT_REFLECTANCE, scope.GetValue(shaderNode.findPlug(Attribute::reflectance, false)));
	valueNode.SetValue(RPR_MATERIAL_INPUT_NORMAL, scope.GetConnectedValue(shaderNode.findPlug(Attribute::normal, false)));
	valueNode.SetValue(RPR_MATERIAL_INPUT_INVEC, scope.GetConnectedValue(shaderNode.findPlug(Attribute::inVec, false)));

	return valueNode;
}
