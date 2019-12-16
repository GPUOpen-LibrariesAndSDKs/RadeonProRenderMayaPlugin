#pragma once

#include "BaseConverter.h"

namespace MayaStandardNodeConverters
{

	class Place2dTextureConverter : public BaseConverter
	{
	public:
		Place2dTextureConverter(const ConverterParams& params);
		virtual frw::Value Convert() const override;
	};

}