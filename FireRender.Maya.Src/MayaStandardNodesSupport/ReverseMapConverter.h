#pragma once

#include "BaseConverter.h"

namespace MayaStandardNodeConverters
{

	class ReverseMapConverter : public BaseConverter
	{
	public:
		ReverseMapConverter(const ConverterParams& params);
		virtual frw::Value Convert() const override;
	};

}