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
	const MTextureAssignment& texture = viewport.getTexture();

	// Check if the viewport texture has changed.
	if (viewport.hasTextureChanged() || m_texture.texture != texture.texture)
	{
		m_texture.texture = texture.texture;
		m_textureChanged = true;
	}
}
