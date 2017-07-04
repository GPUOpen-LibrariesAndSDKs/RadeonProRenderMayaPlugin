#pragma once

#include <maya/MString.h>
#include <maya/MViewport2Renderer.h>
#include <maya/MDrawContext.h>
#include <maya/MFrameContext.h>
#include <maya/MShaderManager.h>
#include "FireRenderViewport.h"

/**
 * Render a quad containing RPR frame
 * buffer data to a Maya viewport panel.
 */
class FireRenderViewportBlit : public MHWRender::MQuadRender
{

public:

	// Life Cycle
	// -----------------------------------------------------------------------------

	FireRenderViewportBlit(const MString &name);
	~FireRenderViewportBlit();


	// MQuadRender Implementation
	// -----------------------------------------------------------------------------

	/** Initialize the shader used to render the quad. */
	virtual const MHWRender::MShaderInstance* shader();

	/** Configure the clear operation. */
	virtual MHWRender::MClearOperation& clearOperation();


	// Public Methods
	// -----------------------------------------------------------------------------

	/** Update the texture for the quad to render from the viewport. */
	void updateTexture(FireRenderViewport& viewport);


private:

	// Members
	// -----------------------------------------------------------------------------

	/** The shader used to render the quad. */
	MHWRender::MShaderInstance* m_shaderInstance;

	/** The texture applied to the quad. Owned by the RPR viewport. */
	MHWRender::MTextureAssignment m_texture;

	/** True if the texture has changed. */
	bool m_textureChanged;
};
