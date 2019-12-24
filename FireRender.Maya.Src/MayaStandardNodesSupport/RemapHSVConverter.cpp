#include "RemapHSVConverter.h"
#include "FireMaya.h"

MayaStandardNodeConverters::RemapHSVConverter::RemapHSVConverter(const ConverterParams& params) : BaseConverter(params)
{
}

frw::Value MayaStandardNodeConverters::RemapHSVConverter::Convert() const
{
	return m_params.scope.createImageFromShaderNodeUsingFileNode(m_params.shaderNode.object(), MString("outColor"));
}
