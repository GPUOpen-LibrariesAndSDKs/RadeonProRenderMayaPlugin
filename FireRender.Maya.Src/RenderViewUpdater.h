#pragma once
#include <maya/MRenderView.h>

#include "RenderRegion.h"

#include <vector>

class RenderViewUpdater
{
public:
	static void UpdateAndRefreshRegion(
		RV_PIXEL* pixelData,
		unsigned int srcWidth,
		unsigned int srcHeight,
		const RenderRegion& region);

private:
	static void EnsureBufferIsAllocated(unsigned int width, unsigned int height);
	static void FlipAndCopyData(
		RV_PIXEL* inputPixelData,
		unsigned int srcWidth,
		unsigned int srcHeight,
		const RenderRegion& region);

private:
	static std::vector<RV_PIXEL> m_pixelData;
};

