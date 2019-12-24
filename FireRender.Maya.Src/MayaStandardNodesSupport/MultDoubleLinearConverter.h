#pragma once

#include "BaseConverter.h"

namespace MayaStandardNodeConverters
{

	class MultDoubleLinearConverter : public BaseConverter
	{
	public:
		MultDoubleLinearConverter(const ConverterParams& params);
		virtual frw::Value Convert() const override;
	};

}