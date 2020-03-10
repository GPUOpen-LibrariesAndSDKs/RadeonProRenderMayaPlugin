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
#include "BlendTwoAttrConverter.h"
#include "FireMaya.h"

MayaStandardNodeConverters::BlendTwoAttrConverter::BlendTwoAttrConverter(const ConverterParams& params) : BaseConverter(params)
{
}

frw::Value MayaStandardNodeConverters::BlendTwoAttrConverter::Convert() const
{
	frw::Value attributesBlender = m_params.scope.GetValueWithCheck(m_params.shaderNode, "attributesBlender");
	int currentValueIndex = m_params.scope.GetValueWithCheck(m_params.shaderNode, "current").GetInt();
	int nextValueIndex = currentValueIndex + 1;
	MPlug inputPlug = m_params.shaderNode.findPlug("input", false);
	unsigned numElements = inputPlug.numElements();

	frw::Value currentValue{ 1.0f, 1.0f, 1.0f };
	frw::Value nextValue{ 1.0f, 1.0f, 1.0f };

	// This node blends input[currentValueIndex] with input[currentValueIndex + 1]
	// If {currentValueIndex} is out of bounds, default value for {currentValue} is taken
	// If {nextValueIndex} is out of bounds, default value for {nextValue} is taken

	if (IsInBounds(currentValueIndex, 0, numElements))
	{
		MPlug firstValuePlug = inputPlug.elementByPhysicalIndex(currentValueIndex);
		currentValue = m_params.scope.GetValue(firstValuePlug);
	}

	if (IsInBounds(nextValueIndex, 0, numElements))
	{
		MPlug nextValuePlug = inputPlug.elementByPhysicalIndex(nextValueIndex);
		nextValue = m_params.scope.GetValue(nextValuePlug);
	}

	return (1 - attributesBlender) * currentValue + attributesBlender * nextValue;
}

bool MayaStandardNodeConverters::BlendTwoAttrConverter::IsInBounds(unsigned value, unsigned min, unsigned max) const
{
	return value >= min && value < max;
}
