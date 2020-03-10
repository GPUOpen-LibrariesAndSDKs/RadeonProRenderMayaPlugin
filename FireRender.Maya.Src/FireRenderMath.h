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
#include <algorithm>

template<typename T>
inline T Clamp(T value, T min, T max)
{
	return std::min<T>(max, std::max<T>(value, min));
}

/** Convert degrees to radians. */
inline float toDegrees(float radians)
{
	return (float) (radians * (180.0f / M_PI));
}

/** Convert radians to degrees. */
inline float toRadians(float degrees)
{
	return (float) (degrees * (M_PI / 180.0f));
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
		double temp = 329.698727446 * (pow(kelvin - 60, -0.1332047592)) / 255;
		r = Clamp(temp, 0.0, 1.0);
	}

	// Green
	if (kelvin <= 66.0)
	{
		double temp = (99.4708025861 * log(kelvin) - 161.1195681661) / 255;
		g = Clamp(temp, 0.0, 1.0);
	}
	else
	{
		double temp = 288.1221695283 * (pow(kelvin - 60, -0.0755148492)) / 255;
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
		double temp = (138.5177312231 * log(kelvin - 10) - 305.0447927307) / 255;
		b = Clamp(temp, 0.0, 1.0);
	}

	// the code above makes color RGB value in gamma 2.2. We need this value in gamma 1.0
	r = pow(r, 2.2);
	g = pow(g, 2.2);
	b = pow(b, 2.2);

	return { static_cast<float>(r), static_cast<float>(g), static_cast<float>(b) };
}

// Function to check are two float point values equal or not.
template<typename T>
inline typename std::enable_if_t<std::is_floating_point<T>::value, bool> IsAlmostEqual(T lhs, T rhs)
{
	return std::abs(lhs - rhs) <=
		std::numeric_limits<T>::epsilon() *
		std::max(std::abs(lhs), std::abs(rhs));
}
