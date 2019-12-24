#pragma once

#include "BaseConverter.h"

namespace MayaStandardNodeConverters
{

	class GammaCorrectConverter : public BaseConverter
	{
	public:
		GammaCorrectConverter(const ConverterParams& params);
		virtual frw::Value Convert() const override;
	};

}