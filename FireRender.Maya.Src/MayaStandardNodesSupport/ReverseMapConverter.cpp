/**********************************************************************
Copyright 2020 Advanced Micro Devices, Inc
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
********************************************************************/
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
