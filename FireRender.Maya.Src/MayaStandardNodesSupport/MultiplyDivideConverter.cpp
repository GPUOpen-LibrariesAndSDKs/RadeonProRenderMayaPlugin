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
#include "MultiplyDivideConverter.h"
#include "FireMaya.h"

MayaStandardNodeConverters::MultiplyDivideConverter::MultiplyDivideConverter(const ConverterParams& params) : BaseConverter(params)
{
}

frw::Value MayaStandardNodeConverters::MultiplyDivideConverter::Convert() const
{
	frw::Value input1 = m_params.scope.GetValueWithCheck(m_params.shaderNode, "input1");
	frw::Value input2 = m_params.scope.GetValueWithCheck(m_params.shaderNode, "input2");
	frw::Value operationValue = m_params.scope.GetValueWithCheck(m_params.shaderNode, "operation");

	Operation operation = static_cast<Operation>(operationValue.GetInt());
	switch (operation)
	{
		case Operation::Multiply: return m_params.scope.MaterialSystem().ValueMul(input1, input2);
		case Operation::Divide: return m_params.scope.MaterialSystem().ValueDiv(input1, input2);
		case Operation::Power: return m_params.scope.MaterialSystem().ValuePow(input1, input2);
		case Operation::None: return input1;
		default: assert(false);
	}

	return input1;
}
