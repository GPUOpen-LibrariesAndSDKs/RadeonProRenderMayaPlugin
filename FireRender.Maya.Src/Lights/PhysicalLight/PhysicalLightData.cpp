#include "PhysicalLightData.h"

#include <maya/MFloatVector.h>
#include <maya/MDistance.h>

#include <vector>

const float PhysicalLightData::defaultLuminousEfficacy = 17.0f;

MColor PhysicalLightData::GetChosenColor() const
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
float PhysicalLightData::GetCalculatedIntensity(float area) const
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
