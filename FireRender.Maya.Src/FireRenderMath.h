#pragma once
#include <algorithm>

template<typename T>
inline T Clamp(T value, T min, T max)
{
	return std::min<T>(max, std::max<T>(value, min));
}

/** Convert degrees to radians. */
inline float toDegrees(float radians)
{
	return radians * (180.0f / M_PI);
}

/** Convert radians to degrees. */
inline float toRadians(float degrees)
{
	return degrees * (M_PI / 180.0f);
}

inline MColor ConvertKelvinToColor(double kelvin)
{
	//Temperature must fall between 1000 and 40000 degrees
	kelvin = Clamp(kelvin, 1000.0, 40000.0) * 0.01;

	double r, g, b;

	// Red
	if (kelvin <= 66.0)
	{
		r = 1.0;
	}
	else
	{
		double temp = 329.698727446 * (std::pow(kelvin - 60, -0.1332047592)) / 255;
		r = Clamp(temp, 0.0, 1.0);
	}

	// Green
	if (kelvin <= 66.0)
	{
		double temp = (99.4708025861 * std::log(kelvin) - 161.1195681661) / 255;
		g = Clamp(temp, 0.0, 1.0);
	}
	else
	{
		double temp = 288.1221695283 * (std::pow(kelvin - 60, -0.0755148492)) / 255;
		g = Clamp(temp, 0.0, 1.0);
	}

	// Blue
	if (kelvin >= 66.0)
	{
		b = 1.0;
	}
	else if (kelvin <= 19.0)
	{
		b = 0.0;
	}
	else
	{
		double temp = (138.5177312231 * std::log(kelvin - 10) - 305.0447927307) / 255;
		b = Clamp(temp, 0.0, 1.0);
	}

	return { static_cast<float>(r), static_cast<float>(g), static_cast<float>(b) };
}
