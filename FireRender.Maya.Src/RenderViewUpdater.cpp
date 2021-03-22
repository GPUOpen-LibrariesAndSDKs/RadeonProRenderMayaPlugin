#include "RenderViewUpdater.h"

std::vector<RV_PIXEL> RenderViewUpdater::m_pixelData;

void RenderViewUpdater::EnsureBufferIsAllocated(unsigned int width, unsigned int height)
{
	if (m_pixelData.size() != width * height)
	{
		m_pixelData.resize(width * height);
	}
}

void RenderViewUpdater::FlipAndCopyData(
	RV_PIXEL* inputPixelData, 
	unsigned int srcWidth,
	unsigned int srcHeight,
	const RenderRegion& region)
{
	//unsigned int dstOffset = 0;
	unsigned int srcOffset = 0;

	unsigned int dstWidth = region.getWidth();
	unsigned int dstHeight = region.getHeight();

	for (unsigned int y = 0; y < dstHeight; ++y)
	{
		//dstOffset = y * dstWidth;
		// Case: region is subarea of bigger buffer
		if (srcHeight > dstHeight)
		{
			srcOffset = (y + srcHeight - region.top - 1) * srcWidth + region.left;
		}
		// Case: region is the whole buffer
		else
		{
			srcOffset = y * srcWidth;
		}

		std::copy(&inputPixelData[srcOffset], &inputPixelData[srcOffset + dstWidth], &m_pixelData[(dstHeight - y - 1) * dstWidth]);
	}
}

void RenderViewUpdater::UpdateAndRefreshRegion(
	RV_PIXEL* pixelData,
	unsigned int srcWidth,
	unsigned int srcHeight,
	const RenderRegion& region)
{
	// as a first solution - just keep a buffer with necessary size
	EnsureBufferIsAllocated(srcWidth, srcHeight);
	FlipAndCopyData(pixelData, srcWidth, srcHeight, region);

	// Update the render view pixels.
	MRenderView::updatePixels(
		region.left, region.right,
		region.bottom, region.top,
		m_pixelData.data(), true);

	// Refresh the render view.
	MRenderView::refresh(region.left, region.right, region.bottom, region.top);
}
