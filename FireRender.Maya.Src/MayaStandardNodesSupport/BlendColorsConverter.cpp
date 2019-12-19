#include "BlendColorsConverter.h"
#include "FireMaya.h"

MayaStandardNodeConverters::BlendColorsConverter::BlendColorsConverter(const ConverterParams& params) : BaseConverter(params)
{
}

frw::Value MayaStandardNodeConverters::BlendColorsConverter::Convert() const
{
	frw::Value color1 = m_params.scope.GetValueWithCheck(m_params.shaderNode, "color1");
	frw::Value color2 = m_params.scope.GetValueWithCheck(m_params.shaderNode, "color2");
	frw::Value blender = m_params.scope.GetValueWithCheck(m_params.shaderNode, "blender");

	// Reverse order to match maya hardware render behavior
	return m_params.scope.MaterialSystem().ValueBlend(color2, color1, blender);
}
