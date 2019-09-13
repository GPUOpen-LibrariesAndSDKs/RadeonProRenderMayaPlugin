#pragma once
#include "Math/mathutils.h"

/** Convert degrees to radians. */
inline float toDegrees(float radians)
{
	return radians * (180.0f / MY_PI);
}

/** Convert radians to degrees. */
inline float toRadians(float degrees)
{
	return degrees * (MY_PI / 180.0f);
}

inline MColor ConvertKelvinToColor(float kelvin)
{
	//Temperature must fall between 1000 and 40000 degrees
	kelvin = RadeonProRender::clamp(kelvin, 1000, 40000) * 0.01;

	float r, g, b;

	// Red
	if (kelvin <= 66)
	{
		r = 1;
	}
	else
	{
		float tmp = 329.698727446 * (std::pow(kelvin - 60, -0.1332047592));
		r = tmp * 1 / 255;
		r = RadeonProRender::clamp(r, 0, 1);
	}

	// Green
	if (kelvin <= 66)
	{
		float tmp = 99.4708025861 * std::log(kelvin) - 161.1195681661;
		g = tmp * 1 / 255;
		g = RadeonProRender::clamp(g, 0, 1);
	}
	else
	{
		float tmp = 288.1221695283 * (std::pow(kelvin - 60, -0.0755148492));
		g = tmp * 1 / 255;
		g = RadeonProRender::clamp(g, 0, 1);
	}

	// Blue
	if (kelvin >= 66)
	{
		b = 1;
	}
	else if (kelvin <= 19)
	{
		b = 0;
	}
	else
	{
		float tmp = 138.5177312231 * std::log(kelvin - 10) - 305.0447927307;
		b = tmp * 1 / 255;
		b = RadeonProRender::clamp(b, 0, 1);
	}

	return { r, g, b };
}