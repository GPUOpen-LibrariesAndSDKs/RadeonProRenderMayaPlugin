#pragma once
#include <frWrap.h>
#include <FireMaya.h>

namespace FireRenderLayeredTextureUtils
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

	/** Called from FireMaya, returns calculated value from layers blend */
	frw::Value ConvertMayaLayeredTexture(const MFnDependencyNode& node, const frw::MaterialSystem& ms, FireMaya::Scope& scope);

	/** Iterates through array of LayerInfoType and blends them */
	frw::Value ProcessLayers(const std::vector<LayerInfoType>& layers, const frw::MaterialSystem& ms);

	/** 
		Blends layer with previous calculated value 
		Ignores background alpha value 
	*/
	frw::Value BlendLayerWithValue(const frw::Value& bg, const LayerInfoType& fg, const frw::MaterialSystem& ms);

	/** Retrieves layer info from the maya plug */
	LayerInfoType GetLayerInfo(const MPlug& inputsPlug, FireMaya::Scope& scope);
}
