/*********************************************************************************************************************************
* Radeon ProRender for plugins
* Copyright (c) 2018 AMD
* All Rights Reserved
*
* FireRenderPBRMaterial class implementation.
* This class is a lightweight version of Uber Material.
*********************************************************************************************************************************/


#include <maya/MColor.h>
#include <maya/MDataHandle.h>
#include <maya/MFloatVector.h>

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
		AddColorAttribute(Attribute::subsurfaceWeight, "subsurfaceWeight", "ssw", true, SSColourDefault);
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
		return scope.GetValue(shaderNode.findPlug(attribute));
	}

	frw::Shader FireRenderPBRMaterial::GetShader(Scope& scope)
	{
		MFnDependencyNode shaderNode(thisMObject());

		frw::Shader shader(scope.MaterialSystem(), scope.Context(), RPRX_MATERIAL_UBER);

		auto ms = scope.MaterialSystem();

		// Setting RPRX_UBER_MATERIAL_DIFFUSE_COLOR
		frw::Value value = scope.GetValue(shaderNode.findPlug(Attribute::baseColor));
		frw::Value diffuseColor = value;
		shader.xSetValue(RPRX_UBER_MATERIAL_DIFFUSE_COLOR, value);

		// Setting RPRX_UBER_MATERIAL_DIFFUSE_WEIGHT
		value = scope.GetValue(shaderNode.findPlug(Attribute::specular));

		shader.xSetValue(RPRX_UBER_MATERIAL_DIFFUSE_WEIGHT, value);

		// Setting RPRX_UBER_MATERIAL_REFLECTION_ROUGHNESS
		value = scope.GetValue(shaderNode.findPlug(Attribute::roughness));
		shader.xSetValue(RPRX_UBER_MATERIAL_REFLECTION_ROUGHNESS, value);
		
		// Setting Metalness
		value = scope.GetValue(shaderNode.findPlug(Attribute::metalness));

		shader.xSetValue(RPRX_UBER_MATERIAL_REFLECTION_COLOR, diffuseColor);
		shader.xSetValue(RPRX_UBER_MATERIAL_REFLECTION_WEIGHT, 1.0f);
		shader.xSetParameterU(RPRX_UBER_MATERIAL_REFLECTION_MODE, RPRX_UBER_MATERIAL_REFLECTION_MODE_METALNESS);
		shader.xSetValue(RPRX_UBER_MATERIAL_REFLECTION_METALNESS, value);

		// Setting Material Normal
		value = scope.GetValue(shaderNode.findPlug(Attribute::normalMap));

		int type = value.GetNodeType();
		if (type == frw::ValueTypeNormalMap || type == frw::ValueTypeBumpMap)
		{
#if (RPR_API_VERSION < 0x010031000)
			shader.xSetValue(RPRX_UBER_MATERIAL_NORMAL, value);
#endif
		}
		else if (type >= 0)
		{
			ErrorPrint("%s NormalMap: invalid node type %d\n", shaderNode.name().asChar(), value.GetNodeType());
		}
		
		// Setting Emissive
		// Set EMISSION_COLOR as intensity. Use multiplier for that

		frw::Value emissiveWeightValue = scope.GetValue(shaderNode.findPlug(Attribute::emissiveWeight));
		frw::Value clampedWeight = ms.ValueClamp(emissiveWeightValue);

		shader.xSetValue(RPRX_UBER_MATERIAL_EMISSION_WEIGHT, clampedWeight);

		value = scope.GetValue(shaderNode.findPlug(Attribute::emissiveColor));
		value = ms.ValueMul(emissiveWeightValue, value);
		
		shader.xSetValue(RPRX_UBER_MATERIAL_EMISSION_COLOR, value);

		// Glass (refraction weight)
		shader.xSetValue(RPRX_UBER_MATERIAL_REFRACTION_COLOR, diffuseColor);

		value = scope.GetValue(shaderNode.findPlug(Attribute::glass));
		shader.xSetValue(RPRX_UBER_MATERIAL_REFRACTION_WEIGHT, value);

		shader.xSetValue(RPRX_UBER_MATERIAL_REFRACTION_ROUGHNESS, 0.0f);

		value = scope.GetValue(shaderNode.findPlug(Attribute::glassIOR));
		shader.xSetValue(RPRX_UBER_MATERIAL_REFRACTION_IOR, value);

		// SSS
		value = scope.GetValue(shaderNode.findPlug(Attribute::subsurfaceWeight));
		shader.xSetValue(RPRX_UBER_MATERIAL_SSS_WEIGHT, value);

		value = scope.GetValue(shaderNode.findPlug(Attribute::subsurfaceColor));
		shader.xSetValue(RPRX_UBER_MATERIAL_SSS_SCATTER_COLOR, value);

		value = scope.GetValue(shaderNode.findPlug(Attribute::subsurfaceRadius));
		shader.xSetValue(RPRX_UBER_MATERIAL_SSS_SCATTER_DISTANCE, value);

		return shader;
	}
}