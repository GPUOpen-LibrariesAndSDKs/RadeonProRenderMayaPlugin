#include "BlendColorsConverter.h"
#include "FireMaya.h"

MayaStandardNodeConverters::BlendColorsConverter::BlendColorsConverter(const ConverterParams& params) : BaseConverter(params)
{
}

frw::Value MayaStandardNodeConverters::BlendColorsConverter::Convert() const
{
	auto color1 = m_params.scope.GetValueWithCheck(m_params.shaderNode, "color1");
	auto color2 = m_params.scope.GetValueWithCheck(m_params.shaderNode, "color2");
	auto blender = m_params.scope.GetValueWithCheck(m_params.shaderNode, "blender");

	return m_params.scope.MaterialSystem().ValueBlend(color1, color2, blender);
}
