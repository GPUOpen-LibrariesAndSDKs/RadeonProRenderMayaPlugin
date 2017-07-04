#pragma once

namespace FireMaya
{
	// Calculates value for raycastepsilon parameter based on the size of the scene
	class RayCastEpsilon
	{
	public:
		static const float MaxEpsilon;
		static const float MinEpsilon;

	public:
		static float Calculate(double width, double height, double depth);
	};
};

