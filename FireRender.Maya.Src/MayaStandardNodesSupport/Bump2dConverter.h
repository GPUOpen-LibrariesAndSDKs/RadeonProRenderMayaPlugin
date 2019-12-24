#pragma once

#include "BaseConverter.h"

namespace MayaStandardNodeConverters
{

	class Bump2dConverter : public BaseConverter
	{

		enum class UseAs
		{
			Bump = 0,
			TangentSpaceNormals,
			ObjectSpaceNormals
		};

	public:
		Bump2dConverter(const ConverterParams& params);
		virtual frw::Value Convert() const override;

	private:
		frw::Value GetBumpValue(const frw::Value& strength) const;
		frw::Value GetNormalValue(const frw::Value& strength) const;
	};

}
