#pragma once

#include "frWrap.h"

namespace MayaStandardNodeConverters
{
	// Forward declarations
	class BaseConverter;
	struct ConverterParams;

	enum class MayaValueId
	{
		// maps
		Place2dTexture = 0x52504c32,
		File = 0x52544654,
		Checker = 0x52544348,
		Ramp = 0x52545241,
		Noise = 0x52544e33,
		Bump2d = 0x5242554D,
		RemapHSV = 0x524d4853,
		GammaCorrect = 0x5247414d,
		ReverseMap = 0x52525653,
		LayeredTexture = 0x4c595254,

		// arithmetic
		AddDoubleLinear = 0x4441444c,
		BlendColors = 0x52424c32,
		Contrast = 0x52434f4e,
		MultDoubleLinear = 0x444d444c,
		MultiplyDivide = 0x524d4449,
		PlusMinusAverage = 0x52504d41,
		VectorProduct = 0x52564543,
		BlendTwoAttr = 0x41424C32,
		Clamp = 0x52434c33
	};

	class NodeConverterUtil
	{
	public:
		static frw::Value Convert(const ConverterParams& params, const MayaValueId mayaNodeId);
	private:
		static std::unique_ptr<BaseConverter> CreateConverter(const ConverterParams& params, const MayaValueId mayaNodeId);
	};

}