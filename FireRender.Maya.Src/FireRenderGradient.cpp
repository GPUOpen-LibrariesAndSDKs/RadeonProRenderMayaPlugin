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
#include "FireRenderGradient.h"

namespace
{
	namespace Attribute
	{
		MObject	uv;
		MObject	color0;
		MObject	color1;
		MObject	output;
	}
}

void* FireMaya::Gradient::creator()
{
	return new Gradient;
}

MStatus FireMaya::Gradient::initialize()
{
	MFnNumericAttribute nAttr;
	Attribute::uv = nAttr.create("uvCoord", "uv", MFnNumericData::k2Float);
	MAKE_INPUT(nAttr);

	Attribute::color0 = nAttr.createColor("color0", "c0");
	MAKE_INPUT(nAttr);
	Attribute::color1 = nAttr.createColor("color1", "c1");
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(1.f, 1.f, 1.f));

	Attribute::output = nAttr.createColor("out", "o");
	MAKE_OUTPUT(nAttr);

	CHECK_MSTATUS(addAttribute(Attribute::uv));
	CHECK_MSTATUS(addAttribute(Attribute::color0));
	CHECK_MSTATUS(addAttribute(Attribute::color1));
	CHECK_MSTATUS(addAttribute(Attribute::output));

	CHECK_MSTATUS(attributeAffects(Attribute::uv, Attribute::output));
	CHECK_MSTATUS(attributeAffects(Attribute::color0, Attribute::output));
	CHECK_MSTATUS(attributeAffects(Attribute::color1, Attribute::output));

	return MS::kSuccess;
}

frw::Value FireMaya::Gradient::GetValue(const Scope& scope) const
{
	MFnDependencyNode shaderNode(thisMObject());

	frw::ValueNode valueNode(scope.MaterialSystem(), frw::ValueTypeGradientMap);

	valueNode.SetValue(RPR_MATERIAL_INPUT_UV, scope.GetConnectedValue(shaderNode.findPlug(Attribute::uv, false)));
	valueNode.SetValue(RPR_MATERIAL_INPUT_COLOR0, scope.GetValue(shaderNode.findPlug(Attribute::color0, false)));
	valueNode.SetValue(RPR_MATERIAL_INPUT_COLOR1, scope.GetValue(shaderNode.findPlug(Attribute::color1, false)));

	return valueNode;
}