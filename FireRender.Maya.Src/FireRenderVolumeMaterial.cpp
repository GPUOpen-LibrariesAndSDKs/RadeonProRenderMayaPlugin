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
#include "FireRenderVolumeMaterial.h"
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MDataHandle.h>
#include <maya/MFloatVector.h>

#include "FireMaya.h"

namespace
{
	namespace Attribute
	{
		MObject color;
		MObject density;
		MObject weight;

		MObject	output;
	}
}

// Attributes
void FireMaya::VolumeMaterial::postConstructor()
{
	setMPSafe(true);
}


// creates an instance of the node
void* FireMaya::VolumeMaterial::creator()
{
	return new VolumeMaterial;
}

// initializes attribute information
// call by MAYA when this plug-in was loaded.
//
MStatus FireMaya::VolumeMaterial::initialize()
{
	MFnNumericAttribute nAttr;
	MFnEnumAttribute eAttr;

	Attribute::color = nAttr.createColor("color", "c");
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(1.0f, 1.0f, 1.0f));

 	Attribute::density = nAttr.create("density", "d", MFnNumericData::kFloat, 100.);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0.0);
	nAttr.setMax(1000.f);

	Attribute::weight = nAttr.create("weight", "w", MFnNumericData::kFloat, 1.);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0.0);
	nAttr.setMax(1.f);

	// output color
	Attribute::output = nAttr.createColor("outColor", "oc");
	MAKE_OUTPUT(nAttr);

	CHECK_MSTATUS(addAttribute(Attribute::color));
	CHECK_MSTATUS(addAttribute(Attribute::density));
	CHECK_MSTATUS(addAttribute(Attribute::weight));
	CHECK_MSTATUS(addAttribute(Attribute::output));

	CHECK_MSTATUS(attributeAffects(Attribute::color, Attribute::output));
	CHECK_MSTATUS(attributeAffects(Attribute::density, Attribute::output));
	CHECK_MSTATUS(attributeAffects(Attribute::weight, Attribute::output));

	return MS::kSuccess;
}

MStatus FireMaya::VolumeMaterial::compute(const MPlug& plug, MDataBlock& block)
{
	if ((plug == Attribute::output) || (plug.parent() == Attribute::output))
	{
		// We need to get all attributes which affect outputs in order to recalculate all dependent nodes
		// It needs to get IPR properly updating while changing attributes on the "left" nodes in dependency graph
		ForceEvaluateAllAttributes(true);

		MFloatVector& surfaceColor = block.inputValue(Attribute::color).asFloatVector();

		// set color attribute
		MDataHandle outColorHandle = block.outputValue(Attribute::output);
		MFloatVector& outColor = outColorHandle.asFloatVector();
		outColor = surfaceColor;
		outColorHandle.setClean();
		block.setClean(plug);
	}
	else
		return MS::kUnknownParameter;

	return MS::kSuccess;
}

frw::Shader FireMaya::VolumeMaterial::GetShader(Scope& scope)
{
	return frw::TransparentShader(scope.MaterialSystem());
}

frw::Shader FireMaya::VolumeMaterial::GetVolumeShader(Scope& scope)
{
	auto ms = scope.MaterialSystem();
	MFnDependencyNode shaderNode(thisMObject());

	frw::Shader material = frw::Shader(ms, frw::ShaderTypeVolume);

	auto color = scope.GetValue(shaderNode.findPlug(Attribute::color, false));
	auto density = shaderNode.findPlug(Attribute::density, false).asFloat();
	auto weight = shaderNode.findPlug(Attribute::weight, false).asFloat();

	// density
	material.SetValue(RPR_MATERIAL_INPUT_DENSITY, density);

	// color
	material.SetValue(RPR_MATERIAL_INPUT_COLOR, color);

	// weight
	material.SetValue(RPR_MATERIAL_INPUT_WEIGHT, weight);

	return material;
}
