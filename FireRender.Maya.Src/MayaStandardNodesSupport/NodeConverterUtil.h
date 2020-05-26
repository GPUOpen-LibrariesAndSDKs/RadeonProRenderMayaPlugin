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
		Projection = 0x5250524A,

		// arithmetic
		AddDoubleLinear = 0x4441444c,
		BlendColors = 0x52424c32,
		Contrast = 0x52434f4e,
		MultDoubleLinear = 0x444d444c,
		MultiplyDivide = 0x524d4449,
		PlusMinusAverage = 0x52504d41,
		VectorProduct = 0x52564543,
		BlendTwoAttr = 0x41424C32,
		Clamp = 0x52434c33,
		RemapValue = 0x524d564c,
		SetRange = 0x52524e47,
	};

	class NodeConverterUtil
	{
	public:
		static frw::Value Convert(const ConverterParams& params, const MayaValueId mayaNodeId);
	private:
		static std::unique_ptr<BaseConverter> CreateConverter(const ConverterParams& params, const MayaValueId mayaNodeId);
	};

}