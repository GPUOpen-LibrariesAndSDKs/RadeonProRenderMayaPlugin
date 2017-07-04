#pragma once

#include <maya/MString.h>
#include <maya/MViewport2Renderer.h>
#include "FireRenderViewport.h"


/**
 * A custom operation that can be added to a
 * render override operation list, to be
 * performed in sequence with other operations
 * such as viewport blitting and UI drawing.
 */
class FireRenderViewportOperation : public MHWRender::MUserRenderOperation
{

public:

	// Enums
	// -----------------------------------------------------------------------------

	/** The type of operation to perform.*/
	enum Type
	{
		PRE_BLIT,
		POST_BLIT
	};


	// Life Cycle
	// -----------------------------------------------------------------------------

	FireRenderViewportOperation(const MString& name, Type type);
	virtual ~FireRenderViewportOperation();


	// MUserRenderOperation Implementation
	// -----------------------------------------------------------------------------

	/** Perform the operation. */
	virtual MStatus execute(const MHWRender::MDrawContext &drawContext);


	// Public Methods
	// -----------------------------------------------------------------------------

	/** Set the associated viewport. */
	void setViewport(FireRenderViewport& viewport);


private:

	// Members
	// -----------------------------------------------------------------------------

	/** The viewport associated with this operation. */
	FireRenderViewport* m_viewport;

	/** The type of operation to perform. */
	Type m_type;
};
