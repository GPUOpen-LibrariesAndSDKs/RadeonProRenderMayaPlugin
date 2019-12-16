#pragma once

#include "BaseConverter.h"

namespace MayaStandardNodeConverters
{

	class RemapHSVConverter : public BaseConverter
	{
	public:
		RemapHSVConverter(const ConverterParams& params);
		virtual frw::Value Convert() const override;
	};

}