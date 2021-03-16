#pragma once
#include <maya/MRenderView.h>

#include <vector>

class RenderViewUpdater
{
public:
	static void UpdateAndRefreshRegion(RV_PIXEL* pixelData,
		unsigned int regionLeft, unsigned int regionTop, 
		unsigned int regionRight, unsigned int regionBottom);

	//static RenderViewUpdater m_sInstance;

private:
	static void EnsureBufferIsAllocated(unsigned int width, unsigned int height);
	static void FlipAndCopyData(RV_PIXEL* inputPixelData, unsigned int width, unsigned int height);

private:
	static std::vector<RV_PIXEL> m_pixelData;
};

