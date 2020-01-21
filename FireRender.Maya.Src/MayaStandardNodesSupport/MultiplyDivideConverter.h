#pragma once

#include "BaseConverter.h"

namespace MayaStandardNodeConverters
{

	class MultiplyDivideConverter : public BaseConverter
	{

		enum class Operation
		{
			None = 0,
			Multiply,
			Divide,
			Power
		};

	public:
		MultiplyDivideConverter(const ConverterParams& params);
		virtual frw::Value Convert() const override;
	};

}