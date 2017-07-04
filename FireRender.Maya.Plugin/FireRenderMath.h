#pragma once

/** Define PI. */
#define PI 3.14159265358979323846
#define PI_F 3.14159265358979323846f

/** Convert degrees to radians. */
inline float toDegrees(float radians)
{
	return radians * (180.0f / PI_F);
}

/** Convert radians to degrees. */
inline float toRadians(float degrees)
{
	return degrees * (PI_F / 180.0f);
}
