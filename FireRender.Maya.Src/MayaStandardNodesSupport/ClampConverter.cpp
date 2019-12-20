#include "ClampConverter.h"
#include "FireMaya.h"

namespace MayaStandardNodeConverters
{
	ClampConverter::ClampConverter(const ConverterParams& params) : BaseConverter(params)
	{
	}

	frw::Value ClampConverter::Convert() const
	{
		frw::Value input = m_params.scope.GetValueWithCheck(m_params.shaderNode, "input");
		frw::Value min = m_params.scope.GetValueWithCheck(m_params.shaderNode, "min");
		frw::Value max = m_params.scope.GetValueWithCheck(m_params.shaderNode, "max");

		return m_params.scope.MaterialSystem().ValueClamp(input, min, max);
	}
}
