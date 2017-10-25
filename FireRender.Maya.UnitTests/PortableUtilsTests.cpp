#include "stdafx.h"
#include "CppUnitTest.h"

#include "..\FireRender.Maya.Src\FireRenderPortableUtils.h"

using namespace FireMaya;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace fireRenderUnitTests
{
	TEST_CLASS(PortableUtilsTests)
	{
	public:
		TEST_METHOD(EmptyBoundingBox_ShouldReturn_MinEpsilon)
		{
			auto actual = RayCastEpsilon::Calculate(0, 0, 0);

			Assert::AreEqual(RayCastEpsilon::MinEpsilon, actual);
		}

		TEST_METHOD(DinosourEyeSceneBox_ShouldReturn_MinEpsilon)
		{
			auto actual = RayCastEpsilon::Calculate(2, 2, 3.62);

			Assert::AreEqual(RayCastEpsilon::MinEpsilon, actual);
		}

		TEST_METHOD(Dinosour_RPRSceneBox_ShouldReturn_2PowMinus5Epsilon)
		{
			auto actual = RayCastEpsilon::Calculate(128, 287, 53);

			Assert::AreEqual(0.000'02f, actual);
		}

		TEST_METHOD(LighthouseSceneBox_ShouldReturn_MaxEpsilon)
		{
			auto actual = RayCastEpsilon::Calculate(4867, 1087, 2135);

			Assert::AreEqual(RayCastEpsilon::MaxEpsilon, actual);
		}

		TEST_METHOD(BoothDemoSceneBox_ShouldReturn_MaxEpsilon)
		{
			auto actual = RayCastEpsilon::Calculate(5000, 5113, 935);

			Assert::AreEqual(RayCastEpsilon::MaxEpsilon, actual);
		}

		TEST_METHOD(DragonSceneBox_ShouldReturn_MaxEpsilon)
		{
			auto actual = RayCastEpsilon::Calculate(5894, 2145, 5794);

			Assert::AreEqual(RayCastEpsilon::MaxEpsilon, actual);
		}

		TEST_METHOD(Helmet_DemoSceneBox_ShouldReturn_2PowMinus4Epsilon)
		{
			auto actual = RayCastEpsilon::Calculate(400, 400, 143);

			Assert::AreEqual(0.000'2f, actual);
		}

		TEST_METHOD(MaxBoundingBox_ShouldReturn_MaxEpsilon)
		{
			auto actual = RayCastEpsilon::Calculate(std::numeric_limits<double>::max(), std::numeric_limits<double>::max(), std::numeric_limits<double>::max());

			Assert::AreEqual(RayCastEpsilon::MaxEpsilon, actual);
		}

	};
}
