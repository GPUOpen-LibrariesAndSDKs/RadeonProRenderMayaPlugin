#pragma once

#include "BaseConverter.h"

namespace MayaStandardNodeConverters
{

	class PlusMinusAverageConverter : public BaseConverter
	{

		enum class Operation
		{
			None = 0,
			Sum,
			Subtract,
			Average,
		};

	public:
		PlusMinusAverageConverter(const ConverterParams& params);
		virtual frw::Value Convert() const override;
	private:
		std::vector<frw::Value> GetArrayOfValues(const MFnDependencyNode& node, const std::string& plugName) const;

		frw::Value ValuesSum(const std::vector<frw::Value>& values) const;
		frw::Value ValuesSubtract(const std::vector<frw::Value>& values) const;
		frw::Value ValuesAverage(const std::vector<frw::Value>& values) const;
	};

}