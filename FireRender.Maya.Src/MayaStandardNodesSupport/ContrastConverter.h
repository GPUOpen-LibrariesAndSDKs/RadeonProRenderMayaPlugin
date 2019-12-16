#pragma once

#include "BaseConverter.h"

namespace MayaStandardNodeConverters
{

	class ContrastConverter : public BaseConverter
	{
	public:
		ContrastConverter(const ConverterParams& params);
		virtual frw::Value Convert() const override;
	private:
		double CalcContrast(double value, double bias, double contrast) const;
	};

}