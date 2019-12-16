#pragma once

#include "BaseConverter.h"

namespace MayaStandardNodeConverters
{

	class PlusMinusAverageConverter : public BaseConverter
	{
	public:
		PlusMinusAverageConverter(const ConverterParams& params);
		virtual frw::Value Convert() const override;
	private:
		void GetArrayOfValues(const MFnDependencyNode& node, const char* plugName, std::vector<frw::Value>& arr) const;
		frw::Value CalcElements(const std::vector<frw::Value>& arr, bool substract = false) const;
	};

}