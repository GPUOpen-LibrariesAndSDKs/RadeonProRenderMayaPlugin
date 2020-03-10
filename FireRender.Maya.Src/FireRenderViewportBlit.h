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
