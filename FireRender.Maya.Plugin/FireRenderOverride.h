#pragma once

#include <maya/MString.h>
#include <maya/MViewport2Renderer.h>
#include <maya/MDrawContext.h>
#include <maya/MFrameContext.h>

#include "FireRenderContext.h"
#include "ShadersManager.h"
#include "FireRenderViewportOperation.h"


/**
 * A render override for drawing the RPR frame
 * buffer to a Maya viewport. A static instance
 * is created when the plug-in is loaded.
 */
class FireRenderOverride : public MHWRender::MRenderOverride
{

public:

	// Life Cycle
	// -----------------------------------------------------------------------------

	FireRenderOverride(const MString &name);
	virtual ~FireRenderOverride();


	// Singleton Instance Methods
	// -----------------------------------------------------------------------------

	/** Get the override instance. */
	static FireRenderOverride* instance();

	/** Delete the override instance. */
	static void deleteInstance();


	// MRenderOverride Implementation
	// -----------------------------------------------------------------------------

	/** Return the supported viewport rendering modes. */
	virtual MHWRender::DrawAPI supportedDrawAPIs() const;

	/** Reset to the first render operation. */
	virtual bool startOperationIterator();

	/** Return the current render operation, otherwise null. */
	virtual MHWRender::MRenderOperation* renderOperation();

	/** Move to the next render operation if there is one. */
	virtual bool nextRenderOperation();

	/** Reset the current render operation. */
	virtual MStatus cleanup();

	/** Return the name to use for the renderer in the Maya UI. */
	virtual MString uiName() const;

	/** Perform any setup required before performing the render operations. */
	virtual MStatus setup(const MString & destination);


	// Public Methods
	// -----------------------------------------------------------------------------

	/** Initialize the override. */
	void initialize();


private:

	// Constants
	// -----------------------------------------------------------------------------

	/** The number of operations. */
	int c_operationCount = 6;


	// Members
	// -----------------------------------------------------------------------------

	/** The global override instance. */
	static FireRenderOverride* s_instance;

	/** The name to use for the renderer in the Maya UI. */
	MString m_uiName;

	/** The list of render operations. */
	MHWRender::MRenderOperation* m_operations[6];

	/** The list of corresponding operation names. */
	MString m_operationNames[6];

	/** The currently active render operation. */
	int m_currentOperation;

	/** The custom pre blit operation. */
	FireRenderViewportOperation* m_preBlitOperation;

	/** The custom post blit operation. */
	FireRenderViewportOperation* m_postBlitOperation;


	// Private Methods
	// -----------------------------------------------------------------------------

	/** Set up the viewport. */
	MStatus setupViewport(const MString & panelName);

	/** Create render operations if required. */
	bool setupOperations(FireRenderViewport& viewport);
};
