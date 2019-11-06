#pragma once
#include <FireRenderLayeredTextureUtils.h>

using FireRenderLayeredTextureUtils::LayerInfoType;
using FireRenderLayeredTextureUtils::MayaLayeredTextureBlendMode;

frw::Value FireRenderLayeredTextureUtils::ProcessLayers(const std::vector<LayerInfoType>& layers, const frw::MaterialSystem& ms)
{
	if (layers.empty())
	{
		return nullptr;
	}

	// First node blend with black color.
	const LayerInfoType& firstLayer = layers.at(layers.size() - 1);
	frw::Value result = BlendLayerWithValue({ 0.0, 0.0, 0.0 }, firstLayer, ms);

	// Reverse order because layers calculated right to left
	for (auto layer = layers.rbegin() + 1; layer != layers.rend(); layer++)
	{
		result = BlendLayerWithValue(result, *layer, ms);
	}

	return ms.ValueClamp(result);
}

frw::Value FireRenderLayeredTextureUtils::BlendLayerWithValue(const frw::Value& bg, const LayerInfoType& fg, const frw::MaterialSystem& ms)
{
	switch (fg.mode)
	{
	case MayaLayeredTextureBlendMode::None:
		return fg.color;

	case MayaLayeredTextureBlendMode::Over:
	{
		frw::Value action1 = ms.ValueSub(fg.color, bg);
		frw::Value action2 = ms.ValueMul(action1, fg.alpha);
		return ms.ValueAdd(bg, action2);
	}

	case MayaLayeredTextureBlendMode::In:
		return ms.ValueMul(bg, fg.alpha);

	case MayaLayeredTextureBlendMode::Out:
	{
		frw::Value action1 = ms.ValueSub(1, fg.alpha);
		return ms.ValueMul(bg, action1);
	}

	case MayaLayeredTextureBlendMode::Add:
	{
		frw::Value action1 = ms.ValueMul(fg.color, fg.alpha);
		return ms.ValueAdd(bg, action1);
	}

	case MayaLayeredTextureBlendMode::Subtract:
		return ms.ValueSub(bg, ms.ValueMul(fg.color, fg.alpha));

	case MayaLayeredTextureBlendMode::Multiply:
	{
		frw::Value action1 = ms.ValueMul(fg.color, fg.alpha);
		frw::Value action2 = ms.ValueAdd(action1, 1);
		frw::Value action3 = ms.ValueSub(action2, fg.alpha);
		return ms.ValueMul(action3, bg);
	}

	case MayaLayeredTextureBlendMode::Difference:
	{
		frw::Value action1 = ms.ValueSub(fg.color, bg);
		frw::Value action2 = ms.ValueAbs(action1);
		frw::Value action3 = ms.ValueMul(action2, fg.alpha);
		frw::Value action4 = ms.ValueSub(1, fg.alpha);
		frw::Value action5 = ms.ValueMul(bg, action4);
		return ms.ValueAdd(action3, action5);
	}

	case MayaLayeredTextureBlendMode::Lighten:
	{
		frw::Value action1 = ms.ValueMax(fg.color, bg);
		frw::Value action2 = ms.ValueMul(action1, fg.alpha);
		frw::Value action3 = ms.ValueSub(1, fg.alpha);
		frw::Value action4 = ms.ValueMul(action3, bg);
		return ms.ValueAdd(action2, action4);
	}

	case MayaLayeredTextureBlendMode::Darken:
	{
		frw::Value action1 = ms.ValueMin(fg.color, bg);
		frw::Value action2 = ms.ValueMul(action1, fg.alpha);
		frw::Value action3 = ms.ValueSub(1, fg.alpha);
		frw::Value action4 = ms.ValueMul(action3, bg);
		return ms.ValueAdd(action2, action4);
	}

	case MayaLayeredTextureBlendMode::Saturate:
	{
		frw::Value action1 = ms.ValueMul(fg.color, fg.alpha);
		frw::Value action2 = ms.ValueAdd(1, action1);
		return ms.ValueMul(bg, action2);
	}

	case MayaLayeredTextureBlendMode::Desatureate:
	{
		frw::Value action1 = ms.ValueMul(fg.color, fg.alpha);
		frw::Value action2 = ms.ValueSub(1, action1);
		return ms.ValueMul(bg, action2);
	}

	case MayaLayeredTextureBlendMode::Illuminate:
	{
		frw::Value action1 = ms.ValueMul(2, fg.color);
		frw::Value action2 = ms.ValueMul(action1, fg.alpha);
		frw::Value action3 = ms.ValueAdd(action2, 1);
		frw::Value action4 = ms.ValueSub(action3, fg.alpha);
		return ms.ValueMul(bg, action4);
	}

	default:
		assert(false);
		return frw::Value();
	}
}

LayerInfoType FireRenderLayeredTextureUtils::GetLayerInfo(const MPlug& inputsPlug, FireMaya::Scope& scope)
{
	MayaLayeredTextureBlendMode blendMode;
	frw::Value color;
	frw::Value alpha;
	bool isVisible;

	// Parse attibutes
	for (unsigned childIndex = 0; childIndex < inputsPlug.numChildren(); childIndex++)
	{
		MPlug childPlug = inputsPlug.child(childIndex);
		MFnAttribute childAttribute(childPlug.attribute());
		MString attrName = childAttribute.name();

		if (attrName == "isVisible")
		{
			isVisible = childPlug.asBool();
		}
		else if (attrName == "blendMode")
		{
			blendMode = static_cast<MayaLayeredTextureBlendMode>(childPlug.asInt());
		}
		else if (attrName == "color")
		{
			color = scope.GetValue(childPlug);
		}
		else if (attrName == "alpha")
		{
			alpha = scope.GetValue(childPlug);
		}
	}

	return { blendMode, color, alpha, isVisible };
}

frw::Value FireRenderLayeredTextureUtils::ConvertMayaLayeredTexture(const MFnDependencyNode& node, const frw::MaterialSystem& ms, FireMaya::Scope& scope)
{
	MPlug inputs = node.findPlug("inputs", false);
	unsigned inputsCount = inputs.numElements();

	std::vector<LayerInfoType> layers;
	layers.reserve(inputsCount);

	for (int inputIndex = 0; inputIndex < inputsCount; inputIndex++)
	{
		MPlug inputsPlug = inputs.elementByPhysicalIndex(inputIndex);
		LayerInfoType layer = GetLayerInfo(inputsPlug, scope);
		if (layer.isVisible)
		{
			layers.emplace_back(layer);
		}
	}

	return ProcessLayers(layers, ms);
}
