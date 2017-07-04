#include "FireRenderViewportOperation.h"

// Life Cycle
// -----------------------------------------------------------------------------
FireRenderViewportOperation::FireRenderViewportOperation(const MString& name, Type type) :
	MHWRender::MUserRenderOperation(name),
	m_viewport(nullptr),
	m_type(type)
{}

// -----------------------------------------------------------------------------
FireRenderViewportOperation::~FireRenderViewportOperation()
{
}


// Public Methods
// -----------------------------------------------------------------------------
void FireRenderViewportOperation::setViewport(FireRenderViewport& viewport)
{
	m_viewport = &viewport;
}


// MUserRenderOperation Implementation
// -----------------------------------------------------------------------------
MStatus FireRenderViewportOperation::execute(const MHWRender::MDrawContext &drawContext)
{
	// Ensure a viewport exists.
	if (!m_viewport)
		return MStatus::kFailure;

	// Perform the operation.
	switch (m_type)
	{
		case PRE_BLIT:
			m_viewport->preBlit();
			break;

		case POST_BLIT:
			m_viewport->postBlit();
			break;
	}

	return MStatus::kSuccess;
}
