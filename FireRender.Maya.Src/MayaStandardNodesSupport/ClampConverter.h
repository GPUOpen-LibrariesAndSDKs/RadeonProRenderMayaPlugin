#pragma once

#include "BaseConverter.h"

namespace MayaStandardNodeConverters
{
	class ClampConverter : public BaseConverter
	{
	public:
		ClampConverter(const ConverterParams& params);
		virtual frw::Value Convert() const override;
	};

}
