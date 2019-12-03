#pragma once
#include <frWrap.h>
#include <FireMaya.h>

class LayeredTextureConverter
{
	enum class MayaLayeredTextureBlendMode
	{
		None,
		Over,
		In,
		Out,
		Add,
		Subtract,
		Multiply,
		Difference,
		Lighten,
		Darken,
		Saturate,
		Desatureate,
		Illuminate
	};

	struct LayerInfoType
	{
		MayaLayeredTextureBlendMode mode;
		frw::Value color;
		frw::Value alpha;
		bool isVisible;
	};

	// Output attributes
	const MString OUTPUT_COLOR_ATTRIBUTE_NAME = "oc";
	const MString OUTPUT_ALPHA_ATTRIBUTE_NAME = "oa";

	// Node attributes
	const MString COLOR_ATTRIBUTE_NAME = "color";
	const MString ALPHA_ATTRIBUTE_NAME = "alpha";
	const MString IS_VISIBLE_ATTRIBUTE_NAME = "isVisible";
	const MString BLEND_MODE_ATTRIBUTE_NAME = "blendMode";

	// Plug names
	const MString PLUG_ALPHA_IS_LUMINANCE = "alphaIsLuminance";
	const MString PLUG_INPUTS = "inputs";

	// Class members
	const MFnDependencyNode& m_node;
	const frw::MaterialSystem& m_materialSystem;
	FireMaya::Scope& m_scope;
	const MString& m_outPlugName;

public:
	LayeredTextureConverter(const MFnDependencyNode& node, const frw::MaterialSystem& materialSystem, FireMaya::Scope& scope, const MString& outPlugName);

	/** Called from FireMaya, returns calculated value from layers blend */
	frw::Value Convert();

private:
	/** Iterates through array of LayerInfoType and blends them */
	frw::Value ProcessLayers(const std::vector<LayerInfoType>& layers);

	/**
		Blends layer with previous calculated value
		Ignores background alpha value
	*/
	frw::Value BlendColorValueWithLayer(const frw::Value& backgroundValue, const LayerInfoType& foregroundLayer);
	frw::Value BlendAlphaValueWithLayer(const frw::Value& backgroundValue, const LayerInfoType& foregroundLayer);

	/** Retrieves layer info from the maya plug */
	LayerInfoType GetLayerInfo(const MPlug& inputsPlug, bool alphaIsLuminance);
};
