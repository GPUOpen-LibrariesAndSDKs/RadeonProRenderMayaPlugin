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
#include "FireRenderDoublesided.h"
#include "FireRenderMaterial.h"

#include <maya/MFnEnumAttribute.h>
#include <maya/MFloatVector.h>

namespace
{
	namespace Attribute
	{
		MObject	inputFront;
		MObject	inputBack;

		MObject	output;
	}
}


MStatus FireMaya::RPRDoubleSided::initialize()
{
	MFnNumericAttribute nAttr;

	Attribute::inputFront = nAttr.createColor("inputFront", "a");
	MAKE_INPUT(nAttr);
	nAttr.setDefault(0.0, 0.0, 0.0);

	Attribute::inputBack = nAttr.createColor("inputBack", "b");
	MAKE_INPUT(nAttr);
	nAttr.setDefault(0.0, 0.0, 0.0);

	Attribute::output = nAttr.createColor("outColor", "oc");
	MAKE_OUTPUT(nAttr);

	CHECK_MSTATUS(addAttribute(Attribute::inputFront));
	CHECK_MSTATUS(addAttribute(Attribute::inputBack));
	CHECK_MSTATUS(addAttribute(Attribute::output));

	CHECK_MSTATUS(attributeAffects(Attribute::inputFront, Attribute::output));
	CHECK_MSTATUS(attributeAffects(Attribute::inputBack, Attribute::output));

	return MS::kSuccess;
}

MStatus FireMaya::RPRDoubleSided::compute(const MPlug& plug, MDataBlock& block)
{
	if ((plug == Attribute::output) || (plug.parent() == Attribute::output))
	{
		MFloatVector& topColor = block.inputValue(Attribute::inputFront).asFloatVector();
		MFloatVector& baseColor = block.inputValue(Attribute::inputBack).asFloatVector();

		// set ouput color attribute
		MDataHandle outColorHandle = block.outputValue(Attribute::output);
		MFloatVector& outColor = outColorHandle.asFloatVector();
		outColor = topColor; // maybe add something better in the future?
		outColorHandle.setClean();
		block.setClean(plug);
	}
	else
		return MS::kUnknownParameter;

	return MS::kSuccess;
}

void* FireMaya::RPRDoubleSided::creator()
{
	return new RPRDoubleSided;
}

frw::Shader FireMaya::RPRDoubleSided::GetShader(Scope& scope)
{
	MFnDependencyNode shaderNode(thisMObject());

	frw::Shader inputFrontShdr = scope.GetShader(shaderNode.findPlug(Attribute::inputFront, false));
	frw::Shader inputBackShdr = scope.GetShader(shaderNode.findPlug(Attribute::inputBack, false));

	return scope.MaterialSystem().ShaderDoubleSided(inputFrontShdr, inputBackShdr);
}

