#pragma once

#include "BaseConverter.h"

namespace MayaStandardNodeConverters
{

	class FileNodeConverter : public BaseConverter
	{
	public:
		FileNodeConverter(const ConverterParams& params);
		virtual frw::Value Convert() const override;
	private:
		float ColorSpace2Gamma(const MString& colorSpace) const;
	};

}