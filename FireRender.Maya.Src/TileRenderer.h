#pragma once

#include <functional>

#include "RenderRegion.h"
#include "FireRenderAOV.h"

class FireRenderContext;

enum class TileRenderFillType
{
	Normal = 0

	// _TODO Add Different algorithms, i.e spiral etc
};

struct TileRenderInfo
{
	unsigned int totalWidth;
	unsigned int totalHeight;

	unsigned int tileSizeX;
	unsigned int tileSizeY;

	TileRenderFillType tilesFillType;
};

typedef std::function<bool(RenderRegion&, int, std::map<unsigned int, PixelBuffer>& out)> TileRenderingCallback;

class TileRenderer
{
public:
	TileRenderer();
	~TileRenderer();

	void Render(FireRenderContext& renderContext, const TileRenderInfo& info, std::map<unsigned int, PixelBuffer>& outBuffer, TileRenderingCallback callbackFunc);
};

