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

#include "maya/MObject.h"
#include "maya/MImage.h"
#include "SkyAttributes.h"
#include <maya/MFloatVector.h>
#include <memory>

// The following define will activate code which uses directional light as primary light
// source for sky. It is obsolete code and should be considered for removal later.
//#define USE_DIRECTIONAL_SKY_LIGHT 1

// Forward declarations
namespace frw
{
	class Context;
	class Image;
}

struct SkyRgbFloat32;

/**
 * The sky builder uses the sky dependency node as input
 * and reads the attributes to calculate the position of
 * the sun and update the sphere environment map.
 */
class SkyBuilder
{
public:

	// Life Cycle
	// -----------------------------------------------------------------------------

	SkyBuilder() = delete;

	SkyBuilder(const MObject& object,
		unsigned int imageWidth = 1024,
		unsigned int imageHeight = 1024);

	virtual ~SkyBuilder();


	// Public Methods
	// -----------------------------------------------------------------------------

	/** Refresh the sky. Return true if it has changed. */
	bool refresh();

	/** Update an RPR image with the sky. */
	void updateImage(frw::Context& context, frw::Image& image);

	/** Update a Maya sample image with the sky. */
	void updateSampleImage(MImage& image);

	/** Get the sun light color. */
	MColor& getSunLightColor();

	/** Get the sun azimuth in radians. */
	float getSunAzimuth();

	/** Get the sun altitude in radians. */
	float getSunAltitude();

	/** Get the sun direction in Maya world space. */
	MFloatVector getWorldSpaceSunDirection();

	/** Get the intensity of sky. */
	float getSkyIntensity();

private:

	// Members
	// -----------------------------------------------------------------------------

	/** Sky attributes. */
	SkyAttributes m_attributes;


	/** Sun azimuth in radians. */
	float m_sunAzimuth;

	/** Sun altitude in radians. */
	float m_sunAltitude;

	/** Sun direction vector. */
	MFloatVector m_sunDirection;

	/** The color of the directional sun light. */
	MColor m_sunLightColor;


	/** Sky image width. */
	const unsigned int m_imageWidth;

	/** Sky image height. */
	const unsigned int m_imageHeight;

	/** Storage space for the sky image. */
	std::unique_ptr<SkyRgbFloat32[]> m_imageBuffer;

	// Private Methods
	// -----------------------------------------------------------------------------

	/** Calculate the sun position. */
	void calculateSunPosition();

	/** Calculate the sun position from location, date and time. */
	void calculateSunPositionGeographic();

	/** Adjust the given time values for daylight saving. */
	void adjustDaylightSavingTime(int& hours, int& day, int& month, int& year) const;

	/** Create the sky sphere map. */
	void createSkyImage();
};
