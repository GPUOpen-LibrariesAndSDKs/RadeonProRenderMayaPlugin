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
#include "NodeCheckerConverter.h"
#include "FireMaya.h"

MayaStandardNodeConverters::NodeCheckerConverter::NodeCheckerConverter(const ConverterParams& params) : BaseConverter(params)
{
}

frw::Value MayaStandardNodeConverters::NodeCheckerConverter::Convert() const
{
	frw::ValueNode map(m_params.scope.MaterialSystem(), frw::ValueTypeCheckerMap);
	auto uv = (m_params.scope.GetConnectedValue(m_params.shaderNode.findPlug("uvCoord")) | m_params.scope.MaterialSystem().ValueLookupUV(0));
	map.SetValue(RPR_MATERIAL_INPUT_UV, (uv * .25) + 128.);	// <- offset added because FR mirrors checker at origin

	frw::Value color1 = m_params.scope.GetValue(m_params.shaderNode.findPlug("color1"));
	frw::Value color2 = m_params.scope.GetValue(m_params.shaderNode.findPlug("color2"));
	frw::Value contrast = m_params.scope.GetValue(m_params.shaderNode.findPlug("contrast"));

	return m_params.scope.MaterialSystem().ValueBlend(color2, color1, map); // Reverse blend to match tile alignment
}
