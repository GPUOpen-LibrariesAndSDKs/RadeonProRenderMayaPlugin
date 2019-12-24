#include "AddDoubleLinearConverter.h"
#include "FireMaya.h"

MayaStandardNodeConverters::AddDoubleLinearConverter::AddDoubleLinearConverter(const ConverterParams& params) : BaseConverter(params)
{
}

frw::Value MayaStandardNodeConverters::AddDoubleLinearConverter::Convert() const
{
	auto input1 = m_params.scope.GetValueWithCheck(m_params.shaderNode, "input1");
	auto input2 = m_params.scope.GetValueWithCheck(m_params.shaderNode, "input2");
	return m_params.scope.MaterialSystem().ValueAdd(input1, input2);
}
