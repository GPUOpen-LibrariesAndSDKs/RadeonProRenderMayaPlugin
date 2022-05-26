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
#include "FireRenderMaterial.h"

#include <maya/MFnNumericAttribute.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MDataHandle.h>
#include <maya/MFloatVector.h>

 // Attributes
namespace
{
	namespace Attribute
	{
		MObject type;
		MObject color;
		MObject transparency;
		MObject output;
		MObject outputAlpha;
		MObject normalMap;
		MObject refractiveIndex;
		MObject roughness;
		MObject wattsPerSQM;

		MObject disableSwatch;
		MObject swatchIterations;
	}
}

void FireMaya::Material::postConstructor()
{
	setMPSafe(true);
}

// creates an instance of the node
void* FireMaya::Material::creator()
{
	return new Material;
}

// initializes attribute information
// call by MAYA when this plug-in was loaded.
//
MStatus FireMaya::Material::initialize()
{
	MFnNumericAttribute nAttr;
	MFnEnumAttribute eAttr;

	Attribute::type = eAttr.create("type", "t", 0);
	eAttr.addField("Diffuse", kDiffuse);
	eAttr.addField("Microfacet", kMicrofacet);
	eAttr.addField("Microfacet Refraction", kMicrofacetRefract);
	eAttr.addField("Reflect", kReflect);
	eAttr.addField("Refract", kRefract);
	eAttr.addField("Transparent", kTransparent);
	eAttr.addField("Emissive", kEmissive);
	eAttr.addField("Oren-Nayar", kOrenNayar);
	eAttr.addField("Diffuse Refraction", kDiffuseRefraction);
	eAttr.addField("Pass Through", kPassThrough);
	MAKE_INPUT_CONST(eAttr);

	Attribute::color = nAttr.createColor("color", "c");
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(0.644f, 0.644f, 0.644f));

	// input transparency
	Attribute::transparency = nAttr.createColor("transparency", "it");
	MAKE_INPUT(nAttr);

	Attribute::normalMap = nAttr.createPoint("normalMap", "nm");
	MAKE_INPUT(nAttr);

	Attribute::refractiveIndex = nAttr.create("refractiveIndex", "ri", MFnNumericData::kFloat, 4.5);
	MAKE_INPUT(nAttr);
	nAttr.setDefault(4.5);
	nAttr.setSoftMin(1.0);
	nAttr.setSoftMax(5.0);

	Attribute::roughness = nAttr.create("roughness", "rg", MFnNumericData::kFloat, 0.5);
	MAKE_INPUT(nAttr);
	nAttr.setDefault(0.5);
	nAttr.setSoftMin(0.0);
	nAttr.setSoftMax(0.9);

	Attribute::wattsPerSQM = nAttr.create("wattsPerSqm", "wqm", MFnNumericData::kFloat, 1.0);
	MAKE_INPUT(nAttr);
	nAttr.setDefault(1.0);
	nAttr.setMin(0.0);
	nAttr.setMax(10000000.0);
	nAttr.setSoftMax(100.0);

	// output color
	Attribute::output = nAttr.createColor("outColor", "oc");
	MAKE_OUTPUT(nAttr);

	// output transparency
	Attribute::outputAlpha = nAttr.createColor("outTransparency", "ot");
	MAKE_OUTPUT(nAttr);

	Attribute::disableSwatch = nAttr.create("disableSwatch", "ds", MFnNumericData::kBoolean, 0);
	MAKE_INPUT(nAttr);

	Attribute::swatchIterations = nAttr.create("swatchIterations", "swi", MFnNumericData::kInt, 1);
	MAKE_INPUT(nAttr);
	nAttr.setMin(1);
	nAttr.setMax(64);

	CHECK_MSTATUS(addAttribute(Attribute::type));
	CHECK_MSTATUS(addAttribute(Attribute::color));
	CHECK_MSTATUS(addAttribute(Attribute::transparency));
	CHECK_MSTATUS(addAttribute(Attribute::normalMap));
	CHECK_MSTATUS(addAttribute(Attribute::refractiveIndex));
	CHECK_MSTATUS(addAttribute(Attribute::roughness));
	CHECK_MSTATUS(addAttribute(Attribute::wattsPerSQM));

	CHECK_MSTATUS(addAttribute(Attribute::output));
	CHECK_MSTATUS(addAttribute(Attribute::outputAlpha));

	CHECK_MSTATUS(addAttribute(Attribute::disableSwatch));
	CHECK_MSTATUS(addAttribute(Attribute::swatchIterations));

	CHECK_MSTATUS(attributeAffects(Attribute::type, Attribute::output));
	CHECK_MSTATUS(attributeAffects(Attribute::color, Attribute::output));
	CHECK_MSTATUS(attributeAffects(Attribute::transparency, Attribute::outputAlpha));
	CHECK_MSTATUS(attributeAffects(Attribute::normalMap, Attribute::output));
	CHECK_MSTATUS(attributeAffects(Attribute::refractiveIndex, Attribute::output));
	CHECK_MSTATUS(attributeAffects(Attribute::roughness, Attribute::output));
	CHECK_MSTATUS(attributeAffects(Attribute::wattsPerSQM, Attribute::output));

	return MS::kSuccess;
}

MStatus FireMaya::Material::compute(const MPlug& plug, MDataBlock& block)
{
	if ((plug == Attribute::output) || (plug.parent() == Attribute::output))
	{
		// We need to get all attributes which affect outputs in order to recalculate all dependent nodes
		// It needs to get IPR properly updating while changing attributes on the "left" nodes in dependency graph
		ForceEvaluateAllAttributes(true);

		MFloatVector& surfaceColor = block.inputValue(Attribute::color).asFloatVector();

		// set output color attribute
		MDataHandle outColorHandle = block.outputValue(Attribute::output);
		MFloatVector& outColor = outColorHandle.asFloatVector();
		outColor = surfaceColor;
		outColorHandle.setClean();
		block.setClean(plug);
	}
	else if ((plug == Attribute::outputAlpha) || (plug.parent() == Attribute::outputAlpha))
	{
		MFloatVector& tr = block.inputValue(Attribute::transparency).asFloatVector();

		// set output color attribute
		MDataHandle outTransHandle = block.outputValue(Attribute::outputAlpha);
		MFloatVector& outTrans = outTransHandle.asFloatVector();
		outTrans = tr;
		block.setClean(plug);
	}
	else
		return MS::kUnknownParameter;

	return MS::kSuccess;
}

FireMaya::Material::Type FireMaya::Material::GetMayaShaderType() const
{
	MFnDependencyNode shaderNode(thisMObject());

	Type mayaType = kDiffuse;

	MPlug plug = shaderNode.findPlug("type", false);
	if (!plug.isNull())
	{
		int n = 0;
		if (MStatus::kSuccess == plug.getValue(n))
			mayaType = static_cast<Type>(n);
	}

	return mayaType;
}


frw::Shader FireMaya::Material::GetShader(Scope& scope)
{
	MFnDependencyNode shaderNode(thisMObject());

	Type mayaType = GetMayaShaderType();

	float intensity = 1.0f;
	if (mayaType == kEmissive)
	{
		MPlug plug = shaderNode.findPlug("wattsPerSqm", false);
		if (!plug.isNull())
			plug.getValue(intensity);
	}

	// translate maya node type to internal fr shader type
	static struct { Type mayaType; frw::ShaderType shaderType; } typeMap[] =
	{
		{ kDiffuse, frw::ShaderTypeDiffuse },
		{ kMicrofacet, frw::ShaderTypeMicrofacet },
		{ kMicrofacetRefract, frw::ShaderTypeMicrofacetRefraction },
		{ kReflect, frw::ShaderTypeReflection },
		{ kRefract, frw::ShaderTypeRefraction },
		{ kTransparent, frw::ShaderTypeTransparent },
		{ kEmissive, frw::ShaderTypeEmissive },
		{ kOrenNayar, frw::ShaderTypeOrenNayer },
		{ kDiffuseRefraction, frw::ShaderTypeDiffuseRefraction },
		{ kPassThrough, frw::ShaderTypeFlatColor }
	};

	auto shaderType = frw::ShaderTypeDiffuse;

	for (auto it : typeMap)
	{
		if (it.mayaType == mayaType)
		{
			shaderType = it.shaderType;
			break;
		}
	}

	frw::Shader shader(scope.MaterialSystem(), shaderType);

	static struct { const MObject& attribute; rpr_material_node_input inputKey; } attributes[] =
	{
		{ Attribute::color, RPR_MATERIAL_INPUT_COLOR },
		{ Attribute::normalMap, RPR_MATERIAL_INPUT_NORMAL },
		{ Attribute::roughness, RPR_MATERIAL_INPUT_ROUGHNESS },
		{ Attribute::refractiveIndex, RPR_MATERIAL_INPUT_IOR }
	};

	if (shaderType == frw::ShaderTypeEmissive)
	{
		frw::Value colorValue = scope.GetValue(shaderNode.findPlug(Attribute::color, false));
		colorValue = colorValue * intensity;

		shader.SetValue(RPR_MATERIAL_INPUT_COLOR, colorValue);
	}
	else
	{
		for (auto it : attributes)
		{
			if (auto value = (it.attribute == Attribute::normalMap)
				? scope.GetConnectedValue(shaderNode.findPlug(it.attribute, false))
				: scope.GetValue(shaderNode.findPlug(it.attribute, false))
				)
			{
				shader.SetValue(it.inputKey, value);
			}
		}
	}


	auto roughnessPlug = shaderNode.findPlug(Attribute::roughness, false);
	auto roughnessVal = scope.GetValue(roughnessPlug).GetX();
	MFnNumericAttribute roughnessAttr(roughnessPlug.attribute());
	auto softMax = float(int(roughnessVal * 2 + 0.5));
	if (softMax < 0.9f)
		softMax = 0.9f;
	roughnessAttr.setSoftMax(softMax);

	return shader;
}