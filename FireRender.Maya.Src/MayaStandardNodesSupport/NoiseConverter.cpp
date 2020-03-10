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
#include "NoiseConverter.h"
#include "FireMaya.h"

MayaStandardNodeConverters::NoiseConverter::NoiseConverter(const ConverterParams& params) : BaseConverter(params)
{
}

frw::Value MayaStandardNodeConverters::NoiseConverter::Convert() const
{
	auto uv = m_params.scope.GetValueWithCheck(m_params.shaderNode, "uvCoord");

	auto threshold = m_params.scope.GetValueWithCheck(m_params.shaderNode, "threshold");
	auto amplitude = m_params.scope.GetValueWithCheck(m_params.shaderNode, "amplitude");
	auto ratio = m_params.scope.GetValueWithCheck(m_params.shaderNode, "ratio");
	auto frequencyRatio = m_params.scope.GetValueWithCheck(m_params.shaderNode, "frequencyRatio");
	auto depthMax = m_params.scope.GetValueWithCheck(m_params.shaderNode, "depthMax");
	auto time = m_params.scope.GetValueWithCheck(m_params.shaderNode, "time");
	auto frequency = m_params.scope.GetValueWithCheck(m_params.shaderNode, "frequency");
	// Implode - not implemented
	// Implode Center - not implemented

	frw::Value minScale(0.225f);

	frw::MaterialSystem materialSystem = m_params.scope.MaterialSystem();

	// calc uv1
	auto scaleUV1 = materialSystem.ValueMul(frequency, minScale);
	auto uv1 = materialSystem.ValueMul(uv, scaleUV1);

	// calc uv2
	auto freqSq = materialSystem.ValuePow(frequencyRatio, frw::Value(1.4f));
	auto freq = materialSystem.ValueMul(frequency, freqSq);
	auto scaleUV2 = materialSystem.ValueMul(freq, minScale);

	auto dephVal = materialSystem.ValueSub(depthMax, frw::Value(1.0));
	dephVal = materialSystem.ValueDiv(dephVal, frw::Value(2.0));
	scaleUV2 = materialSystem.ValueMul(scaleUV2, dephVal);

	auto uv2 = materialSystem.ValueMul(uv, scaleUV2);

	auto scaledTime = materialSystem.ValueMul(time, frw::Value(0.5));
	auto offset1 = materialSystem.ValueMul(scaleUV1, scaledTime);
	auto offset2 = materialSystem.ValueMul(scaleUV2, scaledTime);

	frw::ValueNode node1(materialSystem, frw::ValueTypeNoiseMap);
	uv1 = materialSystem.ValueAdd(uv1, offset1);
	node1.SetValue(RPR_MATERIAL_INPUT_UV, uv1);

	frw::ValueNode node2(materialSystem, frw::ValueTypeNoiseMap);
	uv2 = materialSystem.ValueSub(uv2, offset2);
	node2.SetValue(RPR_MATERIAL_INPUT_UV, uv2);

	auto mulNoise = materialSystem.ValueMul(node1, node2);
	auto v = materialSystem.ValueBlend(node1, mulNoise, ratio);


	auto amplScaled = materialSystem.ValueMul(amplitude, frw::Value(0.5));
	auto th = materialSystem.ValueAdd(threshold, frw::Value(0.5));

	auto min = materialSystem.ValueSub(th, amplScaled);
	auto max = materialSystem.ValueAdd(th, amplScaled);

	frw::Value thresholdInv = materialSystem.ValueSub(frw::Value(1.0), threshold);
	frw::Value v1 = materialSystem.ValueMin(v, thresholdInv);

	return materialSystem.ValueBlend(min, max, v1);
}
