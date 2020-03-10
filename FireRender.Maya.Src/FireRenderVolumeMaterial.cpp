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
		MObject scatterColor;
		MObject transmissionColor;
		MObject emissionColor;
		MObject density;
		MObject scatteringDirection;
		MObject multiScattering;

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

	Attribute::scatterColor = nAttr.createColor("scatterColor", "c");
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(1.0f, 1.0f, 1.0f));

	Attribute::transmissionColor = nAttr.createColor("transmissionColor", "ct");
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(1.0f, 1.0f, 1.0f));

	Attribute::emissionColor = nAttr.createColor("emissionColor", "ce");
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(.0f, .0f, .0f));

	Attribute::density = nAttr.create("density", "d", MFnNumericData::kFloat, 1.);
	MAKE_INPUT(nAttr);
	nAttr.setSoftMin(0.0);
	nAttr.setSoftMax(10.f);

	Attribute::scatteringDirection = nAttr.create("scatteringDirection", "sd", MFnNumericData::kFloat, 0.0);
	MAKE_INPUT(nAttr);
	nAttr.setSoftMin(-1.0f);
	nAttr.setSoftMax(1.0f);

	Attribute::multiScattering = nAttr.create("multiscatter", "ms", MFnNumericData::kBoolean, true);
	MAKE_INPUT(nAttr);

	// output color
	Attribute::output = nAttr.createColor("outColor", "oc");
	MAKE_OUTPUT(nAttr);

	CHECK_MSTATUS(addAttribute(Attribute::scatterColor));
	CHECK_MSTATUS(addAttribute(Attribute::transmissionColor));
	CHECK_MSTATUS(addAttribute(Attribute::density));
	CHECK_MSTATUS(addAttribute(Attribute::emissionColor));
	CHECK_MSTATUS(addAttribute(Attribute::scatteringDirection));
	CHECK_MSTATUS(addAttribute(Attribute::multiScattering));
	CHECK_MSTATUS(addAttribute(Attribute::output));

	CHECK_MSTATUS(attributeAffects(Attribute::scatterColor, Attribute::output));
	CHECK_MSTATUS(attributeAffects(Attribute::transmissionColor, Attribute::output));
	CHECK_MSTATUS(attributeAffects(Attribute::density, Attribute::output));
	CHECK_MSTATUS(attributeAffects(Attribute::emissionColor, Attribute::output));
	CHECK_MSTATUS(attributeAffects(Attribute::scatteringDirection, Attribute::output));
	CHECK_MSTATUS(attributeAffects(Attribute::multiScattering, Attribute::output));

	return MS::kSuccess;
}

MStatus FireMaya::VolumeMaterial::compute(const MPlug& plug, MDataBlock& block)
{
	if ((plug == Attribute::output) || (plug.parent() == Attribute::output))
	{
		// We need to get all attributes which affect outputs in order to recalculate all dependent nodes
		// It needs to get IPR properly updating while changing attributes on the "left" nodes in dependency graph
		ForceEvaluateAllAttributes(true);

		MFloatVector& surfaceColor = block.inputValue(Attribute::scatterColor).asFloatVector();

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

	auto scatterColor = scope.GetValue(shaderNode.findPlug(Attribute::scatterColor, false));
	auto transmissionColor = scope.GetValue(shaderNode.findPlug(Attribute::transmissionColor, false));
	auto emissionColor = scope.GetValue(shaderNode.findPlug(Attribute::emissionColor, false));
	auto k = shaderNode.findPlug(Attribute::density, false).asFloat();
	auto scatteringDirection = shaderNode.findPlug(Attribute::scatteringDirection, false).asFloat();
	auto multiScatter = shaderNode.findPlug(Attribute::multiScattering, false).asBool();

	// scattering
	material.SetValue(RPR_MATERIAL_INPUT_SCATTERING, scatterColor * k);

	// absorption
	material.SetValue(RPR_MATERIAL_INPUT_ABSORBTION, (1 - transmissionColor) * k);

	// emission
	material.SetValue(RPR_MATERIAL_INPUT_EMISSION, emissionColor * k);

	// phase and multi on/off
	material.SetValue(RPR_MATERIAL_INPUT_G, scatteringDirection);
	material.SetValue(RPR_MATERIAL_INPUT_MULTISCATTER, multiScatter ? 1.f : 0.f);

	return material;
}
