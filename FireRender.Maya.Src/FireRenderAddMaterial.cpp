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
#include "FireRenderAddMaterial.h"

#include <cassert>

#include <maya/MGlobal.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnNumericData.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFloatVector.h>
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>

#include "FireMaya.h"

namespace
{
	namespace Attribute
	{
		MObject	inputA;
		MObject	inputB;
		MObject output;
		MObject disableSwatch;
		MObject swatchIterations;
	}
}


void* FireMaya::AddMaterial::creator()
{
	return new AddMaterial();
}

MStatus FireMaya::AddMaterial::initialize()
{
	MFnNumericAttribute nAttr;
	MFnTypedAttribute tAttr;

	Attribute::inputA = nAttr.createColor("color0", "c0");
	MAKE_INPUT(nAttr);
	nAttr.setDefault(0.0, 0.0, 0.0);

	Attribute::inputB = nAttr.createColor("color1", "c1");
	MAKE_INPUT(nAttr);
	nAttr.setDefault(0.0, 0.0, 0.0);

	Attribute::output = nAttr.createColor("outColor", "oc");
	MAKE_OUTPUT(nAttr);

	Attribute::disableSwatch = nAttr.create("disableSwatch", "ds", MFnNumericData::kBoolean, 0);
	MAKE_INPUT(nAttr);

	Attribute::swatchIterations = nAttr.create("swatchIterations", "swi", MFnNumericData::kInt, 4);
	MAKE_INPUT(nAttr);
	nAttr.setMin(1);
	nAttr.setMax(64);

	CHECK_MSTATUS(addAttribute(Attribute::inputA));
	CHECK_MSTATUS(addAttribute(Attribute::inputB));
	CHECK_MSTATUS(addAttribute(Attribute::output));

	CHECK_MSTATUS(attributeAffects(Attribute::inputA, Attribute::output));
	CHECK_MSTATUS(attributeAffects(Attribute::inputB, Attribute::output));

	CHECK_MSTATUS(addAttribute(Attribute::disableSwatch));
	CHECK_MSTATUS(addAttribute(Attribute::swatchIterations));

	return MS::kSuccess;
}

MStatus FireMaya::AddMaterial::compute(const MPlug& plug, MDataBlock& block)
{
	if ((plug == Attribute::output) || (plug.parent() == Attribute::output))
	{
		MFloatVector& topColor = block.inputValue(Attribute::inputA).asFloatVector();
		MFloatVector& baseColor = block.inputValue(Attribute::inputB).asFloatVector();

		// set ouput color attribute
		MDataHandle outColorHandle = block.outputValue(Attribute::output);
		MFloatVector& outColor = outColorHandle.asFloatVector();
		outColor = topColor + baseColor;
		outColorHandle.setClean();
		block.setClean(plug);
	}
	else
		return MS::kUnknownParameter;

	return MS::kSuccess;
}

frw::Shader FireMaya::AddMaterial::GetShader(Scope& scope)
{
	MFnDependencyNode shaderNode(thisMObject());

	auto a = scope.GetShader(shaderNode.findPlug(Attribute::inputA));
	auto b = scope.GetShader(shaderNode.findPlug(Attribute::inputB));

	return scope.MaterialSystem().ShaderAdd(a,b);
}