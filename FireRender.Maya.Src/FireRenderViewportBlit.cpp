/**********************************************************************
Copyright 2020 Advanced Micro Devices, Inc
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
********************************************************************/
#include "FireRenderViewportBlit.h"


// Namespaces
// -----------------------------------------------------------------------------

using namespace MHWRender;


// Life Cycle
// -----------------------------------------------------------------------------
FireRenderViewportBlit::FireRenderViewportBlit(const MString &name) :
	MQuadRender(name),
	m_shaderInstance(nullptr),
	m_textureChanged(false)
{
	m_texture.texture = nullptr;
}

// -----------------------------------------------------------------------------
FireRenderViewportBlit::~FireRenderViewportBlit()
{
	// Get the Maya renderer.
	MRenderer* renderer = MRenderer::theRenderer();
	if (!renderer)
		return;

	// Release the shader.
	if (m_shaderInstance)
	{
		const MShaderManager* shaderMgr = renderer->getShaderManager();
		if (shaderMgr)
			shaderMgr->releaseShader(m_shaderInstance);

		m_shaderInstance = nullptr;
	}
}


// MQuadRender Implementation
// -----------------------------------------------------------------------------
const MShaderInstance* FireRenderViewportBlit::shader()
{
	// Create the shader if required.
	if (!m_shaderInstance)
	{
		// Get the shader manager.
		MRenderer* renderer = MRenderer::theRenderer();
		const MShaderManager* shaderMgr =
			renderer ? renderer->getShaderManager() : nullptr;

		// Create a shader instance. The shader is loaded
		// from the shaders folder and the specific shader file
		// used is base on the current viewport render engine.
		if (shaderMgr)
			m_shaderInstance = shaderMgr->getEffectsFileShader("rprBlitColor", "");
	}

	// Bind a new color texture if it has changed.
	MStatus status;
	if (m_textureChanged && m_shaderInstance)
	{
		status = m_shaderInstance->setParameter("colorTexture", m_texture);
		m_textureChanged = false;

		if (status != MStatus::kSuccess)
			return nullptr;
	}

	// Return the shader instance.
	return m_shaderInstance;
}

// -----------------------------------------------------------------------------
MClearOperation & FireRenderViewportBlit::clearOperation()
{
	mClearOperation.setClearGradient(false);
	mClearOperation.setMask((unsigned int)MClearOperation::kClearAll);
	return mClearOperation;
}


// Public Methods
// -----------------------------------------------------------------------------
void FireRenderViewportBlit::updateTexture(FireRenderViewport& viewport)
{
	// Get the viewport texture.
	ViewportTexture* viewportTexture = viewport.getTexture();

	// Check if the viewport texture has changed.
	if (viewport.hasTextureChanged() || m_texture.texture != viewportTexture->GetTexture())
	{
		m_texture.texture = viewportTexture->GetTexture();
		m_textureChanged = true;
	}
}
