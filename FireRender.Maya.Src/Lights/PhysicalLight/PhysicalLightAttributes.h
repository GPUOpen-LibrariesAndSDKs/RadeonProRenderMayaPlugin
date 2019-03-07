#pragma once


#include <maya/MObject.h>
#include <maya/MColor.h>
#include <maya/MFnDependencyNode.h>

#include "PhysicalLightData.h"

class PhysicalLightAttributes
{
public:
	PhysicalLightAttributes();
	~PhysicalLightAttributes();

	static void Initialize();

	static bool GetEnabled(const MFnDependencyNode& node);
	static PLType GetLightType(const MFnDependencyNode& node);
	static PLAreaLightShape GetAreaLightShape(const MFnDependencyNode& node);
	static bool GetAreaLightVisible(const MFnDependencyNode& node);

	static float GetAreaWidth(const MFnDependencyNode& node);
	static float GetAreaLength(const MFnDependencyNode& node);

	static PLColorMode GetColorMode(const MFnDependencyNode& node);
	static MColor GetColor(const MFnDependencyNode& node);
	static MColor GetTempreratureColor(const MFnDependencyNode& node);
	static float GetIntensity(const MFnDependencyNode& node);
	static float GetLuminousEfficacy(const MFnDependencyNode& node);
	static PLIntensityUnit GetIntensityUnits(const MFnDependencyNode& node);

	static void GetSpotLightSettings(const MFnDependencyNode& node, float& innerAngle, float& outerfalloff);

	static bool GetShadowsEnabled(const MFnDependencyNode& node);
	static float GetShadowsSoftness(const MFnDependencyNode& node);
	static MString GetAreaLightMeshSelectedName(const MFnDependencyNode& node);

public:
	// General
	static MObject enabled;
	static MObject lightType;
	static MObject areaLength;
	static MObject areaWidth;
	static MObject targeted;

	// Intensity
	static MObject lightIntensity;
	static MObject intensityUnits;
	static MObject luminousEfficacy;
	static MObject colorMode;
	static MObject colorPicker;
	static MObject temperature;
	static MObject temperatureColor;

	// Area Light
	static MObject areaLightVisible;
	static MObject areaLightBidirectional;
	static MObject areaLightShape;
	static MObject areaLightSelectingMesh;
	static MObject areaLightMeshSelectedName;
	static MObject areaLightIntensityNorm;

	// Spot Light
	static MObject spotLightVisible;
	static MObject spotLightInnerConeAngle;
	static MObject spotLightOuterConeFalloff;

	//Light Decay
	static MObject decayType;
	static MObject decayFalloffStart;
	static MObject decayFalloffEnd;

	// Shadows
	static MObject shadowsEnabled;
	static MObject shadowsSoftness;
	static MObject shadowsTransparency;

	// Volume
	static MObject volumeScale;

public:
	static void FillPhysicalLightData(PhysicalLightData& data, const MObject& node, FireMaya::Scope* scope);

private:
	static void CreateGeneralAttributes();
	static void CreateIntensityAttributes();
	static void CreateAreaLightAttrbutes();
	static void CreateSpotLightAttrbutes();
	static void CreateDecayAttrbutes();
	static void CreateShadowsAttrbutes();
	static void CreateVolumeAttrbutes();
	static void CreateHiddenAttributes();
};

