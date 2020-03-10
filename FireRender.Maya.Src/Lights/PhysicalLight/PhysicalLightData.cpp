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
