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
#include "ClampConverter.h"
#include "FireMaya.h"

namespace MayaStandardNodeConverters
{
	ClampConverter::ClampConverter(const ConverterParams& params) : BaseConverter(params)
	{
	}

	frw::Value ClampConverter::Convert() const
	{
		frw::Value input = m_params.scope.GetValueWithCheck(m_params.shaderNode, "input");
		frw::Value min = m_params.scope.GetValueWithCheck(m_params.shaderNode, "min");
		frw::Value max = m_params.scope.GetValueWithCheck(m_params.shaderNode, "max");

		return m_params.scope.MaterialSystem().ValueClamp(input, min, max);
	}
}
