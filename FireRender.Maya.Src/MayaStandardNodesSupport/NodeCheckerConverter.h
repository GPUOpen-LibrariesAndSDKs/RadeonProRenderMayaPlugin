#pragma once

#include "BaseConverter.h"

namespace MayaStandardNodeConverters
{

	class NodeCheckerConverter : public BaseConverter
	{
	public:
		NodeCheckerConverter(const ConverterParams& params);
		virtual frw::Value Convert() const override;
	};

}