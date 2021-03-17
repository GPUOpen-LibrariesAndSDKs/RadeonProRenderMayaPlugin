#include "RenderViewUpdater.h"

std::vector<RV_PIXEL> RenderViewUpdater::m_pixelData;

void RenderViewUpdater::EnsureBufferIsAllocated(unsigned int width, unsigned int height)
{
	if (m_pixelData.size() != width * height)
	{
		m_pixelData.resize(width * height);
	}
}

void RenderViewUpdater::FlipAndCopyData(RV_PIXEL* inputPixelData, unsigned int width, unsigned int height)
{
	int yOffset = 0;
	for (unsigned int y = 0; y < height; ++y)
	{
		yOffset = y * width;

		std::copy(&inputPixelData[yOffset], &inputPixelData[yOffset + width], &m_pixelData[(height - y - 1) * width]);
	}
}

void RenderViewUpdater::UpdateAndRefreshRegion(RV_PIXEL* pixelData, 
												unsigned int regionLeft, 
												unsigned int regionTop, 
												unsigned int regionRight, 
												unsigned int regionBottom)
{
	unsigned int width = regionRight - regionLeft + 1;
	unsigned int height = regionBottom - regionTop + 1;

	// as a first solution - just keep a buffer with necessary size
	EnsureBufferIsAllocated(width, height);
	FlipAndCopyData(pixelData, width, height);

	// Update the render view pixels.
	MRenderView::updatePixels(
		regionLeft, regionRight,
		regionTop, regionBottom,
		m_pixelData.data(), true);

	// Refresh the render view.
	MRenderView::refresh(regionLeft, regionRight, regionTop, regionBottom);
}