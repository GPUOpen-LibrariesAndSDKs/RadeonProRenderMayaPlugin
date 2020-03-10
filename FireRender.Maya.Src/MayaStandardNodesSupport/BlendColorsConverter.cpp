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
#include "BlendColorsConverter.h"
#include "FireMaya.h"

MayaStandardNodeConverters::BlendColorsConverter::BlendColorsConverter(const ConverterParams& params) : BaseConverter(params)
{
}

frw::Value MayaStandardNodeConverters::BlendColorsConverter::Convert() const
{
	frw::Value color1 = m_params.scope.GetValueWithCheck(m_params.shaderNode, "color1");
	frw::Value color2 = m_params.scope.GetValueWithCheck(m_params.shaderNode, "color2");
	frw::Value blender = m_params.scope.GetValueWithCheck(m_params.shaderNode, "blender");

	// Reverse order to match maya hardware render behavior
	return m_params.scope.MaterialSystem().ValueBlend(color2, color1, blender);
}
