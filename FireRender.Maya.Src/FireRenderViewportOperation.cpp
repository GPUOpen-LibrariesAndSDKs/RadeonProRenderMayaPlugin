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
