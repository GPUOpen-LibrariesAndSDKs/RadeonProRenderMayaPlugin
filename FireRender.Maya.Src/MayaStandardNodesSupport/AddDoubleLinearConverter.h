#pragma once

#include "BaseConverter.h"

namespace MayaStandardNodeConverters
{

	class AddDoubleLinearConverter : public BaseConverter
	{
	public:
		AddDoubleLinearConverter(const ConverterParams& params);
		virtual frw::Value Convert() const override;
	};

}