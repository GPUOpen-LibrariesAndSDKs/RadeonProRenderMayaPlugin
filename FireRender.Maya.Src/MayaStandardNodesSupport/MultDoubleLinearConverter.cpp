#include "MultDoubleLinearConverter.h"
#include "FireMaya.h"

MayaStandardNodeConverters::MultDoubleLinearConverter::MultDoubleLinearConverter(const ConverterParams& params) : BaseConverter(params)
{
}

frw::Value MayaStandardNodeConverters::MultDoubleLinearConverter::Convert() const
{
	auto input1 = m_params.scope.GetValueWithCheck(m_params.shaderNode, "input1");
	auto input2 = m_params.scope.GetValueWithCheck(m_params.shaderNode, "input2");
	return m_params.scope.MaterialSystem().ValueMul(input1, input2);
}
