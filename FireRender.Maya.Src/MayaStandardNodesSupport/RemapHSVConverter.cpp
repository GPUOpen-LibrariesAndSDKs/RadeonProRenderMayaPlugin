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

#include "FireMaya.h"
#include "RemapHSVConverter.h"
#include "NodeProcessingUtils.h"

MayaStandardNodeConverters::RemapHSVConverter::RemapHSVConverter(const ConverterParams& params) : BaseConverter(params)
{
}

frw::Value MayaStandardNodeConverters::RemapHSVConverter::Convert() const
{
	frw::RGBToHSVNode rgbToHSVNode(m_params.scope.MaterialSystem());

	frw::Value rgbInput = m_params.scope.GetValue(m_params.shaderNode.findPlug("color", false));
	rgbToHSVNode.SetInputColor(rgbInput);

	frw::Value hsvInput = rgbToHSVNode;

	frw::Value hueOutput = GetSamplerNodeForValue(m_params, "hue", hsvInput.SelectX());
	frw::Value saturationOutput = GetSamplerNodeForValue(m_params, "saturation", hsvInput.SelectY());
	frw::Value valueOutput = GetSamplerNodeForValue(m_params, "value", hsvInput.SelectZ());
	frw::Value hsvRemapped = m_params.scope.MaterialSystem().ValueCombine(hueOutput, saturationOutput, valueOutput);

	frw::HSVToRGBNode hsvToRGBNode(m_params.scope.MaterialSystem());

	hsvToRGBNode.SetInputColor(hsvRemapped);

	return hsvToRGBNode;
}
