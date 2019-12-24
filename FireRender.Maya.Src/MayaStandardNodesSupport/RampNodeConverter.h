#pragma once

#include "BaseConverter.h"

namespace MayaStandardNodeConverters
{

	class RampNodeConverter : public BaseConverter
	{
	public:
		RampNodeConverter(const ConverterParams& params);
		virtual frw::Value Convert() const override;
	};

}