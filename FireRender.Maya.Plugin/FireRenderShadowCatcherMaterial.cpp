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

#include "FireMaya.h"

namespace
{
	namespace Attribute
	{
		MObject bgIsEnv;
		MObject bgWeight;
		MObject	bgColor;
		MObject refOverride;
		MObject refWeight;
		MObject refColor;
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
		{&Attribute::refWeight, "refWeight", "rw", 0.0, 1.0, 1.0, MFnNumericData::kFloat},
		{&Attribute::shadowTransparency, "shadowTransp", "st", 0.0, 1.0, 0.0, MFnNumericData::kFloat},
		{&Attribute::shadowWeight, "shadowWeight", "sw", 0.0, 1.0, 1.0, MFnNumericData::kFloat}
	};
	std::vector<AttributeEntry<bool>> boolAttributes = 
	{
		{ &Attribute::bgIsEnv, "bgIsEnv", "bgie", 0, 0, 1, MFnNumericData::kBoolean },
		{ &Attribute::refOverride, "refOverride", "ro", 0, 0, 0, MFnNumericData::kBoolean },
		{ &Attribute::useNormalMap, "useNormalMap", "unm", 0, 0, 0, MFnNumericData::kBoolean },
		{ &Attribute::useDispMap, "useDispMap", "udm", 0, 0, 0, MFnNumericData::kBoolean },
		{ &Attribute::disableSwatch, "disableSwatch", "ds", 0, 0, 0, MFnNumericData::kBoolean }
	};

	std::vector<ColorAttributeEntry> colorAttributes = 
	{
		{&Attribute::bgColor, "bgColor", "bgc", {0.0f, 0.0f, 0.0f} },
		{&Attribute::refColor, "refColor", "rc", { 0.0f, 0.0f, 0.0f } },
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
#define GET_BOOL(_attrib_) \
	shaderNode.findPlug(Attribute::_attrib_).asBool()

#define GET_VALUE(_attrib_) \
	scope.GetValue(shaderNode.findPlug(Attribute::_attrib_))
	
	MFnDependencyNode shaderNode(thisMObject());

	frw::Shader shader(scope.MaterialSystem(), scope.Context(), RPRX_MATERIAL_UBER);
	
	// Normal map
	if (GET_BOOL(useNormalMap))
	{
		frw::Value value = GET_VALUE(normalMap);
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

	shader.SetShadowCatcher(true);

	return shader;

#undef GET_BOOL
#undef GET_VALUE
}
