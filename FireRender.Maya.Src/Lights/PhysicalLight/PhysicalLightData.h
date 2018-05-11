#pragma once

#include "FireMaya.h"
#include <maya/MObject.h>
#include <maya/MColor.h>

// Meaningful values should starts from zero (use as indices)
enum PLType
{
	PLTUnknown = -1,
	PLTArea = 0,
	PLTSpot,
	PLTPoint,
	PLTDirectional
};

enum PLIntensityUnit
{
	PLTIUUnknown = -1,
	PLTIULumen = 0,
	PLTIULuminance,
	PLTIUWatts,
	PLTIURadiance
};

enum PLColorMode
{
	PLCUnknown = -1,
	PLCColor = 0,
	PLCTemperature
};

enum PLAreaLightShape
{
	PLAUnknown = -1,
	PLADisc = 0,
	PLACylinder,
	PLASphere,
	PLARectangle,
	PLAMesh
};

enum PLDecay
{
	PLDUnknown = -1,
	PLDNone = 0,
	PLDInverseSq,
	PLDLinear
};

struct PhysicalLightData
{
	static const float defaultLuminousEfficacy;

	bool enabled;
	PLType lightType;
	PLAreaLightShape areaLightShape;
	bool areaLightVisible;
	float areaWidth;
	float areaLength;
	MObject areaLightSelectedMesh;

	PLColorMode colorMode;
	MColor colorBase;
	MColor temperatureColorBase;
	float intensity;
	float luminousEfficacy;
	PLIntensityUnit intensityUnits;

	frw::Value resultFrwColor;
	// Spot
	float spotInnerAngle;
	float spotOuterFallOff;

	bool shadowsEnabled;
	float shadowsSoftness;

	MColor GetChosenColor() const;
	float GetCalculatedIntensity(float area = 1.0f) const;
};
