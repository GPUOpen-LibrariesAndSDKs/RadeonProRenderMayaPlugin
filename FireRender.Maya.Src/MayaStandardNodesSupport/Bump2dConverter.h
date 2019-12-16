#pragma once

#include "BaseConverter.h"

namespace MayaStandardNodeConverters
{

	class Bump2dConverter : public BaseConverter
	{
	public:
		Bump2dConverter(const ConverterParams& params);
		virtual frw::Value Convert() const override;
	};

}