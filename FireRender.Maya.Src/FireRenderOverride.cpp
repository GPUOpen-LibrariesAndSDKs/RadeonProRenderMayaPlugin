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
#include <GL/glew.h>
#include <cassert>
#include <sstream>
#include <iomanip>
#include "frWrap.h"
#include "common.h"

#include <maya/MNodeMessage.h>
#include <maya/MShaderManager.h>
#include <maya/MAnimControl.h>

#include "FireRenderOverride.h"
#include "FireRenderViewport.h"
#include "FireRenderViewportManager.h"
#include "FireRenderViewportBlit.h"
#include "FireRenderViewportUI.h"
#include "Context/FireRenderContext.h"


// Static Initialization
// -----------------------------------------------------------------------------
FireRenderOverride* FireRenderOverride::s_instance = nullptr;


// Life Cycle
// -----------------------------------------------------------------------------
FireRenderOverride::FireRenderOverride(const MString & name) :
	MRenderOverride(name),
	m_uiName(FIRE_RENDER_NAME),
	m_currentOperation(-1),
	m_preBlitOperation(nullptr),
	m_postBlitOperation(nullptr)
{
	for (int i = 0; i < c_operationCount; i++)
		m_operations[i] = nullptr;

	m_operationNames[0] = "FireRenderOverride_PreBlit";
	m_operationNames[1] = "FireRenderOverride_Blit";
	m_operationNames[2] = "FireRenderOverride_PostBlit";
	m_operationNames[3] = "FireRenderOverride_Scene";
	m_operationNames[4] = "FireRenderOverride_HUD";
	m_operationNames[5] = "FireRenderOverride_Present";
}

// -----------------------------------------------------------------------------
FireRenderOverride::~FireRenderOverride()
{
	// Delete render operations.
	for (int i = 0; i < c_operationCount; i++)
		if (m_operations[i])
			delete m_operations[i];

	// Unregister the override with Maya.
	auto renderer = MHWRender::MRenderer::theRenderer();
	if (!renderer)
		return;

	renderer->deregisterOverride(this);
}


// Singleton Instance Methods
// -----------------------------------------------------------------------------
FireRenderOverride* FireRenderOverride::instance()
{
	if (!s_instance)
		s_instance = new FireRenderOverride("FireRenderOverride");

	return s_instance;
}

// -----------------------------------------------------------------------------
void FireRenderOverride::deleteInstance()
{
	if (!s_instance)
		return;

	delete s_instance;
	s_instance = nullptr;
}


// Public Methods
// -----------------------------------------------------------------------------
void FireRenderOverride::initialize()
{
	// Get the Maya renderer.
	auto renderer = MHWRender::MRenderer::theRenderer();
	if (!renderer)
		return;

	// Register the override with Maya.
	MStatus status = renderer->registerOverride(this);
	if (status != MStatus::kSuccess)
		MGlobal::displayError("Unable to register" + MString(FIRE_RENDER_NAME) + "render override.");

	// Add shader path containing shaders
	// for rendering the viewport quad.
	if (auto shaderManager = renderer->getShaderManager())
		shaderManager->addShaderPath(FireMaya::GetShaderPath());
}


// MRenderOverride Implementation
// -----------------------------------------------------------------------------
MHWRender::DrawAPI FireRenderOverride::supportedDrawAPIs() const
{
	return MHWRender::kAllDevices;
}

// -----------------------------------------------------------------------------
bool FireRenderOverride::startOperationIterator()
{
	m_currentOperation = 0;
	return true;
}

// -----------------------------------------------------------------------------
MHWRender::MRenderOperation *FireRenderOverride::renderOperation()
{
	if (m_currentOperation >= 0 && m_currentOperation < c_operationCount && m_operations[m_currentOperation])
		return m_operations[m_currentOperation];

	return nullptr;
}

// -----------------------------------------------------------------------------
bool FireRenderOverride::nextRenderOperation()
{
	m_currentOperation++;
	return m_currentOperation < c_operationCount;
}

// -----------------------------------------------------------------------------
MStatus FireRenderOverride::cleanup()
{
	m_currentOperation = -1;
	return MStatus::kSuccess;
}

// -----------------------------------------------------------------------------
MString FireRenderOverride::uiName() const
{
	return m_uiName;
}

// -----------------------------------------------------------------------------
MStatus FireRenderOverride::setup(const MString& panelName)
{
	// Set up the viewport.
	if (setupViewport(panelName) == MStatus::kFailure)
		return MStatus::kFailure;

	// Ensure the view is rendered using gouraud shaded display style.
	M3dView view;
    MStatus status = FireRenderViewport::FindMayaView(panelName, &view);

	if (panelName.length() && status == MStatus::kSuccess)
		if (view.displayStyle() != M3dView::kGouraudShaded)
			view.setDisplayStyle(M3dView::kGouraudShaded);

	// Override setup complete.
	return MStatus::kSuccess;
}

// -----------------------------------------------------------------------------
bool FireRenderOverride::setupOperations(FireRenderViewport& viewport)
{
	// Create operations if required.
	if (!m_operations[0])
	{
		m_preBlitOperation = new FireRenderViewportOperation(m_operationNames[0], FireRenderViewportOperation::PRE_BLIT);
		m_postBlitOperation = new FireRenderViewportOperation(m_operationNames[2], FireRenderViewportOperation::POST_BLIT);

		m_operations[0] = m_preBlitOperation;
		m_operations[1] = new FireRenderViewportBlit(m_operationNames[1]);
		m_operations[2] = m_postBlitOperation;
		m_operations[3] = new FireRenderViewportUI(m_operationNames[3]);
		m_operations[4] = new MHWRender::MHUDRender();
		m_operations[5] = new MHWRender::MPresentTarget(m_operationNames[5]);
	}

	// Check that operations were created successfully.
	for (int i = 0; i < c_operationCount; i++)
		if (!m_operations[i])
			return false;

	// Update the viewport on the custom operations.
	m_preBlitOperation->setViewport(viewport);
	m_postBlitOperation->setViewport(viewport);

	// Operations setup complete.
	return true;
}

// -----------------------------------------------------------------------------
MStatus FireRenderOverride::setupViewport(const MString & panelName)
{
	// Get the viewport for the panel.
	FireRenderViewportManager& viewportManager = FireRenderViewportManager::instance();
	FireRenderViewport* viewport = viewportManager.getViewport(panelName);

    bool doReinit = false;
    if (viewport)
    {
        M3dView currentView;
        MStatus status = FireRenderViewport::FindMayaView(panelName, &currentView);
		if ((status == MStatus::kSuccess) && (currentView.widget() != viewport->getMayaWidget()) || 
			viewport->ShouldBeRecreated())
		{
			doReinit = true;
		}
    }

	// Create the viewport if required.
	if (!viewport || doReinit)
	{
		viewport = viewportManager.createViewport(panelName);
		if (!viewport) // creation failed
			return MStatus::kFailure;
	}

	// Create a new set of operations if required.
	if (!setupOperations(*viewport))
		return MStatus::kFailure;

	// Set up the viewport.
	MStatus status = viewport->setup();
	if (status != MStatus::kSuccess)
		return status;

	// Update the blit texture from the viewport.
	FireRenderViewportBlit* blit = static_cast<FireRenderViewportBlit*>(m_operations[1]);
	blit->updateTexture(*viewport);

	// Viewport setup complete.
	return MStatus::kSuccess;
}
