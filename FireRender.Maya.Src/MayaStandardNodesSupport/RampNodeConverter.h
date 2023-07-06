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

#include "BaseConverter.h"
#include <functional>

enum RampUVType
{
	// supported by RPR
	VRamp = 1 << 0,
	URamp = 1 << 1,
	DiagonalRamp = 1 << 2,
	CircularRamp = 1 << 4,

	// not supported by RPR
	RadialRamp = 1 << 3,
	BoxRamp = 1 << 5,
	UVRamp = 1 << 6,
	FourCornersRamp = 1 << 7,
	TartanRamp = 1 << 8,
};

frw::ArithmeticNode ApplyUVType(const FireMaya::Scope& scope, frw::ArithmeticNode& source, int uvIntType);
frw::ArithmeticNode GetRampNodeLookup(const FireMaya::Scope& scope, RampUVType rampType);

namespace MayaStandardNodeConverters
{

	class RampNodeConverter : public BaseConverter
	{
	public:
		RampNodeConverter(const ConverterParams& params);
		virtual frw::Value Convert() const override;
	};

}