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
#pragma once

#include "FireMaya.h"
#include <maya/MObject.h>
#include <maya/MColor.h>

// Meaningful values should starts from zero (use as indices)
enum PLType
{
	PLTUnknown = -1,
	PLTArea = 0,
	PLTSpot = 1,
	PLTPoint = 2,
	PLTDirectional = 3,
	PLTSphere = 4,		// RPR 2.0 only
	PLTDisk = 5			// RPR 2.0 only
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

	// Disk
	float diskRadius;
	float diskAngle;

	// Sphere

	float sphereRadius;

	bool shadowsEnabled;
	float shadowsSoftnessAngle;

	MColor GetChosenColor() const;
	float GetCalculatedIntensity(float area = 1.0f) const;
};
