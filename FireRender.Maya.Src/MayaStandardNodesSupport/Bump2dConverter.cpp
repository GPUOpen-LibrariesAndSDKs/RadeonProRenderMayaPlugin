#include "Bump2dConverter.h"
#include "FireMaya.h"

MayaStandardNodeConverters::Bump2dConverter::Bump2dConverter(const ConverterParams& params) : BaseConverter(params)
{
}

frw::Value MayaStandardNodeConverters::Bump2dConverter::Convert() const
{
	if (auto bumpValue = m_params.scope.GetConnectedValue(m_params.shaderNode.findPlug("bumpValue")))
	{
		// TODO: need to find a way to turn generic input color data
		// into bump/normal format
	}

	return nullptr;
}
