#include "NodeCheckerConverter.h"
#include "FireMaya.h"

MayaStandardNodeConverters::NodeCheckerConverter::NodeCheckerConverter(const ConverterParams& params) : BaseConverter(params)
{
}

frw::Value MayaStandardNodeConverters::NodeCheckerConverter::Convert() const
{
	frw::ValueNode map(m_params.scope.MaterialSystem(), frw::ValueTypeCheckerMap);
	auto uv = (m_params.scope.GetConnectedValue(m_params.shaderNode.findPlug("uvCoord")) | m_params.scope.MaterialSystem().ValueLookupUV(0));
	map.SetValue("uv", (uv * .25) + 128.);	// <- offset added because FR mirrors checker at origin

	frw::Value color1 = m_params.scope.GetValue(m_params.shaderNode.findPlug("color1"));
	frw::Value color2 = m_params.scope.GetValue(m_params.shaderNode.findPlug("color2"));
	frw::Value contrast = m_params.scope.GetValue(m_params.shaderNode.findPlug("contrast"));

	return m_params.scope.MaterialSystem().ValueBlend(color2, color1, map); // Reverse blend to match tile alignment
}
