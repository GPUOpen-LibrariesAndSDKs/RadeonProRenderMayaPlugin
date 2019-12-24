#include "ReverseMapConverter.h"
#include "FireMaya.h"
#include "maya/MPlugArray.h"

MayaStandardNodeConverters::ReverseMapConverter::ReverseMapConverter(const ConverterParams& params) : BaseConverter(params)
{
}

frw::Value MayaStandardNodeConverters::ReverseMapConverter::Convert() const
{
	MPlugArray nodes;
	m_params.shaderNode.getConnections(nodes);

	for (unsigned int idx = 0; idx < nodes.length(); ++idx)
	{
		const MPlug& connection = nodes[idx];
		const MString partialName = connection.partialName();

		if (partialName == "i") // name of "input" of the node
		{
			frw::Value mapValue = m_params.scope.GetValue(connection);
			frw::Value invertedMapValue = frw::Value(1) - mapValue;
			return invertedMapValue;
		}
	}

	return nullptr;
}
