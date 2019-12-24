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
