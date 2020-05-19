#include "FireMaya.h"
#include "BaseConverter.h"

#include "NodeProcessingUtils.h" 
#include <maya/MRampAttribute.h>

namespace MayaStandardNodeConverters
{ 

frw::Value GetSamplerNodeForValue(
	const ConverterParams& params,
	const MString& plugName,
	frw::Value valueInput,
	frw::Value inputMin,
	frw::Value inputMax,
	frw::Value outputMin,
	frw::Value outputMax)
{
	MPlug plug = params.shaderNode.findPlug(plugName, false);

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
	frw::DataBuffer dataBuffer(params.scope.Context(), bufferDesc, arrData.data());

	frw::Value normalizedValue = (valueInput - inputMin) / (inputMax - inputMin);

	frw::Value arithmeticValue = normalizedValue * (float)bufferSize;

	// create buffer node
	frw::BufferNode bufferNode(params.scope.MaterialSystem());
	bufferNode.SetBuffer(dataBuffer);
	bufferNode.SetUV(arithmeticValue);

	return params.scope.MaterialSystem().ValueClamp(bufferNode, outputMin, outputMax);
}

} // End of namespace MayaStandardNodeConverters