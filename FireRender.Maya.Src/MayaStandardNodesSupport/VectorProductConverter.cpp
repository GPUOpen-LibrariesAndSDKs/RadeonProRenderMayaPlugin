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
#include "VectorProductConverter.h"
#include "FireMaya.h"

MayaStandardNodeConverters::VectorProductConverter::VectorProductConverter(const ConverterParams& params) : BaseConverter(params)
{
}

frw::Value MayaStandardNodeConverters::VectorProductConverter::Convert() const
{
	enum
	{
		OpNone = 0,
		OpDotProduct,
		OpCrossProduct,
		OpVectorMatrixProduct, // Outvector = Input x Matrix
		OpPointMatrixProduct, // Output = Input x Matrix
	};

	auto input1 = m_params.scope.GetValueWithCheck(m_params.shaderNode, "input1");
	auto input2 = m_params.scope.GetValueWithCheck(m_params.shaderNode, "input2");
	auto matrix = m_params.scope.GetValueWithCheck(m_params.shaderNode, "matrix");
	auto operation = m_params.scope.GetValueWithCheck(m_params.shaderNode, "operation");
	int op = operation.GetInt();

	auto res = input1;
	switch (op)
	{
	case OpDotProduct: res = m_params.scope.MaterialSystem().ValueDot(input1, input2); break;
	case OpCrossProduct: res = m_params.scope.MaterialSystem().ValueCross(input1, input2); break;
	}

	auto normalizeOutput = m_params.scope.GetValueWithCheck(m_params.shaderNode, "normalizeOutput");
	if (normalizeOutput.GetBool())
		return m_params.scope.MaterialSystem().ValueNormalize(res);

	return res;
}
