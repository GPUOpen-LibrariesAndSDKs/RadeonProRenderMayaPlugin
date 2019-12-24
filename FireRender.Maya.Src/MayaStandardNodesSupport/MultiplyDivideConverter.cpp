#include "MultiplyDivideConverter.h"
#include "FireMaya.h"

MayaStandardNodeConverters::MultiplyDivideConverter::MultiplyDivideConverter(const ConverterParams& params) : BaseConverter(params)
{
}

frw::Value MayaStandardNodeConverters::MultiplyDivideConverter::Convert() const
{
	enum
	{
		OpNone = 0,
		OpMul,
		OpDiv,
		OpPow
	};

	auto input1 = m_params.scope.GetValueWithCheck(m_params.shaderNode, "input1");
	auto input2 = m_params.scope.GetValueWithCheck(m_params.shaderNode, "input2");
	auto operation = m_params.scope.GetValueWithCheck(m_params.shaderNode, "operation");
	int op = operation.GetInt();
	switch (op)
	{
	case OpMul: return m_params.scope.MaterialSystem().ValueMul(input1, input2);
	case OpDiv: return m_params.scope.MaterialSystem().ValueDiv(input1, input2);
	case OpPow: return m_params.scope.MaterialSystem().ValuePow(input1, input2);
	}

	return input1;
}
