#pragma once

#include "BaseConverter.h"

namespace MayaStandardNodeConverters
{

	class NoiseConverter : public BaseConverter
	{
	public:
		NoiseConverter(const ConverterParams& params);
		virtual frw::Value Convert() const override;
	};

}