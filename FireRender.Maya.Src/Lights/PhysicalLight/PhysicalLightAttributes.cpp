#include "FireMaya.h"
#include "PhysicalLightAttributes.h"
#include "FireRenderUtils.h"

#include <maya/MPxNode.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnTypedAttribute.h>

MObject PhysicalLightAttributes::enabled;
MObject PhysicalLightAttributes::lightType;
MObject PhysicalLightAttributes::areaLength;
MObject PhysicalLightAttributes::areaWidth;
MObject PhysicalLightAttributes::targeted;

// Intensity
MObject PhysicalLightAttributes::lightIntensity;
MObject PhysicalLightAttributes::intensityUnits;
MObject PhysicalLightAttributes::luminousEfficacy;
MObject PhysicalLightAttributes::colorMode;
MObject PhysicalLightAttributes::colorPicker;
MObject PhysicalLightAttributes::temperature;
MObject PhysicalLightAttributes::temperatureColor;

// Area Light
MObject PhysicalLightAttributes::areaLightVisible;
MObject PhysicalLightAttributes::areaLightBidirectional;
MObject PhysicalLightAttributes::areaLightShape;
MObject PhysicalLightAttributes::areaLightSelectingMesh;
MObject PhysicalLightAttributes::areaLightMeshSelectedName;
MObject PhysicalLightAttributes::areaLightIntensityNorm;

// Spot Light
MObject PhysicalLightAttributes::spotLightVisible;
MObject PhysicalLightAttributes::spotLightInnerConeAngle;
MObject PhysicalLightAttributes::spotLightOuterConeFalloff;

//Light Decay
MObject PhysicalLightAttributes::decayType;
MObject PhysicalLightAttributes::decayFalloffStart;
MObject PhysicalLightAttributes::decayFalloffEnd;

// Shadows
MObject PhysicalLightAttributes::shadowsEnabled;
MObject PhysicalLightAttributes::shadowsSoftness;
MObject PhysicalLightAttributes::shadowsTransparency;

// Volume
MObject PhysicalLightAttributes::volumeScale;


void setAttribProps(MFnAttribute& attr, const MObject& attrObj)
{
	CHECK_MSTATUS(attr.setKeyable(true));
	CHECK_MSTATUS(attr.setStorable(true));
	CHECK_MSTATUS(attr.setReadable(true));
	CHECK_MSTATUS(attr.setWritable(true));

	CHECK_MSTATUS(MPxNode::addAttribute(attrObj));
}

void CreateIntAttribute(MObject& propObject, const char* name, const char* shortName, int minVal, int maxVal, int defaultVal)
{
	MFnNumericAttribute nAttr;

	propObject = nAttr.create(name, shortName, MFnNumericData::kInt, defaultVal);

	nAttr.setMin(minVal);
	nAttr.setSoftMax(maxVal);
	setAttribProps(nAttr, propObject);
}

void CreateFloatAttribute(MObject& propObject, const char* name, const char* shortName, float minVal, float maxVal, float defaultVal)
{
	MFnNumericAttribute nAttr;

	propObject = nAttr.create(name, shortName, MFnNumericData::kFloat, defaultVal);

	nAttr.setMin(minVal);
	nAttr.setSoftMax(maxVal);
	setAttribProps(nAttr, propObject);
}

void CreateBoolAttribute(MObject& propObject, const char* name, const char* shortName, bool defaultVal)
{
	MFnNumericAttribute nAttr;

	propObject = nAttr.create(name, shortName, MFnNumericData::kBoolean, defaultVal);

	setAttribProps(nAttr, propObject);
}

void CreateEnumAttribute(MObject& propObject, const char* name, const char* shortName, const std::vector<const char*>& values, int defaultIndex)
{
	MFnEnumAttribute eAttr;

	propObject = eAttr.create(name, shortName, defaultIndex);

	for (int i = 0; i < values.size(); ++i)
	{
		eAttr.addField(values[i], i);
	}

	setAttribProps(eAttr, propObject);
}

void CreateColor(MObject& propObject, const char* name, const char* shortName, bool readOnly = false)
{
	MFnNumericAttribute nAttr;

	propObject = nAttr.createColor(name, shortName);
	CHECK_MSTATUS(nAttr.setDefault(1.0f, 1.0f, 1.0f));

	setAttribProps(nAttr, propObject);

	if (readOnly)
	{
		nAttr.setWritable(false);
	}
}

void PhysicalLightAttributes::Initialize()
{
	CreateGeneralAttributes();
	CreateIntensityAttributes();
	CreateAreaLightAttrbutes();
	CreateSpotLightAttrbutes();
	CreateDecayAttrbutes();
	CreateShadowsAttrbutes();
	CreateVolumeAttrbutes();
	CreateHiddenAttributes();
}

void PhysicalLightAttributes::CreateGeneralAttributes()
{
	float maxVal = 10.0f;

	CreateBoolAttribute(enabled, "enabled", "en", true);

	std::vector<const char*> values = { "Area" , "Spot", "Point", "Directional" };

	CreateEnumAttribute(lightType, "lightType", "lt", values, PLTArea);

	CreateFloatAttribute(areaLength, "areaLength", "al", 0.0f, maxVal, 1.0f);
	CreateFloatAttribute(areaWidth, "areaWidth", "aw", 0.0f, maxVal, 1.0f);

	CreateBoolAttribute(targeted, "targeted", "td", false);
}

void PhysicalLightAttributes::CreateIntensityAttributes()
{
	float maxVal = 100;

	CreateFloatAttribute(lightIntensity, "lightIntensity", "li", 0.0f, maxVal, 1.0f);

	CreateIntAttribute(intensityUnits, "intensityUnits", "iu", 0, 3, PLTIULuminance);

	CreateFloatAttribute(luminousEfficacy, "luminousEfficacy", "le", 0.1f, maxVal, 17.0f);

	std::vector<const char*> colorValues = { "Color", "Temperature" };
	CreateEnumAttribute(colorMode, "colorMode", "cm", colorValues, PLCColor);

	CreateColor(colorPicker, "colorPicker", "cp");

	CreateFloatAttribute(temperature, "temperature", "t", 0.0f, 40000.0f, 1.0f);

	CreateColor(temperatureColor, "temperatureColor", "tc", true);
}

void PhysicalLightAttributes::CreateAreaLightAttrbutes()
{
	CreateBoolAttribute(areaLightVisible, "areaLightVisible", "alv", false);
	CreateBoolAttribute(areaLightBidirectional, "areaLightBidirectional", "albd", false);

	std::vector<const char*> values = { "Disc", "Cylinder", "Sphere", "Rectangle", "Mesh" };
	CreateEnumAttribute(areaLightShape, "areaLightShape", "alls", values, PLARectangle);
	CreateBoolAttribute(areaLightIntensityNorm, "areaLightIntensityNorm", "alin", false);
}

void PhysicalLightAttributes::CreateSpotLightAttrbutes()
{
	CreateBoolAttribute(spotLightVisible, "spotLightVisible", "slv", false);

	CreateFloatAttribute(spotLightInnerConeAngle, "spotLightInnerConeAngle", "slia", 0.0f, 179.0f, 43.0f);
	CreateFloatAttribute(spotLightOuterConeFalloff, "spotLightOuterConeFalloff", "slof", 0.0f, 179.0f, 45.0f);
}

void PhysicalLightAttributes::CreateDecayAttrbutes()
{
	float maxVal = 100;

	std::vector<const char*> values = { "None", "InverseSq", "Linear" };
	CreateEnumAttribute(decayType, "decayType", "dt", values, PLDNone);

	CreateFloatAttribute(decayFalloffStart, "decayFalloffStart", "dfs", 0.0f, maxVal, 1.0f);
	CreateFloatAttribute(decayFalloffEnd, "decayFalloffEnd", "dfe", 0.0f, maxVal, 1.0f);
}

void PhysicalLightAttributes::CreateShadowsAttrbutes()
{
	CreateBoolAttribute(shadowsEnabled, "shadowsEnabled", "se", true);

	CreateFloatAttribute(shadowsSoftness, "shadowsSoftness", "ss", 0.0f, 1.0f, 0.5f);
	CreateFloatAttribute(shadowsTransparency, "shadowsTransparency", "st", 0.0f, 1.0f, 1.0f);
}

void PhysicalLightAttributes::CreateVolumeAttrbutes()
{
	CreateFloatAttribute(volumeScale, "volumeScale", "vs", 0.0f, 1.0f, 1.0f);
}

void PhysicalLightAttributes::CreateHiddenAttributes()
{
	MFnTypedAttribute tAttr;
	MStatus status;

	CreateBoolAttribute(areaLightSelectingMesh, "areaLightSelectingMesh", "alsm", false);


	areaLightMeshSelectedName = tAttr.create("areaLightMeshSelectedName", "almsn", MFnData::kString, &status);
	tAttr.setHidden(true);
	setAttribProps(tAttr, areaLightMeshSelectedName);
}

MColor GetColorAttribute(const MFnDependencyNode& node, const MObject& attribute)
{
	MPlug plug = node.findPlug(attribute);

	_ASSERT(!plug.isNull());

	if (!plug.isNull())
	{
		float r, g, b;

		plug.child(0).getValue(r);
		plug.child(1).getValue(g);
		plug.child(2).getValue(b);

		return MColor(r, g, b);
	}

	return MColor::kOpaqueBlack;
}

int GetIntAttribute(const MFnDependencyNode& node, const MObject& attribute)
{
	MPlug plug = node.findPlug(attribute);

	_ASSERT(!plug.isNull());

	if (!plug.isNull())
	{
		return plug.asInt();
	}

	return -1;
}

float GetFloatAttribute(const MFnDependencyNode& node, const MObject& attribute)
{
	MPlug plug = node.findPlug(attribute);

	_ASSERT(!plug.isNull());
	if (!plug.isNull())
	{
		return plug.asFloat();
	}

	return 0.0f;
}

bool GetBoolAttribute(const MFnDependencyNode& node, const MObject& attribute)
{
	MPlug plug = node.findPlug(attribute);

	_ASSERT(!plug.isNull());
	if (!plug.isNull())
	{
		return plug.asBool();
	}

	return false;
}

MString GetStringAttribute(const MFnDependencyNode& node, const MObject& attribute)
{
	MPlug plug = node.findPlug(attribute);

	_ASSERT(!plug.isNull());
	if (!plug.isNull())
	{
		return plug.asString();
	}

	return "";
}

bool PhysicalLightAttributes::GetEnabled(const MFnDependencyNode& node)
{
	return GetBoolAttribute(node, PhysicalLightAttributes::enabled);
}

PLColorMode PhysicalLightAttributes::GetColorMode(const MFnDependencyNode& node)
{
	return (PLColorMode)GetIntAttribute(node, PhysicalLightAttributes::colorMode);
}

// Get light color for non connected map case
MColor PhysicalLightAttributes::GetColor(const MFnDependencyNode& node)
{
	return GetColorAttribute(node, PhysicalLightAttributes::colorPicker);
}

MColor PhysicalLightAttributes::GetTempreratureColor(const MFnDependencyNode& node)
{
	return GetColorAttribute(node, PhysicalLightAttributes::temperatureColor);
}

float PhysicalLightAttributes::GetIntensity(const MFnDependencyNode& node)
{
	return GetFloatAttribute(node, PhysicalLightAttributes::lightIntensity);
}

float PhysicalLightAttributes::GetLuminousEfficacy(const MFnDependencyNode& node)
{
	return GetFloatAttribute(node, PhysicalLightAttributes::luminousEfficacy);
}

PLIntensityUnit PhysicalLightAttributes::GetIntensityUnits(const MFnDependencyNode& node)
{
	return (PLIntensityUnit) GetIntAttribute(node, PhysicalLightAttributes::intensityUnits);
}

PLAreaLightShape PhysicalLightAttributes::GetAreaLightShape(const MFnDependencyNode& node)
{
	return (PLAreaLightShape) GetIntAttribute(node, PhysicalLightAttributes::areaLightShape);
}

bool PhysicalLightAttributes::GetAreaLightVisible(const MFnDependencyNode& node)
{
	return GetBoolAttribute(node, PhysicalLightAttributes::areaLightVisible);
}

float PhysicalLightAttributes::GetAreaWidth(const MFnDependencyNode& node)
{
	return GetFloatAttribute(node, areaWidth);
}

float PhysicalLightAttributes::GetAreaLength(const MFnDependencyNode& node)
{
	return GetFloatAttribute(node, areaLength);
}

PLType PhysicalLightAttributes::GetLightType(const MFnDependencyNode& node)
{
	return (PLType)GetIntAttribute(node, PhysicalLightAttributes::lightType);
}

void PhysicalLightAttributes::GetSpotLightSettings(const MFnDependencyNode& node, float& innerAngle, float& outerfalloff)
{
	innerAngle = GetFloatAttribute(node, PhysicalLightAttributes::spotLightInnerConeAngle);
	outerfalloff = GetFloatAttribute(node, PhysicalLightAttributes::spotLightOuterConeFalloff);
}

bool PhysicalLightAttributes::GetShadowsEnabled(const MFnDependencyNode& node)
{
	return GetBoolAttribute(node, PhysicalLightAttributes::shadowsEnabled);
}

float PhysicalLightAttributes::GetShadowsSoftness(const MFnDependencyNode& node)
{
	return GetFloatAttribute(node, PhysicalLightAttributes::shadowsSoftness);
}

void PhysicalLightAttributes::FillPhysicalLightData(PhysicalLightData& physicalLightData, const MObject& node, FireMaya::Scope* scope)
{
	physicalLightData.enabled = PhysicalLightAttributes::GetEnabled(node);

	physicalLightData.lightType = PhysicalLightAttributes::GetLightType(node);

	physicalLightData.colorMode = PhysicalLightAttributes::GetColorMode(node);
	physicalLightData.temperatureColorBase = PhysicalLightAttributes::GetTempreratureColor(node);
	physicalLightData.colorBase = PhysicalLightAttributes::GetColor(node);
	physicalLightData.resultFrwColor = frw::Value(physicalLightData.colorBase.r, 
											physicalLightData.colorBase.g, physicalLightData.colorBase.b);

	physicalLightData.intensity = PhysicalLightAttributes::GetIntensity(node);
	physicalLightData.luminousEfficacy = PhysicalLightAttributes::GetLuminousEfficacy(node);
	physicalLightData.intensityUnits = PhysicalLightAttributes::GetIntensityUnits(node);

	physicalLightData.areaWidth = GetAreaWidth(node);
	physicalLightData.areaLength = GetAreaLength(node);

	physicalLightData.shadowsEnabled = GetShadowsEnabled(node);
	physicalLightData.shadowsSoftness = GetShadowsSoftness(node);

	if (physicalLightData.lightType == PLTArea)
	{
		physicalLightData.areaLightShape = PhysicalLightAttributes::GetAreaLightShape(node);
		physicalLightData.areaLightVisible = PhysicalLightAttributes::GetAreaLightVisible(node);

		if (scope != nullptr)
		{
			frw::Value frwColor;
			if (physicalLightData.colorMode == PLCColor)
			{
				MPlug plug = MFnDependencyNode(node).findPlug(PhysicalLightAttributes::colorPicker);
				frwColor = scope->GetValue(plug);
			}
			else
			{
				MColor color = physicalLightData.temperatureColorBase;
				frwColor = frw::Value(color.r, color.g, color.b);
			}

			physicalLightData.resultFrwColor = frwColor;
		}
	} 
	else if (physicalLightData.lightType == PLTSpot)
	{
		GetSpotLightSettings(node, physicalLightData.spotInnerAngle, physicalLightData.spotOuterFallOff);

		// We should divide by 2 because Core accepts half of an angle value
		physicalLightData.spotInnerAngle = (float) deg2rad(physicalLightData.spotInnerAngle / 2.0f);
		physicalLightData.spotOuterFallOff = (float) deg2rad(physicalLightData.spotOuterFallOff / 2.0f);
	}
}
