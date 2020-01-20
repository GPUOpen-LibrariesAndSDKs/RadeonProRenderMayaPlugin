#pragma once

#include "BaseConverter.h"

namespace MayaStandardNodeConverters
{

	class BlendTwoAttrConverter : public BaseConverter
	{
	public:
		BlendTwoAttrConverter(const ConverterParams& params);
		virtual frw::Value Convert() const override;

	private:
		bool IsInBounds(unsigned value, unsigned min, unsigned max) const;
	};

}
