#include "FireRenderShadowCatcherMaterial.h"

#include <cassert>

#include <maya/MGlobal.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnNumericData.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFloatVector.h>
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MPlugArray.h>

#include "FireRenderDisplacement.h"

#include "FireMaya.h"

namespace
{
	namespace Attribute
	{
		MObject bgIsEnv;
		MObject bgWeight;
		MObject	bgColor;
		MObject bgTransparency;
		MObject shadowColor;
		MObject shadowWeight;
		MObject shadowTransparency;
		MObject useNormalMap;
		MObject normalMap;
		MObject useDispMap;
		MObject dispMap;
		MObject output;
		MObject disableSwatch;
		MObject swatchIterations;
	}

	template<class T> struct AttributeEntry
	{
		MObject *object;
		std::string name;
		std::string shortName;
		T min;
		T max;
		T def;
		MFnNumericData::Type type;
	};
	struct ColorAttributeEntry
	{
		MObject *object;
		std::string name;
		std::string shortName;
		std::vector<float> def;
	};
}

void* FireMaya::ShadowCatcherMaterial::creator()
{
	return new ShadowCatcherMaterial();
}

MStatus FireMaya::ShadowCatcherMaterial::initialize()
{
	MFnNumericAttribute nAttr;
	
	std::vector<AttributeEntry<float>> floatAttributes =
	{
		{&Attribute::bgWeight, "bgWeight", "bgw", 0.0, 1.0, 1.0, MFnNumericData::kFloat},
		{&Attribute::bgTransparency, "bgTransparency", "bgt", 0.0, 1.0, 0.0, MFnNumericData::kFloat},
		{&Attribute::shadowTransparency, "shadowTransp", "st", 0.0, 1.0, 0.0, MFnNumericData::kFloat},
		{&Attribute::shadowWeight, "shadowWeight", "sw", 0.0, 1.0, 1.0, MFnNumericData::kFloat}
	};
	std::vector<AttributeEntry<bool>> boolAttributes = 
	{
		{ &Attribute::bgIsEnv, "bgIsEnv", "bgie", 0, 0, 1, MFnNumericData::kBoolean },
		{ &Attribute::useNormalMap, "useNormalMap", "unm", 0, 0, 0, MFnNumericData::kBoolean },
		{ &Attribute::useDispMap, "useDispMap", "udm", 0, 0, 0, MFnNumericData::kBoolean },
		{ &Attribute::disableSwatch, "disableSwatch", "ds", 0, 0, 0, MFnNumericData::kBoolean }
	};

	std::vector<ColorAttributeEntry> colorAttributes = 
	{
		{&Attribute::bgColor, "bgColor", "bgc", {0.0f, 0.0f, 0.0f} },
		{&Attribute::shadowColor, "shadowColor", "sc", { 0.0f, 0.0f, 0.0f } },
		{&Attribute::normalMap, "normalMap", "nm", { 0.0f, 0.0f, 0.0f } },
		{&Attribute::dispMap, "dispMap", "dm", { 0.0f, 0.0f, 0.0f } }
	};

	Attribute::output = nAttr.createColor("outColor", "oc");
	MAKE_OUTPUT(nAttr);

	Attribute::swatchIterations = nAttr.create("swatchIterations", "swi", MFnNumericData::kInt, 4);
	MAKE_INPUT(nAttr);
	nAttr.setMin(1);
	nAttr.setMax(64);

	CHECK_MSTATUS(addAttribute(Attribute::output));
	CHECK_MSTATUS(addAttribute(Attribute::swatchIterations));
	
	for (auto &att : floatAttributes)
	{
		*(att.object) = nAttr.create(att.name.c_str(), att.shortName.c_str(), att.type, att.def);
		MAKE_INPUT(nAttr);
		nAttr.setDefault(att.def);
		nAttr.setMin(att.min);
		nAttr.setMax(att.max);
		CHECK_MSTATUS(addAttribute(*(att.object)));
		CHECK_MSTATUS(attributeAffects(*(att.object), Attribute::output));
	}

	for (auto &att : boolAttributes)
	{
		*(att.object) = nAttr.create(att.name.c_str(), att.shortName.c_str(), att.type, att.def);
		MAKE_INPUT(nAttr);
		CHECK_MSTATUS(addAttribute(*(att.object)));
		CHECK_MSTATUS(attributeAffects(*(att.object), Attribute::output));
	}

	for (auto &att : colorAttributes)
	{
		*(att.object) = nAttr.createColor(att.name.c_str(), att.shortName.c_str());
		MAKE_INPUT(nAttr);
		nAttr.setDefault(att.def[0], att.def[1], att.def[2]);
		CHECK_MSTATUS(addAttribute(*(att.object)));
		CHECK_MSTATUS(attributeAffects(*(att.object), Attribute::output));
	}

	return MS::kSuccess;
}

MStatus FireMaya::ShadowCatcherMaterial::compute(const MPlug& plug, MDataBlock& block)
{
	if ((plug == Attribute::output) || (plug.parent() == Attribute::output))
	{
		return MS::kSuccess;
	}
	else
	{
		return MS::kUnknownParameter;
	}
}

frw::Shader FireMaya::ShadowCatcherMaterial::GetShader(Scope& scope)
{
	
	MFnDependencyNode shaderNode(thisMObject());

	frw::Shader shader(scope.MaterialSystem(), scope.Context(), RPRX_MATERIAL_UBER);
	
	// Code below this line copyed from FireRenderStandardMaterial
	// Normal map
	if (shaderNode.findPlug(Attribute::useNormalMap).asBool())
	{
		frw::Value value = scope.GetValue(shaderNode.findPlug(Attribute::normalMap));
		int type = value.GetNodeType();
		if (type == frw::ValueTypeNormalMap || type == frw::ValueTypeBumpMap)
		{
			shader.xSetValue(RPRX_UBER_MATERIAL_NORMAL, value);
		}
		else if (type >= 0)
		{
			ErrorPrint("%s NormalMap: invalid node type %d\n", shaderNode.name().asChar(), value.GetNodeType());
		}
	}

	// Special code for displacement map. We're using GetDisplacementNode() function which is called twice:
	// from this function, and from FireRenderMesh::setupDisplacement(). This is done because RPRX UberMaterial
	// doesn't have capabilities to set any displacement parameters except map image, so we're setting other
	// parameters from FireRenderMesh. If we'll skip setting RPRX_UBER_MATERIAL_DISPLACEMENT parameter here,
	// RPRX will reset displacement map in some unpredicted cases.
	MObject displacementNode = GetDisplacementNode();
	if (displacementNode != MObject::kNullObj)
	{
		MFnDependencyNode dispShaderNode(displacementNode);
		FireMaya::Displacement* displacement = dynamic_cast<FireMaya::Displacement*>(dispShaderNode.userNode());
		if (displacement)
		{
			float minHeight, maxHeight, creaseWeight;
			int subdivision, boundary;
			frw::Value mapValue;

			bool haveDisplacement = displacement->getValues(mapValue, scope, minHeight, maxHeight, subdivision, creaseWeight, boundary);
			if (haveDisplacement)
			{
				shader.xSetValue(RPRX_UBER_MATERIAL_DISPLACEMENT, mapValue);
			}
		}
	}
	
	frw::Value shadowColor = scope.GetValue(shaderNode.findPlug(Attribute::shadowColor));
	frw::Value shadowAlpha = scope.GetValue(shaderNode.findPlug(Attribute::shadowTransparency));
	if (shadowColor.IsFloat())
	{
		float r = shadowColor.GetX();
		float g = shadowColor.GetY();
		float b = shadowColor.GetZ();
		float a = shadowAlpha.GetX();
		shader.SetShadowColor(r, g, b, a);
	}

	frw::Value shadowWeight = scope.GetValue(shaderNode.findPlug(Attribute::shadowWeight));
	if (shadowWeight.IsFloat())
	{
		shader.SetShadowWeight(shadowWeight.GetX());
	}

	bool bgIsEnv = shaderNode.findPlug(Attribute::bgIsEnv).asBool();
	shader.SetBackgroundIsEnvironment(bgIsEnv);
	
	if (!bgIsEnv)
	{
		shader.xSetValue(RPRX_UBER_MATERIAL_DIFFUSE_COLOR, scope.GetValue(shaderNode.findPlug(Attribute::bgColor)));
		shader.xSetValue(RPRX_UBER_MATERIAL_DIFFUSE_WEIGHT, scope.GetValue(shaderNode.findPlug(Attribute::bgWeight)));
		shader.xSetValue(RPRX_UBER_MATERIAL_TRANSPARENCY, scope.GetValue(shaderNode.findPlug(Attribute::bgTransparency)));
	}

	shader.SetShadowCatcher(true);

	return shader;
}

MObject FireMaya::ShadowCatcherMaterial::GetDisplacementNode()
{
	MFnDependencyNode shaderNode(thisMObject());

	if (!shaderNode.findPlug(Attribute::useDispMap).asBool())
		return MObject::kNullObj;

	MPlug plug = shaderNode.findPlug(Attribute::dispMap);
	if (plug.isNull())
		return MObject::kNullObj;

	MPlugArray shaderConnections;
	plug.connectedTo(shaderConnections, true, false);
	if (shaderConnections.length() == 0)
		return MObject::kNullObj;
	MObject node = shaderConnections[0].node();
	return node;
}
