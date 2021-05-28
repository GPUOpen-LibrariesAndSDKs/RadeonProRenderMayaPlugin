#pragma once

#include <maya/MShaderManager.h>
#include <vector>

class ViewportTexture
{
public:
	ViewportTexture();
	~ViewportTexture();

	// only from main thread
	MStatus UpdateTexture(float* externalData = nullptr);

	void Resize(unsigned int width, unsigned int height);

	void Release();

	MTexture* GetTexture() const{ return m_texture; }

	float* GetPixelData() { return m_pixels.data(); }
	void SetPixelData(std::vector<float> vecData);

private:
	void ClearPixels();

private:
	MHWRender::MTexture* m_texture;
	std::vector<float> m_pixels;
};
