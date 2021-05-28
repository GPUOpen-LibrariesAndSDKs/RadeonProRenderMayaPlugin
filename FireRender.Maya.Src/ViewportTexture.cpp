#include <maya/MTextureManager.h>
#include <maya/MRenderView.h>
#include <assert.h>

#include "ViewportTexture.h"


ViewportTexture::ViewportTexture() : m_texture(nullptr)
{

}

ViewportTexture::~ViewportTexture()
{
	Release();
}

MStatus ViewportTexture::UpdateTexture(float* externalData /* = nullptr*/)
{
	if (m_texture != nullptr)
	{
		return m_texture->update(externalData ? externalData : m_pixels.data(), false);
	}

	return MStatus::kFailure;
}

void ViewportTexture::ClearPixels()
{
	assert(m_pixels.size() > 0);

	RV_PIXEL zero;
	zero.r = 0;
	zero.g = 0;
	zero.b = 0;
	zero.a = 1;

	const unsigned int numComponents = 4;
	for (unsigned int i = 0; i < m_pixels.size(); i += numComponents)
	{
		memcpy(m_pixels.data() + i, &zero, sizeof(RV_PIXEL));
	}
}


void ViewportTexture::Resize(unsigned int width, unsigned int height)
{
	// Create the hardware backed texture if required.

	MTextureDescription texDescription;

	if (m_texture != nullptr)
	{
		m_texture->textureDescription(texDescription);
	}

	bool createNewTexture = m_texture == nullptr ||
		texDescription.fWidth != width ||
		texDescription.fHeight != height;

	if (createNewTexture)
	{
		Release();

		m_pixels.resize(width * height * 4);
		ClearPixels();

		/** The description of the texture that will receive the RPR frame buffer. */
		MHWRender::MTextureDescription textureDesc;

		textureDesc.setToDefault2DTexture();
		textureDesc.fFormat = MHWRender::MRasterFormat::kR32G32B32A32_FLOAT;

		// Update the texture description.
		textureDesc.setToDefault2DTexture();
		textureDesc.fWidth = width;
		textureDesc.fHeight = height;
		textureDesc.fDepth = 1;
		textureDesc.fBytesPerRow = 4 * sizeof(float) * width;
		textureDesc.fBytesPerSlice = textureDesc.fBytesPerRow * height;
		textureDesc.fFormat = MHWRender::MRasterFormat::kR32G32B32A32_FLOAT;

		// Create a new texture with the supplied data.
		MRenderer* renderer = MRenderer::theRenderer();

		MTextureManager* textureManager = renderer->getTextureManager();

		m_texture = textureManager->acquireTexture("", textureDesc, m_pixels.data(), false);
	}
}

void ViewportTexture::Release()
{
	MRenderer* renderer = MRenderer::theRenderer();
	MTextureManager* textureManager = renderer->getTextureManager();

	if (m_texture != nullptr)
	{
		textureManager->releaseTexture(m_texture);
		m_texture = nullptr;
	}
}

void ViewportTexture::SetPixelData(std::vector<float> vecData)
{
	m_pixels = std::move(vecData);
}
