#pragma once

#include "BaseConverter.h"

namespace MayaStandardNodeConverters
{

	class MultiplyDivideConverter : public BaseConverter
	{
	public:
		MultiplyDivideConverter(const ConverterParams& params);
		virtual frw::Value Convert() const override;
	};

}