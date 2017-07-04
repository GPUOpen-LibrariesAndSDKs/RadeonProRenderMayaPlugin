#pragma once

#include <maya/MViewport2Renderer.h>

/** A scene render that draws UI elements such as the resolution gate. */
class FireRenderViewportUI : public MHWRender::MSceneRender
{

public:

	// Life Cycle
	// -----------------------------------------------------------------------------

	FireRenderViewportUI(const MString& name);
	virtual ~FireRenderViewportUI();


	// MSceneRender Implementation
	// -----------------------------------------------------------------------------

	/** Filter the scene to show only UI elements. */
	virtual MHWRender::MSceneRender::MSceneFilterOption renderFilterOverride();

	/** Exclude object types that shouldn't be drawn. */
	virtual MHWRender::MSceneRender::MObjectTypeExclusions objectTypeExclusions();

	/** Disable clear. */
	virtual MHWRender::MClearOperation& clearOperation();
};
