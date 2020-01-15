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
