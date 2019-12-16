#include "PlusMinusAverageConverter.h"
#include "FireMaya.h"
#include "maya/MString.h"

MayaStandardNodeConverters::PlusMinusAverageConverter::PlusMinusAverageConverter(const ConverterParams& params) : BaseConverter(params)
{
}

frw::Value MayaStandardNodeConverters::PlusMinusAverageConverter::Convert() const
{
	enum
	{
		OpNone = 0,
		OpSum,
		OpSubtract,
		OpAverage,
	};

	const char* name = "input1D";
	if (m_params.outPlugName.indexW(MString("o2")) == 0)
		name = "input2D";
	if (m_params.outPlugName.indexW(MString("o3")) == 0)
		name = "input3D";

	std::vector<frw::Value> arr;
	GetArrayOfValues(m_params.shaderNode, name, arr);
	if (arr.size() == 0)
		return frw::Value(0.0);

	auto operation = m_params.scope.GetValueWithCheck(m_params.shaderNode, "operation");
	int op = operation.GetInt();
	frw::Value res;

	switch (op)
	{
	case OpNone:		res = arr[0]; break;
	case OpSum:			res = CalcElements(arr); break;
	case OpSubtract:	res = CalcElements(arr, true); break;
	case OpAverage:		res = m_params.scope.MaterialSystem().ValueDiv(CalcElements(arr), frw::Value((double)arr.size())); break;
	}

	return res;
}

void MayaStandardNodeConverters::PlusMinusAverageConverter::GetArrayOfValues(const MFnDependencyNode& node, const char* plugName, std::vector<frw::Value>& arr) const
{
	MStatus status;
	MPlug plug = node.findPlug(plugName, &status);
	assert(status == MStatus::kSuccess);

	if (plug.isNull())
		return;

	for (unsigned int i = 0; i < plug.numElements(); i++)
	{
		MPlug elementPlug = plug[i];
		frw::Value val = m_params.scope.GetValue(elementPlug);
		arr.push_back(val);
	}
}

frw::Value MayaStandardNodeConverters::PlusMinusAverageConverter::CalcElements(const std::vector<frw::Value>& arr, bool substract) const
{
	if (arr.size() == 0)
		return frw::Value(0.0);

	frw::Value res = arr[0];

	frw::MaterialSystem materialSystem = m_params.scope.MaterialSystem();

	if (substract)
	{
		for (size_t i = 1; i < arr.size(); i++)
			res = materialSystem.ValueSub(res, arr[i]);
	}
	else
	{
		for (size_t i = 1; i < arr.size(); i++)
			res = materialSystem.ValueAdd(res, arr[i]);
	}

	return res;
}
