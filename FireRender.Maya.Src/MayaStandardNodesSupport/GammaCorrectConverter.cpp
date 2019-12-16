#include "GammaCorrectConverter.h"
#include "FireMaya.h"

MayaStandardNodeConverters::GammaCorrectConverter::GammaCorrectConverter(const ConverterParams& params) : BaseConverter(params)
{
}

frw::Value MayaStandardNodeConverters::GammaCorrectConverter::Convert() const
{
	return m_params.scope.createImageFromShaderNodeUsingFileNode(m_params.shaderNode.object(), MString("outValue"));
}
