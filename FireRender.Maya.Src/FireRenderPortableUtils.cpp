#include "FireRenderPortableUtils.h"

namespace FireMaya
{
	const float RayCastEpsilon::MaxEpsilon = 0.02f;
	const float RayCastEpsilon::MinEpsilon = 0.000'002f;

	float RayCastEpsilon::Calculate(double width, double height, double depth)
	{
		double volume = width * height * depth;

		double cutoff = 1'000'000.0f;
		for (float epsilon = MinEpsilon; epsilon < MaxEpsilon; epsilon *= 10, cutoff *= 10)
			if (volume < cutoff)
				return epsilon;

		return MaxEpsilon;
	}
}
