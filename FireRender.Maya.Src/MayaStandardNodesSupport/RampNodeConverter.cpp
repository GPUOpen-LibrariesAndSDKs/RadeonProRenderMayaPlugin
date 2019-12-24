#include "RampNodeConverter.h"
#include "FireMaya.h"

MayaStandardNodeConverters::RampNodeConverter::RampNodeConverter(const ConverterParams& params) : BaseConverter(params)
{
}

frw::Value MayaStandardNodeConverters::RampNodeConverter::Convert() const
{
	return m_params.scope.createImageFromShaderNode(m_params.shaderNode.object(), MString("outColor"), 128, 128);
}
