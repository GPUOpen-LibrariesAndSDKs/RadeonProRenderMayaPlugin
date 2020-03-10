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

frw::Value FireMaya::BlendValue::GetValue(const Scope& scope) const
{
	MFnDependencyNode shaderNode(thisMObject());

	auto a = scope.GetValue(shaderNode.findPlug(Attribute::inputA, false));
	auto b = scope.GetValue(shaderNode.findPlug(Attribute::inputB, false));
	auto t = scope.GetValue(shaderNode.findPlug(Attribute::weight, false));

	return scope.MaterialSystem().ValueBlend(a, b, t);
}
