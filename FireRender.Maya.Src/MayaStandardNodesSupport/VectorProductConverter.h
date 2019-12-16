#pragma once

#include "BaseConverter.h"

namespace MayaStandardNodeConverters
{

	class VectorProductConverter : public BaseConverter
	{
	public:
		VectorProductConverter(const ConverterParams& params);
		virtual frw::Value Convert() const override;
	};

}