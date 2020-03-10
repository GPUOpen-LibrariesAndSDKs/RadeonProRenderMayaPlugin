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

namespace MayaStandardNodeConverters
{

	class PlusMinusAverageConverter : public BaseConverter
	{

		enum class Operation
		{
			None = 0,
			Sum,
			Subtract,
			Average,
		};

	public:
		PlusMinusAverageConverter(const ConverterParams& params);
		virtual frw::Value Convert() const override;
	private:
		std::vector<frw::Value> GetArrayOfValues(const MFnDependencyNode& node, const std::string& plugName) const;

		frw::Value ValuesSum(const std::vector<frw::Value>& values) const;
		frw::Value ValuesSubtract(const std::vector<frw::Value>& values) const;
		frw::Value ValuesAverage(const std::vector<frw::Value>& values) const;
	};

}