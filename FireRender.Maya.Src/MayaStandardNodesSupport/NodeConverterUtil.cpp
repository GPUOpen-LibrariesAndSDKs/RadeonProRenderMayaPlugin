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
#include "NodeConverterUtil.h"
#include "FireMaya.h"

#include "AddDoubleLinearConverter.h"
#include "ContrastConverter.h"
#include "MultDoubleLinearConverter.h"
#include "MultiplyDivideConverter.h"
#include "PlusMinusAverageConverter.h"
#include "VectorProductConverter.h"
#include "BlendColorsConverter.h"
#include "NoiseConverter.h"
#include "Bump2dConverter.h"
#include "Place2dTextureConverter.h"
#include "FileNodeConverter.h"
#include "NodeCheckerConverter.h"
#include "ReverseMapConverter.h"
#include "FireRenderLayeredTextureUtils.h"
#include "RampNodeConverter.h"
#include "GammaCorrectConverter.h"
#include "RemapHSVConverter.h"
#include "BlendTwoAttrConverter.h"
#include "ClampConverter.h"
#include "ProjectionNodeConverter.h"
#include "RemapValueConverter.h"
#include "SetRangeConverter.h"

frw::Value MayaStandardNodeConverters::NodeConverterUtil::Convert(const ConverterParams& params, const MayaValueId mayaNodeId)
{
	std::unique_ptr<BaseConverter> converter = CreateConverter(params, mayaNodeId);

	if (converter != nullptr)
	{
		return converter->Convert();
	}

	// Try to detect what is connected to this node to obtain a texture of proper size
	if (FireMaya::TypeId::IsValidId(static_cast<int>(mayaNodeId)))
	{
		DebugPrint("Error: Unhandled RPRMaya NodeId: %X", mayaNodeId);
	}
	else if (FireMaya::TypeId::IsReservedId(static_cast<int>(mayaNodeId)))
	{
		DebugPrint("Error: Unrecognized RPRMaya NodeId: %X", static_cast<int>(mayaNodeId));
	}
	else
	{
		DebugPrint("Warning: Unhandled or Unknown RPRMaya Node: %X", static_cast<int>(mayaNodeId));
		// I don't think we need this: Dump(shaderNode);
	}
	return params.scope.createImageFromShaderNodeUsingFileNode(params.shaderNode.object(), params.outPlugName);
}

std::unique_ptr<MayaStandardNodeConverters::BaseConverter> MayaStandardNodeConverters::NodeConverterUtil::CreateConverter(const ConverterParams& params, const MayaValueId mayaNodeId)
{
	switch (mayaNodeId)
	{
		case MayaValueId::AddDoubleLinear:	return std::make_unique<AddDoubleLinearConverter>(params);
		case MayaValueId::BlendColors:		return std::make_unique<BlendColorsConverter>(params);
		case MayaValueId::Contrast:			return std::make_unique<ContrastConverter>(params);
		case MayaValueId::MultDoubleLinear:	return std::make_unique<MultDoubleLinearConverter>(params);
		case MayaValueId::MultiplyDivide:	return std::make_unique<MultiplyDivideConverter>(params);
		case MayaValueId::PlusMinusAverage:	return std::make_unique<PlusMinusAverageConverter>(params);
		case MayaValueId::VectorProduct:	return std::make_unique<VectorProductConverter>(params);
		case MayaValueId::Noise:			return std::make_unique<NoiseConverter>(params);
		case MayaValueId::Bump2d:			return std::make_unique<Bump2dConverter>(params);
		case MayaValueId::Place2dTexture:	return std::make_unique<Place2dTextureConverter>(params);
		case MayaValueId::File:				return std::make_unique<FileNodeConverter>(params);
		case MayaValueId::Checker:			return std::make_unique<NodeCheckerConverter>(params);
		case MayaValueId::ReverseMap:		return std::make_unique<ReverseMapConverter>(params);
		case MayaValueId::LayeredTexture:	return std::make_unique<LayeredTextureConverter>(params);
		case MayaValueId::Ramp:				return std::make_unique<RampNodeConverter>(params);
		case MayaValueId::GammaCorrect:		return std::make_unique<GammaCorrectConverter>(params);
		case MayaValueId::RemapHSV:			return std::make_unique<RemapHSVConverter>(params);
		case MayaValueId::BlendTwoAttr:		return std::make_unique<BlendTwoAttrConverter>(params);
		case MayaValueId::Clamp:			return std::make_unique<ClampConverter>(params);
		case MayaValueId::Projection:		return std::make_unique<ProjectionNodeConverter>(params);
		case MayaValueId::RemapValue:		return std::make_unique<RemapValueConverter>(params);
		case MayaValueId::SetRange:			return std::make_unique<SetRangeConverter>(params);

		default: return nullptr;
	}
}
