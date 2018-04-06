#pragma once

#include "FireMaya.h"
#include <maya/MFloatVector.h>
#include <maya/MColor.h>
#include <maya/MObject.h>
#include <maya/MDistance.h>
#include <vector>

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

	MColor GetChosenColor() const
	{
		if (colorMode == PLCColor)
		{
			return colorBase;
		}
		else
		{
			return temperatureColorBase;
		}
	}

	// area in Maya's units
	float GetCalculatedIntensity(float area = 1.0f) const
	{
		float maxEfficacy = 684.0f;
		float luminousEfficiency = luminousEfficacy / maxEfficacy;

		float calculatedIntensity = 0.0f;
		// In case of area light we need to setup Emiisive shader color. 
		// That's mean that we need pass something like Watts / per square meter
		if (lightType == PLTArea)
		{
			float factor = (float)MDistance::uiToInternal(0.01);
			area *= factor * factor;	// Maya's units to m2

			switch (intensityUnits)
			{
			case PLTIULumen:
				calculatedIntensity = intensity / area;
				break;
			case PLTIULuminance:
				calculatedIntensity = intensity;
				break;
			case PLTIUWatts:
				calculatedIntensity = intensity * luminousEfficiency / area;
				break;
			case PLTIURadiance:
				calculatedIntensity = intensity * luminousEfficiency;
				break;
            case PLTIUUnknown:
                    assert(false);
                    break;
			}
		}
		else if (lightType == PLTDirectional)
		{
			switch (intensityUnits)
			{
			case PLTIULuminance:
				calculatedIntensity = intensity;
				break;
			case PLTIURadiance:
				calculatedIntensity = intensity * luminousEfficiency;
				break;
			default: // _TODO need to disable in UI
				calculatedIntensity = intensity;
			}
		}
		else // spot or point
		{
			switch (intensityUnits)
			{
			case PLTIULumen:
				calculatedIntensity = intensity;
				break;
			case PLTIUWatts:
				calculatedIntensity = intensity * luminousEfficiency;
				break;
			default: // _TODO need to disable in UI
				calculatedIntensity = intensity;
			}
		}

		return calculatedIntensity;
	}

	frw::Value resultFrwColor;
	// Spot
	float spotInnerAngle;
	float spotOuterFallOff;

	bool shadowsEnabled;
	float shadowsSoftness;
};
