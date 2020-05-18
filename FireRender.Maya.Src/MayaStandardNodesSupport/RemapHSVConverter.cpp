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

#include <maya/MRampAttribute.h>

#include "RemapHSVConverter.h"
#include "FireMaya.h"

MayaStandardNodeConverters::RemapHSVConverter::RemapHSVConverter(const ConverterParams& params) : BaseConverter(params)
{
}

frw::Value MayaStandardNodeConverters::RemapHSVConverter::GetSamplerNodeForForPlugName(const MString& plugName, frw::Value valueInput) const
{
	MPlug plug = m_params.shaderNode.findPlug(plugName, false);

	if (plug.isNull())
	{
		assert(!"RemapHSV parse. Error: could not get the plug");
		return frw::Value();
	}

	const unsigned int bufferSize = 256; // same as in Blender

	// create buffer desc
	rpr_buffer_desc bufferDesc;
	bufferDesc.nb_element = bufferSize;
	bufferDesc.element_type = RPR_BUFFER_ELEMENT_TYPE_FLOAT32;
	bufferDesc.element_channel_size = 3;

	std::vector<float> arrData(bufferDesc.element_channel_size * bufferSize);

	MRampAttribute rampAttr(plug);

	for (size_t index = 0; index < bufferSize; ++index)
	{
		float val = 0.0f;
		rampAttr.getValueAtPosition((float)index / (bufferSize - 1), val);

		for (unsigned int j = 0; j < bufferDesc.element_channel_size; ++j)
		{
			arrData[bufferDesc.element_channel_size * index + j] = val;
		}
	}

	// create buffer
	frw::DataBuffer dataBuffer(m_params.scope.Context(), bufferDesc, arrData.data());

	frw::Value arithmeticValue = valueInput * (float)bufferSize;

	// create buffer node
	frw::BufferNode bufferNode(m_params.scope.MaterialSystem());
	bufferNode.SetBuffer(dataBuffer);
	bufferNode.SetUV(arithmeticValue);

	return bufferNode;
}

frw::Value MayaStandardNodeConverters::RemapHSVConverter::Convert() const
{
	frw::RGBToHSVNode rgbToHSVNode(m_params.scope.MaterialSystem());

	frw::Value rgbInput = m_params.scope.GetValue(m_params.shaderNode.findPlug("color", false));
	rgbToHSVNode.SetInputColor(rgbInput);

	frw::Value hsvInput = rgbToHSVNode;

	frw::Value hueOutput = GetSamplerNodeForForPlugName("hue", hsvInput.SelectX());
	frw::Value saturationOutput = GetSamplerNodeForForPlugName("saturation", hsvInput.SelectY());
	frw::Value valueOutput = GetSamplerNodeForForPlugName("value", hsvInput.SelectZ());
	frw::Value hsvRemapped = m_params.scope.MaterialSystem().ValueCombine(hueOutput, saturationOutput, valueOutput);

	frw::HSVToRGBNode hsvToRGBNode(m_params.scope.MaterialSystem());

	hsvToRGBNode.SetInputColor(hsvRemapped);

	return hsvToRGBNode;
}
