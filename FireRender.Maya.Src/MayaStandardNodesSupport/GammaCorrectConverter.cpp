#include "GammaCorrectConverter.h"
#include "FireMaya.h"

MayaStandardNodeConverters::GammaCorrectConverter::GammaCorrectConverter(const ConverterParams& params) : BaseConverter(params)
{
}

frw::Value MayaStandardNodeConverters::GammaCorrectConverter::Convert() const
{
	frw::Value inputValue = m_params.scope.GetValueWithCheck(m_params.shaderNode, "value");
	frw::Value gammaValue = m_params.scope.GetValueWithCheck(m_params.shaderNode, "gamma");
	return m_params.scope.MaterialSystem().ValuePow(inputValue, 1.0f / gammaValue);
}
