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

#include <vector>
#include "frWrap.h"

#include <maya/MCallbackIdArray.h>
#include <maya/MString.h>
#include <maya/MViewport2Renderer.h>
#include <maya/MDrawContext.h>
#include <maya/MFrameContext.h>
#include <maya/M3dView.h>
#include <maya/MUiMessage.h>
#include <maya/MShaderManager.h>
#include "Context/FireRenderContext.h"
#include <maya/MThreadAsync.h>
#include <maya/MTextureManager.h>


class FireRenderViewport;


// FireRenderViewportManager
// This class keep track of every fire render context running on different maya panels
class FireRenderViewportManager
{
public:

	// Singleton access
	static FireRenderViewportManager& instance();

public:

	// Get the viewport running on the panel
	FireRenderViewport* getViewport(const MString& panelName);

	// Get all the viewports
	std::vector<FireRenderViewport*> getViewports();

	// Create a viewport on the panel
	FireRenderViewport* createViewport(const MString& panelName);

	// Remove a the viewport from the panel
	void removeViewport(const MString& panelName, bool destroyed = false);

	// Changed renderer callback
	static void changedRenderer(const MString &panelName, const MString &oldRenderer, const MString &newRenderer, void *clientData);

	// Changed renderer override callback
	static void changedRendererOverride(const MString &panelName, const MString &oldRenderer, const MString &newRenderer, void *clientData);

	// Camera changed callback
	static void cameraChanged(const MString &str, MObject &node, void *clientData);

	// Panel destroyed callback
	static void mayaPanelDestroyed(const MString &str, void *clientData);

	// Panel destroyed callback
	static void postRenderMsgCallback(const MString &str, void *clientData);

	// register callbacks
	void registerCallbacks(const MString& panelName);

	// remove callbacks
	void removeCallbacks(const MString& panelName);

	// clear map
	void clear();

private:

	// Constructor
	FireRenderViewportManager();

	// Destructor
	~FireRenderViewportManager();

	// Copy constructor
	FireRenderViewportManager(const FireRenderViewportManager&) = delete;

	// Assign operator
	FireRenderViewportManager& operator=(const FireRenderViewportManager&) = delete;

private:

	// viewport maps
	std::map<std::string, std::shared_ptr<FireRenderViewport>> m_viewports;

	// changed viewport callbacks map
	std::map<std::string, std::vector<MCallbackId> > m_changedViewportRendererCallbacks;

};
