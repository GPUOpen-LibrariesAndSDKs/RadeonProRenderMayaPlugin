#include "ContrastConverter.h"
#include "FireMaya.h"

MayaStandardNodeConverters::ContrastConverter::ContrastConverter(const ConverterParams& params) : BaseConverter(params)
{
}

frw::Value MayaStandardNodeConverters::ContrastConverter::Convert() const
{
	auto value = m_params.scope.GetValueWithCheck(m_params.shaderNode, "value");
	auto bias = m_params.scope.GetValueWithCheck(m_params.shaderNode, "bias");
	auto contrast = m_params.scope.GetValueWithCheck(m_params.shaderNode, "contrast");

	// renderPassMode

	auto x = CalcContrast(value.GetX(), bias.GetX(), contrast.GetX());
	auto y = CalcContrast(value.GetY(), bias.GetY(), contrast.GetY());
	auto z = CalcContrast(value.GetZ(), bias.GetZ(), contrast.GetZ());

	return frw::Value(x, y, z);
}

double MayaStandardNodeConverters::ContrastConverter::CalcContrast(double value, double bias, double contrast) const
{
	double res = 0;
	double factor = 1.0;
	bool inv = value > bias;

	if (inv)
	{
		factor = bias / 0.25 * 0.5;
		value = 1 - value;
		bias = 1 - bias;
	}

	res = pow(bias * pow((value / bias), contrast * factor), log(0.5) / log(bias));
	return inv ? 1 - res : res;
}