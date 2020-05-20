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

#include "RemapValueConverter.h"
#include "FireMaya.h"
#include "NodeProcessingUtils.h"

namespace MayaStandardNodeConverters
{

RemapValueConverter::RemapValueConverter(const ConverterParams& params) : BaseConverter(params)
{
}

frw::Value RemapValueConverter::Convert() const
{
	if (m_params.outPlugName == "outValue" || m_params.outPlugName == "ov")
	{
		frw::Value input = m_params.scope.GetValueWithCheck(m_params.shaderNode, "inputValue");

		frw::Value inputMin = m_params.scope.GetValueWithCheck(m_params.shaderNode, "inputMin");
		frw::Value inputMax = m_params.scope.GetValueWithCheck(m_params.shaderNode, "inputMax");
		frw::Value outputMin = m_params.scope.GetValueWithCheck(m_params.shaderNode, "outputMin");
		frw::Value outputMax = m_params.scope.GetValueWithCheck(m_params.shaderNode, "outputMax");

		return GetSamplerNodeForValue(m_params, "value", input, inputMin, inputMax, outputMin, outputMax);
	}
	else 
	{
		// _TODO: outColor plug, haven't implemented (will be in case of necessity)
		return frw::Value(0.0);
	}
}

} // End of hte namespace
