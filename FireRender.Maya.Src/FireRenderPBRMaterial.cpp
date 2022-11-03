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


#include <maya/MColor.h>
#include <maya/MDataHandle.h>
#include <maya/MFloatVector.h>
#include <maya/MDistance.h>

#include "FireRenderPBRMaterial.h"

namespace FireMaya
{
	static const float NormalizedMin = 0.0f;
	static const float NormalizedMax = 1.0f;
	static const float DefaultRoughness = 0.5f;

	static const MColor BaseColorDefault = MColor(0.5f, 0.5f, 0.5f);
	static const MColor CavityColorDefault = MColor(0.0f, 0.0f, 0.0f);
	static const MColor NormalMapColorDefault = MColor(1.0f, 1.0f, 1.0f);
	static const MColor EmissiveColorDefault = MColor(0.5f, 0.5f, 0.5f);
	static const MColor SSColourDefault = MColor(0.436f, 0.227f, 0.131f);

	namespace
	{
		namespace Attribute
		{
			MObject outColor;
			MObject outAlpha;

			MObject baseColor;
			MObject roughness;
			MObject metalness;
			MObject specular;
			MObject normalMap;
			MObject emissiveColor;
			MObject emissiveWeight;

			MObject glass;
			MObject glassIOR;
			MObject subsurfaceWeight;
			MObject subsurfaceColor;
			MObject subsurfaceRadius;
		}
	}

	// Register attribute and make it affecting output color and alpha
	void FireRenderPBRMaterial::AddAttribute(MObject& nAttr)
	{
		CHECK_MSTATUS(addAttribute(nAttr));
		CHECK_MSTATUS(attributeAffects(nAttr, Attribute::outColor));
		CHECK_MSTATUS(attributeAffects(nAttr, Attribute::outAlpha));
	}

	void* FireRenderPBRMaterial::creator()
	{
		return new FireRenderPBRMaterial();
	}

	void FireRenderPBRMaterial::AddColorAttribute(MObject& attrRef, const char* name, const char* shortName, bool haveConnection, const MColor& color)
	{
		MFnNumericAttribute nAttr;

		attrRef = nAttr.createColor(name, shortName);
		MAKE_INPUT(nAttr);

		nAttr.setConnectable(haveConnection);

		CHECK_MSTATUS(nAttr.setDefault(color.r, color.g, color.b));
		AddAttribute(attrRef);
	}

	void FireRenderPBRMaterial::AddBoolAttribute(MObject& attrRef, const char* name, const char* shortName, bool flag)
	{
		MFnNumericAttribute nAttr;

		attrRef = nAttr.create(name, shortName, MFnNumericData::kBoolean, flag ? 1 : 0);
		nAttr.setConnectable(false);
		MAKE_INPUT(nAttr);
		AddAttribute(attrRef);
	}

	void FireRenderPBRMaterial::AddFloatAttribute(MObject& attrRef, const char* name, const char* shortName, float min, float max, float def)
	{
		MFnNumericAttribute nAttr;

		attrRef = nAttr.create(name, shortName, MFnNumericData::kFloat, def);
		MAKE_INPUT(nAttr);
		nAttr.setMin(min);
		nAttr.setMax(max);
		AddAttribute(attrRef);
	}

	MStatus FireRenderPBRMaterial::initialize()
	{
		MFnNumericAttribute nAttr;

		Attribute::outColor = nAttr.createColor("outColor", "oc");
		MAKE_OUTPUT(nAttr);

		Attribute::outAlpha = nAttr.createColor("outAlpha", "oa");
		MAKE_OUTPUT(nAttr);

		CHECK_MSTATUS(addAttribute(Attribute::outColor));
		CHECK_MSTATUS(addAttribute(Attribute::outAlpha));

		// Add input attributes

		// I use "color" attribute name instead of "baseColor" to get MaterialViewer preview automatically works
		// "color" is standard name and automatic binding will be used. In other case we should create our own ShadingNodeOverride class
		// as it done for i.e. Uber Material
		AddColorAttribute(Attribute::baseColor, "color", "bc", true, BaseColorDefault);
		AddFloatAttribute(Attribute::roughness, "roughness", "r", NormalizedMin, NormalizedMax, DefaultRoughness);
		AddFloatAttribute(Attribute::metalness, "metalness", "m", NormalizedMin, NormalizedMax, NormalizedMin);
		AddFloatAttribute(Attribute::specular, "specular", "s", 0.0f, 1.0f, 1.0f);
		AddColorAttribute(Attribute::normalMap, "normalMap", "nm", true,NormalMapColorDefault);
		AddColorAttribute(Attribute::emissiveColor, "emissiveColor", "ec", true, EmissiveColorDefault);
		AddFloatAttribute(Attribute::emissiveWeight, "emissiveWeight", "ew", NormalizedMin, NormalizedMax, NormalizedMin);

		AddFloatAttribute(Attribute::glass, "glass", "g", 0.0f, 1.0f, 0.0f);
		AddFloatAttribute(Attribute::glassIOR, "glassIOR", "gi", 0.0f, 2.0f, 1.5f);
		AddFloatAttribute(Attribute::subsurfaceWeight, "subsurfaceWeight", "ssw", 0.0f, 1.0f, 0.0f);
		AddColorAttribute(Attribute::subsurfaceColor, "subsurfaceColor", "ssc", true, SSColourDefault);

		Attribute::subsurfaceRadius = nAttr.create("subsurfaceRadius", "ssr", MFnNumericData::k3Float);
		MAKE_INPUT(nAttr);
		CHECK_MSTATUS(nAttr.setDefault(3.67f, 1.37f, 0.68f));
		nAttr.setMin(0.0f, 0.0f, 0.0f);
		nAttr.setMax(5.0f, 5.0f, 5.0f);
		CHECK_MSTATUS(addAttribute(Attribute::subsurfaceRadius));

		return MS::kSuccess;
	}

	MStatus FireRenderPBRMaterial::compute(const MPlug& plug, MDataBlock& block)
	{
		if ((plug == Attribute::outColor) || (plug.parent() == Attribute::outColor))
		{
			// We need to get all attributes which affect outputs in order to recalculate all dependent nodes
			// It needs to get IPR properly updating while changing attributes on the "left" nodes in dependency graph
			ForceEvaluateAllAttributes(true);

			MFloatVector& surfaceColor = block.inputValue(Attribute::baseColor).asFloatVector();

			// set output color attribute
			MDataHandle outColorHandle = block.outputValue(Attribute::outColor);
			MFloatVector& outColor = outColorHandle.asFloatVector();
			outColor = surfaceColor;
			outColorHandle.setClean();
			block.setClean(plug);
		}
		else if ((plug == Attribute::outAlpha) || (plug.parent() == Attribute::outAlpha))
		{
			MFloatVector tr(1.0, 1.0, 1.0);

			// set output color attribute
			MDataHandle outTransHandle = block.outputValue(Attribute::outAlpha);
			MFloatVector& outTrans = outTransHandle.asFloatVector();
			outTrans = tr;
			block.setClean(plug);
		}
		else
			return MS::kUnknownParameter;

		return MS::kSuccess;
	}

	frw::Shader FireRenderPBRMaterial::GetVolumeShader(Scope& scope)
	{
		return frw::Shader();
	}

	frw::Value FireRenderPBRMaterial::GetAttributeValue(const MObject& attribute, const MFnDependencyNode& shaderNode, Scope& scope)
	{
		return scope.GetValue(shaderNode.findPlug(attribute, false));
	}

	frw::Shader FireRenderPBRMaterial::GetShader(Scope& scope)
	{
		MFnDependencyNode shaderNode(thisMObject());

		frw::Shader shader(scope.MaterialSystem(), scope.Context());

		MDistance::Unit sceneUnits = MDistance::uiUnit();
		MDistance distance(1.0, sceneUnits);
		float scale_multiplier = (float) distance.asMeters();

		auto ms = scope.MaterialSystem();

		// Diffuse (base color)
		// apply purple 1x1 image in case we missed a texture file
		frw::Value value = scope.GetValueForDiffuseColor(shaderNode.findPlug(Attribute::baseColor, false));
		frw::Value diffuseColor = value;
		shader.xSetValue(RPR_MATERIAL_INPUT_UBER_DIFFUSE_WEIGHT, 1.0f);
		shader.xSetValue(RPR_MATERIAL_INPUT_UBER_DIFFUSE_COLOR, diffuseColor);

		// Metalness
		value = scope.GetValue(shaderNode.findPlug(Attribute::metalness, false));
		shader.xSetParameterU(RPR_MATERIAL_INPUT_UBER_REFLECTION_MODE, RPR_UBER_MATERIAL_IOR_MODE_METALNESS);
		shader.xSetValue(RPR_MATERIAL_INPUT_UBER_REFLECTION_METALNESS, value);

		// Specular (reflection weight)
		value = scope.GetValue(shaderNode.findPlug(Attribute::specular, false));
		shader.xSetValue(RPR_MATERIAL_INPUT_UBER_REFLECTION_WEIGHT, value);
		shader.xSetValue(RPR_MATERIAL_INPUT_UBER_REFLECTION_COLOR, diffuseColor);

		// Roughness
		value = scope.GetValue(shaderNode.findPlug(Attribute::roughness, false));
		shader.xSetValue(RPR_MATERIAL_INPUT_UBER_DIFFUSE_ROUGHNESS, value);
		shader.xSetValue(RPR_MATERIAL_INPUT_UBER_REFLECTION_ROUGHNESS, value);
		shader.xSetValue(RPR_MATERIAL_INPUT_UBER_REFRACTION_ROUGHNESS, value);

		// Normal
		value = scope.GetValue(shaderNode.findPlug(Attribute::normalMap, false));
		int type = value.GetNodeType();
		if (type == frw::ValueTypeNormalMap || type == frw::ValueTypeBumpMap || type == frw::ValueTypeBevel)
		{
			shader.xSetValue(RPR_MATERIAL_INPUT_UBER_DIFFUSE_NORMAL, value);
			shader.xSetValue(RPR_MATERIAL_INPUT_UBER_REFLECTION_NORMAL, value);
		}
		else if (type >= 0)
		{
			ErrorPrint("%s NormalMap: invalid node type %d\n", shaderNode.name().asChar(), value.GetNodeType());
		}

		// Glass (refraction weight)
		value = scope.GetValue(shaderNode.findPlug(Attribute::glass, false));
		shader.xSetValue(RPR_MATERIAL_INPUT_UBER_REFRACTION_WEIGHT, value);
		shader.xSetValue(RPR_MATERIAL_INPUT_UBER_REFRACTION_COLOR, diffuseColor);

		// Glass IOR (refraction IOR)
		value = scope.GetValue(shaderNode.findPlug(Attribute::glassIOR, false));
		shader.xSetValue(RPR_MATERIAL_INPUT_UBER_REFRACTION_IOR, value);
		
		// Emissive
		// Set EMISSION_COLOR as intensity. Use multiplier for that
		frw::Value emissiveWeightValue = scope.GetValue(shaderNode.findPlug(Attribute::emissiveWeight, false));
		frw::Value clampedWeight = ms.ValueClamp(emissiveWeightValue);
		shader.xSetValue(RPR_MATERIAL_INPUT_UBER_EMISSION_WEIGHT, clampedWeight);

		value = scope.GetValue(shaderNode.findPlug(Attribute::emissiveColor, false));
		value = ms.ValueMul(emissiveWeightValue, value);		
		shader.xSetValue(RPR_MATERIAL_INPUT_UBER_EMISSION_COLOR, value);

		// SSS
		value = scope.GetValue(shaderNode.findPlug(Attribute::subsurfaceWeight, false));
		shader.xSetValue(RPR_MATERIAL_INPUT_UBER_SSS_WEIGHT, value);
		shader.xSetValue(RPR_MATERIAL_INPUT_UBER_BACKSCATTER_WEIGHT, value);

		shader.xSetValue(RPR_MATERIAL_INPUT_UBER_BACKSCATTER_COLOR, frw::Value(1.0, 1.0, 1.0, 1.0));

		value = scope.GetValue(shaderNode.findPlug(Attribute::subsurfaceColor, false));
		shader.xSetValue(RPR_MATERIAL_INPUT_UBER_SSS_SCATTER_COLOR, value);

		value = scope.GetValue(shaderNode.findPlug(Attribute::subsurfaceRadius, false)) * scale_multiplier;
		shader.xSetValue(RPR_MATERIAL_INPUT_UBER_SSS_SCATTER_DISTANCE, value);

		shader.xSetValue(RPR_MATERIAL_INPUT_UBER_SSS_SCATTER_DIRECTION, 0.0);
		shader.xSetParameterU(RPR_MATERIAL_INPUT_UBER_SSS_MULTISCATTER, RPR_TRUE);

		return shader;
	}
}
