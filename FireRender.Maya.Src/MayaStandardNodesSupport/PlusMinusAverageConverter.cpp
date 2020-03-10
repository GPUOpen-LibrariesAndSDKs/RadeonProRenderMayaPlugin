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
#include "PlusMinusAverageConverter.h"
#include "FireMaya.h"
#include "maya/MString.h"
#include <numeric>

MayaStandardNodeConverters::PlusMinusAverageConverter::PlusMinusAverageConverter(const ConverterParams& params) : BaseConverter(params)
{
}

frw::Value MayaStandardNodeConverters::PlusMinusAverageConverter::Convert() const
{
	std::string name = "input1D";
	if (m_params.outPlugName.indexW(MString("o2")) == 0)
	{
		name = "input2D";
	}
	else if (m_params.outPlugName.indexW(MString("o3")) == 0)
	{
		name = "input3D";
	}

	std::vector<frw::Value> values = GetArrayOfValues(m_params.shaderNode, name);
	if (values.empty())
	{
		return frw::Value();
	}
	else if (values.size() == 1)
	{
		return values.at(0);
	}

	frw::Value operationValue = m_params.scope.GetValueWithCheck(m_params.shaderNode, "operation");
	Operation operation = static_cast<Operation>(operationValue.GetInt());

	switch (operation)
	{
		case Operation::None: return values.at(0);
		case Operation::Sum: return ValuesSum(values);
		case Operation::Subtract: return ValuesSubtract(values);
		case Operation::Average: return ValuesAverage(values);
		default: assert(false);
	}

	return frw::Value();
}

std::vector<frw::Value> MayaStandardNodeConverters::PlusMinusAverageConverter::GetArrayOfValues(const MFnDependencyNode& node, const std::string& plugName) const
{
	MStatus status;
	MPlug plug = node.findPlug(plugName.c_str(), &status);
	assert(status == MStatus::kSuccess);

	std::vector<frw::Value> result;

	if (plug.isNull())
	{
		return result;
	}

	result.reserve(plug.numElements());
	for (unsigned int i = 0; i < plug.numElements(); i++)
	{
		MPlug elementPlug = plug[i];
		frw::Value elementValue = m_params.scope.GetValue(elementPlug);
		result.push_back(elementValue);
	}

	return result;
}

frw::Value MayaStandardNodeConverters::PlusMinusAverageConverter::ValuesSum(const std::vector<frw::Value>& values) const
{
	assert(values.size() > 1);

	frw::MaterialSystem materialSystem = m_params.scope.MaterialSystem();
	return std::accumulate(values.begin() + 1, values.end(), values.at(0), [&materialSystem](const frw::Value& accumulator, const frw::Value& nextValue)
		{
			return materialSystem.ValueAdd(accumulator, nextValue);
		});
}

frw::Value MayaStandardNodeConverters::PlusMinusAverageConverter::ValuesSubtract(const std::vector<frw::Value>& values) const
{
	assert(values.size() > 1);

	frw::MaterialSystem materialSystem = m_params.scope.MaterialSystem();
	return std::accumulate(values.begin() + 1, values.end(), values.at(0), [&materialSystem](const frw::Value& accumulator, const frw::Value& nextValue)
		{
			return materialSystem.ValueSub(accumulator, nextValue);
		});
}

frw::Value MayaStandardNodeConverters::PlusMinusAverageConverter::ValuesAverage(const std::vector<frw::Value>& values) const
{
	assert(values.size() > 1);

	return m_params.scope.MaterialSystem().ValueDiv(
		ValuesSum(values),
		frw::Value((double)values.size())
	);
}
