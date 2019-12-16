#pragma once
#include <FireRenderLayeredTextureUtils.h>

MayaStandardNodeConverters::LayeredTextureConverter::LayeredTextureConverter(const MayaStandardNodeConverters::ConverterParams& params) : BaseConverter(params)
{
}

frw::Value MayaStandardNodeConverters::LayeredTextureConverter::ProcessLayers(const std::vector<LayerInfoType>& layers) const
{
	if (layers.empty())
	{
		return nullptr;
	}

	// First node blend with black color.
	const LayerInfoType& firstLayer = layers.at(layers.size() - 1);

	if (m_params.outPlugName == OUTPUT_COLOR_ATTRIBUTE_NAME)
	{
		frw::Value result = BlendColorValueWithLayer({ 0.0, 0.0, 0.0 }, firstLayer);
		// Reverse order because layers calculated right to left
		for (auto layer = layers.rbegin() + 1; layer != layers.rend(); layer++)
		{
			result = BlendColorValueWithLayer(result, *layer);
		}

		return m_params.scope.MaterialSystem().ValueClamp(result);
	}
	else if (m_params.outPlugName == OUTPUT_ALPHA_ATTRIBUTE_NAME)
	{
		frw::Value result = BlendAlphaValueWithLayer({ 1.0 }, firstLayer);
		// Reverse order because layers calculated right to left
		for (auto layer = layers.rbegin() + 1; layer != layers.rend(); layer++)
		{
			result = BlendAlphaValueWithLayer(result, *layer);
		}

		// Can't do clamp here - it resetting alpha to zero, but maybe it isn't necessary
		return result;
	}
	else
	{
		return nullptr;
	}
}

frw::Value MayaStandardNodeConverters::LayeredTextureConverter::BlendColorValueWithLayer(const frw::Value& backgroundValue, const LayerInfoType& foregroundLayer) const
{
	switch (foregroundLayer.mode)
	{
	case MayaLayeredTextureBlendMode::None:
		return foregroundLayer.color;

	case MayaLayeredTextureBlendMode::Over:
	{
		frw::Value action1 = m_params.scope.MaterialSystem().ValueSub(foregroundLayer.color, backgroundValue);
		frw::Value action2 = m_params.scope.MaterialSystem().ValueMul(action1, foregroundLayer.alpha);
		return m_params.scope.MaterialSystem().ValueAdd(backgroundValue, action2);
	}

	case MayaLayeredTextureBlendMode::In:
		return m_params.scope.MaterialSystem().ValueMul(backgroundValue, foregroundLayer.alpha);

	case MayaLayeredTextureBlendMode::Out:
	{
		frw::Value action1 = m_params.scope.MaterialSystem().ValueSub(1, foregroundLayer.alpha);
		return m_params.scope.MaterialSystem().ValueMul(backgroundValue, action1);
	}

	case MayaLayeredTextureBlendMode::Add:
	{
		frw::Value action1 = m_params.scope.MaterialSystem().ValueMul(foregroundLayer.color, foregroundLayer.alpha);
		return m_params.scope.MaterialSystem().ValueAdd(backgroundValue, action1);
	}

	case MayaLayeredTextureBlendMode::Subtract:
		return m_params.scope.MaterialSystem().ValueSub(backgroundValue, m_params.scope.MaterialSystem().ValueMul(foregroundLayer.color, foregroundLayer.alpha));

	case MayaLayeredTextureBlendMode::Multiply:
	{
		frw::Value action1 = m_params.scope.MaterialSystem().ValueMul(foregroundLayer.color, foregroundLayer.alpha);
		frw::Value action2 = m_params.scope.MaterialSystem().ValueAdd(action1, 1);
		frw::Value action3 = m_params.scope.MaterialSystem().ValueSub(action2, foregroundLayer.alpha);
		return m_params.scope.MaterialSystem().ValueMul(action3, backgroundValue);
	}

	case MayaLayeredTextureBlendMode::Difference:
	{
		frw::Value action1 = m_params.scope.MaterialSystem().ValueSub(foregroundLayer.color, backgroundValue);
		frw::Value action2 = m_params.scope.MaterialSystem().ValueAbs(action1);
		frw::Value action3 = m_params.scope.MaterialSystem().ValueMul(action2, foregroundLayer.alpha);
		frw::Value action4 = m_params.scope.MaterialSystem().ValueSub(1, foregroundLayer.alpha);
		frw::Value action5 = m_params.scope.MaterialSystem().ValueMul(backgroundValue, action4);
		return m_params.scope.MaterialSystem().ValueAdd(action3, action5);
	}

	case MayaLayeredTextureBlendMode::Lighten:
	{
		frw::Value action1 = m_params.scope.MaterialSystem().ValueMax(foregroundLayer.color, backgroundValue);
		frw::Value action2 = m_params.scope.MaterialSystem().ValueMul(action1, foregroundLayer.alpha);
		frw::Value action3 = m_params.scope.MaterialSystem().ValueSub(1, foregroundLayer.alpha);
		frw::Value action4 = m_params.scope.MaterialSystem().ValueMul(action3, backgroundValue);
		return m_params.scope.MaterialSystem().ValueAdd(action2, action4);
	}

	case MayaLayeredTextureBlendMode::Darken:
	{
		frw::Value action1 = m_params.scope.MaterialSystem().ValueMin(foregroundLayer.color, backgroundValue);
		frw::Value action2 = m_params.scope.MaterialSystem().ValueMul(action1, foregroundLayer.alpha);
		frw::Value action3 = m_params.scope.MaterialSystem().ValueSub(1, foregroundLayer.alpha);
		frw::Value action4 = m_params.scope.MaterialSystem().ValueMul(action3, backgroundValue);
		return m_params.scope.MaterialSystem().ValueAdd(action2, action4);
	}

	case MayaLayeredTextureBlendMode::Saturate:
	{
		frw::Value action1 = m_params.scope.MaterialSystem().ValueMul(foregroundLayer.color, foregroundLayer.alpha);
		frw::Value action2 = m_params.scope.MaterialSystem().ValueAdd(1, action1);
		return m_params.scope.MaterialSystem().ValueMul(backgroundValue, action2);
	}

	case MayaLayeredTextureBlendMode::Desatureate:
	{
		frw::Value action1 = m_params.scope.MaterialSystem().ValueMul(foregroundLayer.color, foregroundLayer.alpha);
		frw::Value action2 = m_params.scope.MaterialSystem().ValueSub(1, action1);
		return m_params.scope.MaterialSystem().ValueMul(backgroundValue, action2);
	}

	case MayaLayeredTextureBlendMode::Illuminate:
	{
		frw::Value action1 = m_params.scope.MaterialSystem().ValueMul(2, foregroundLayer.color);
		frw::Value action2 = m_params.scope.MaterialSystem().ValueMul(action1, foregroundLayer.alpha);
		frw::Value action3 = m_params.scope.MaterialSystem().ValueAdd(action2, 1);
		frw::Value action4 = m_params.scope.MaterialSystem().ValueSub(action3, foregroundLayer.alpha);
		return m_params.scope.MaterialSystem().ValueMul(backgroundValue, action4);
	}

	default:
		assert(false);
		return frw::Value();
	}
}

frw::Value MayaStandardNodeConverters::LayeredTextureConverter::BlendAlphaValueWithLayer(const frw::Value& backgroundValue, const LayerInfoType& foregroundLayer) const
{
	switch (foregroundLayer.mode)
	{
	case MayaLayeredTextureBlendMode::None:
		return foregroundLayer.alpha;

	case MayaLayeredTextureBlendMode::Over:
	{
		frw::Value action1 = m_params.scope.MaterialSystem().ValueAdd(backgroundValue, foregroundLayer.alpha);
		frw::Value action2 = m_params.scope.MaterialSystem().ValueMul(backgroundValue, foregroundLayer.alpha);
		return m_params.scope.MaterialSystem().ValueSub(action1, action2);
	}

	case MayaLayeredTextureBlendMode::In:
		return m_params.scope.MaterialSystem().ValueMul(backgroundValue, foregroundLayer.alpha);

	case MayaLayeredTextureBlendMode::Out:
	{
		frw::Value action1 = m_params.scope.MaterialSystem().ValueSub(1, foregroundLayer.alpha);
		return m_params.scope.MaterialSystem().ValueMul(backgroundValue, action1);
	}

	case MayaLayeredTextureBlendMode::Add:
	case MayaLayeredTextureBlendMode::Subtract:
	case MayaLayeredTextureBlendMode::Multiply:
	case MayaLayeredTextureBlendMode::Difference:
	case MayaLayeredTextureBlendMode::Lighten:
	case MayaLayeredTextureBlendMode::Darken:
	case MayaLayeredTextureBlendMode::Saturate:
	case MayaLayeredTextureBlendMode::Desatureate:
	case MayaLayeredTextureBlendMode::Illuminate:
		return backgroundValue;

	default:
		assert(false);
		return frw::Value();
	}
}

MayaStandardNodeConverters::LayeredTextureConverter::LayerInfoType MayaStandardNodeConverters::LayeredTextureConverter::GetLayerInfo(const MPlug& inputsPlug, bool alphaIsLuminance) const
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

		if (attrName == IS_VISIBLE_ATTRIBUTE_NAME)
		{
			isVisible = childPlug.asBool();
		}
		else if (attrName == BLEND_MODE_ATTRIBUTE_NAME)
		{
			blendMode = static_cast<MayaLayeredTextureBlendMode>(childPlug.asInt());
		}
		else if (attrName == COLOR_ATTRIBUTE_NAME)
		{
			// Color should be parsed when alphaIsLuminance too
			if (m_params.outPlugName == OUTPUT_COLOR_ATTRIBUTE_NAME || alphaIsLuminance)
			{
				color = m_params.scope.GetValue(childPlug);
			}
		}
		else if (attrName == ALPHA_ATTRIBUTE_NAME)
		{
			if (m_params.outPlugName == OUTPUT_ALPHA_ATTRIBUTE_NAME && alphaIsLuminance)
			{
				// Color parsed before alpha, so we must have valid value
				if (color.IsNull())
				{
					assert(false);
				}

				alpha = m_params.scope.MaterialSystem().ValueConvertToLuminance(color);
			}
			else
			{
				// Default processing
				alpha = m_params.scope.GetValue(childPlug);
			}
		}
	}

	return { blendMode, color, alpha, isVisible };
}

frw::Value MayaStandardNodeConverters::LayeredTextureConverter::Convert() const
{
	MPlug alphaIsLuminancePlug = m_params.shaderNode.findPlug(PLUG_ALPHA_IS_LUMINANCE);
	bool alphaIsLuminance = alphaIsLuminancePlug.asBool();

	MPlug inputs = m_params.shaderNode.findPlug(PLUG_INPUTS, false);
	unsigned inputsCount = inputs.numElements();

	std::vector<LayerInfoType> layers;
	layers.reserve(inputsCount);

	for (unsigned int inputIndex = 0; inputIndex < inputsCount; inputIndex++)
	{
		MPlug inputsPlug = inputs.elementByPhysicalIndex(inputIndex);
		LayerInfoType layer = GetLayerInfo(inputsPlug, alphaIsLuminance);
		if (layer.isVisible)
		{
			layers.emplace_back(layer);
		}
	}

	return ProcessLayers(layers);
}
