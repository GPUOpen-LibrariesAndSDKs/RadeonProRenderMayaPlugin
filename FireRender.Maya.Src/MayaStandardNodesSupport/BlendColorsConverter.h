#pragma once

#include "BaseConverter.h"

namespace MayaStandardNodeConverters
{

	class BlendColorsConverter : public BaseConverter
	{
	public:
		BlendColorsConverter(const ConverterParams& params);
		virtual frw::Value Convert() const override;
	};

}