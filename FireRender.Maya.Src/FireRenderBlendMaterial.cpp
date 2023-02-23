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
#include "FireRenderBlendMaterial.h"

#include <cassert>

#include <maya/MGlobal.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnNumericData.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFloatVector.h>
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MFnFloatArrayData.h>
#include <maya/MFnArrayAttrsData.h>

#include "FireMaya.h"
#include "FireRenderUtils.h"

namespace
{
	namespace Attribute
	{
		MObject	inputA;
		MObject	inputB;
		MObject	weight;
		MObject output;
		MObject disableSwatch;
		MObject swatchIterations;
	}
}

void* FireMaya::BlendMaterial::creator()
{
	return new BlendMaterial();
}

MStatus FireMaya::BlendMaterial::initialize()
{
	MFnNumericAttribute nAttr;
	MFnTypedAttribute tAttr;

	Attribute::inputA = nAttr.createColor("color0", "c0");
	MAKE_INPUT(nAttr);
	nAttr.setDefault(0.0, 0.0, 0.0);

	Attribute::inputB = nAttr.createColor("color1", "c1");
	MAKE_INPUT(nAttr);
	nAttr.setDefault(0.0, 0.0, 0.0);

	Attribute::weight = nAttr.create("weight", "w", MFnNumericData::kFloat, 0.0);
	MAKE_INPUT(nAttr);
	nAttr.setSoftMin(0.0);
	nAttr.setSoftMax(1.0);

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
	CHECK_MSTATUS(addAttribute(Attribute::weight));
	CHECK_MSTATUS(addAttribute(Attribute::output));

	CHECK_MSTATUS(attributeAffects(Attribute::inputA, Attribute::output));
	CHECK_MSTATUS(attributeAffects(Attribute::inputB, Attribute::output));
	CHECK_MSTATUS(attributeAffects(Attribute::weight, Attribute::output));

	CHECK_MSTATUS(addAttribute(Attribute::disableSwatch));
	CHECK_MSTATUS(addAttribute(Attribute::swatchIterations));

	return MS::kSuccess;
}

MStatus FireMaya::BlendMaterial::compute(const MPlug& plug, MDataBlock& block)
{
	if ((plug == Attribute::output) || (plug.parent() == Attribute::output))
	{
		MFloatVector& topColor = block.inputValue(Attribute::inputA).asFloatVector();
		MFloatVector& baseColor = block.inputValue(Attribute::inputB).asFloatVector();
		float weight = block.inputValue(Attribute::weight).asFloat();
		
		// set ouput color attribute
		MDataHandle outColorHandle = block.outputValue(Attribute::output);
		MFloatVector& outColor = outColorHandle.asFloatVector();
		outColor = topColor * weight + baseColor * (1.0f - weight);
		outColorHandle.setClean();
		block.setClean(plug);
	}
	else
		return MS::kUnknownParameter;

	return MS::kSuccess;
}


frw::Shader GetShaderForBlend(FireMaya::Scope& scope, const MFnDependencyNode &node, const MObject & attr)
{
	auto shader = scope.GetShader(node.findPlug(attr));

	if (shader.GetShaderType() != frw::ShaderTypeInvalid)
		return shader;

	auto color = scope.GetValue(node.findPlug(attr, false));
	frw::DiffuseShader defaultShader(scope.MaterialSystem());
	defaultShader.SetColor(color);
	return defaultShader;
}


frw::Shader FireMaya::BlendMaterial::GetShader(Scope& scope)
{
	MFnDependencyNode shaderNode(thisMObject());

	auto t = scope.GetValue(shaderNode.findPlug(Attribute::weight, false));
	auto a = GetShaderForBlend(scope, shaderNode, Attribute::inputA);
	auto b = GetShaderForBlend(scope, shaderNode, Attribute::inputB);
	return scope.MaterialSystem().ShaderBlend(a, b, t);
}

frw::Shader FireMaya::BlendMaterial::GetVolumeShader(Scope& scope)
{
	MFnDependencyNode shaderNode(thisMObject());

	auto t = scope.GetValue(shaderNode.findPlug(Attribute::weight, false));

	float blendValue = 0;

	if (t.IsFloat())
		blendValue = t.GetX();

	auto shaderA = scope.GetVolumeShader(shaderNode.findPlug(Attribute::inputA));
	auto shaderB = scope.GetVolumeShader(shaderNode.findPlug(Attribute::inputB));

	auto shaderTypeA = shaderA.GetShaderType();
	auto shaderTypeB = shaderB.GetShaderType();

	if (shaderTypeA != frw::ShaderTypeInvalid && shaderTypeB != frw::ShaderTypeInvalid)
		return blendValue < 0.5f ? shaderA : shaderB;
	else if (shaderTypeA != frw::ShaderTypeInvalid)
		return shaderA;
	else if (shaderTypeB != frw::ShaderTypeInvalid)
		return shaderB;
	else
		return frw::Shader();
}

//////////////////////////////////////////////////////////////////////////////
// VIEWPORT 2.0

#include <maya/MFragmentManager.h>
#include <maya/MShaderManager.h>

FireMaya::BlendMaterial::Override::Override(const MObject& obj) : MPxSurfaceShadingNodeOverride(obj)
{
	m_shader = obj;
}

FireMaya::BlendMaterial::Override::~Override()
{

}

MHWRender::MPxSurfaceShadingNodeOverride* FireMaya::BlendMaterial::Override::creator(const MObject& obj)
{
	return new Override(obj);
}

MHWRender::DrawAPI FireMaya::BlendMaterial::Override::supportedDrawAPIs() const
{
	return MHWRender::DrawAPI::kAllDevices;
}

bool FireMaya::BlendMaterial::Override::allowConnections() const
{
	return true;
}

MString FireMaya::BlendMaterial::Override::fragmentName() const
{
	return "mayaLambertSurface";
}

void FireMaya::BlendMaterial::Override::getCustomMappings(MHWRender::MAttributeParameterMappingList& mappings)
{
	// Mapping removed because of strange behavior when mix different shader types with multiple blend nodes
}

bool FireMaya::BlendMaterial::Override::valueChangeRequiresFragmentRebuild(const MPlug* plug) const
{
	return false;
}

void FireMaya::BlendMaterial::Override::updateDG()
{
}

const float3 defaultBlendShaderTexturePlaceholderColor = { 0.2f, 0.2f, 0.2f };

void FireMaya::BlendMaterial::Override::updateShader(MHWRender::MShaderInstance& shader, const MHWRender::MAttributeParameterMappingList& mappings)
{
	if (m_shader.isNull())
	{
		return;
	}

	MFnDependencyNode shaderNode(m_shader);
	MStatus success;

	MPlug colorPlug0 = shaderNode.findPlug("color0", false);
	assert(!colorPlug0.isNull());
	MFloatVector color0;
	if (colorPlug0.isConnected())
	{
		color0 = defaultBlendShaderTexturePlaceholderColor;
	}
	else
	{
		MDataHandle colorDataHandle0;
		success = colorPlug0.getValue(colorDataHandle0);
		assert(success == MStatus::kSuccess);
		color0 = colorDataHandle0.asFloat3();
	}

	MPlug colorPlug1 = shaderNode.findPlug("color1", false);
	assert(!colorPlug1.isNull());
	MFloatVector color1;
	if (colorPlug1.isConnected())
	{
		color1 = defaultBlendShaderTexturePlaceholderColor;
	}
	else
	{
		MDataHandle colorDataHandle1;
		success = colorPlug1.getValue(colorDataHandle1);
		assert(success == MStatus::kSuccess);
		color1 = colorDataHandle1.asFloat3();
	}

	MPlug weightPlug = shaderNode.findPlug("weight", false);
	float weight = 0.0f;
	MStatus weightStatus = weightPlug.getValue(weight);
	assert(weightStatus == MStatus::kSuccess);

	MFloatVector outColor;
	outColor = color0 * weight + color1 * (1.0f - weight);
	success = shader.setParameter("color", outColor);
	assert(success == MStatus::kSuccess);
}

const char* FireMaya::BlendMaterial::Override::className()
{
	return "RPRBlendNodeOverride";
}
