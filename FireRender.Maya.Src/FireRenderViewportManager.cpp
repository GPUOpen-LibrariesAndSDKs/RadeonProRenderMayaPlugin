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
#include <cassert>

#include <maya/MNodeMessage.h>
#include <maya/MDagPath.h>
#include <maya/M3dView.h>
#include <maya/MFnCamera.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MImage.h>
#include <maya/MFileIO.h>
#include <maya/MGlobal.h>
#include "Context/FireRenderContext.h"
#include "FireRenderViewport.h"
#include "FireRenderViewportManager.h"
#include <maya/MAnimControl.h>
#include <thread>


FireRenderViewportManager::FireRenderViewportManager()
{

}

FireRenderViewportManager::~FireRenderViewportManager()
{

}

FireRenderViewportManager& FireRenderViewportManager::instance()
{
	static FireRenderViewportManager data;
	return data;
}

void FireRenderViewportManager::clear()
{
	m_viewports.clear();
}

FireRenderViewport* FireRenderViewportManager::getViewport(const MString& panelName)
{
	auto it = m_viewports.find(panelName.asChar());
	if (it != m_viewports.end())
		return it->second.get();

	return nullptr;
}

std::vector<FireRenderViewport*> FireRenderViewportManager::getViewports()
{
	std::vector<FireRenderViewport*> viewports;
	for (auto& it : m_viewports)
	{
		if (auto vp = it.second.get())
			viewports.push_back(vp);
	}
	return viewports;
}

FireRenderViewport* FireRenderViewportManager::createViewport(const MString& panelName)
{
	// Delete any existing viewport on the panel.
	removeViewport(panelName);

	// Create and map the viewport and register callbacks.
	auto viewport = std::make_shared<FireRenderViewport>(panelName);
	if (viewport->createFailed())
		return NULL;

	m_viewports[panelName.asChar()] = viewport;
	registerCallbacks(panelName);

	return viewport.get();
}

void FireRenderViewportManager::removeViewport(const MString& panelName, bool destroyed)
{
	auto it = m_viewports.find(panelName.asChar());
	if (it != m_viewports.end())
	{
		it->second->removed(destroyed);
		removeCallbacks(panelName);
		m_viewports.erase(it);
	}
}

void FireRenderViewportManager::changedRenderer(const MString &panelName, const MString &oldRenderer, const MString &newRenderer, void *clientData)
{
	if (oldRenderer == "vp2Renderer")
	{
		//delete current viewport
		FireRenderViewportManager& panelManager = FireRenderViewportManager::instance();
		panelManager.removeViewport(panelName.asChar());
	}
}

void FireRenderViewportManager::changedRendererOverride(const MString &panelName, const MString &oldRenderer, const MString &newRenderer, void *clientData)
{
	if (oldRenderer == "FireRenderOverride")
	{
		//delete current viewport
		FireRenderViewportManager& panelManager = FireRenderViewportManager::instance();
		panelManager.removeViewport(panelName.asChar());
	}
}

void FireRenderViewportManager::cameraChanged(const MString &panelName, MObject &camera, void *clientData)
{
	FireRenderViewportManager& panelManager = FireRenderViewportManager::instance();
	if (auto viewport = panelManager.getViewport(panelName.asChar()))
	{
		MFnDagNode cameraNode(camera);
		MDagPath cameraPath;
		cameraNode.getPath(cameraPath);
		if (cameraPath.isValid())
			viewport->cameraChanged(cameraPath);
	}
}

void FireRenderViewportManager::mayaPanelDestroyed(const MString &panelName, void *clientData)
{
	FireRenderViewportManager& panelManager = FireRenderViewportManager::instance();
	panelManager.removeViewport(panelName.asChar(), true);
}

void FireRenderViewportManager::postRenderMsgCallback(const MString& panelName, void* clientData)
{
	FireRenderViewportManager& panelManager = FireRenderViewportManager::instance();
	if (auto viewport = panelManager.getViewport(panelName.asChar()))
		viewport->refresh();
}

void FireRenderViewportManager::registerCallbacks(const MString& panelName)
{
	if (m_changedViewportRendererCallbacks.count(panelName.asChar()) == 0)
	{
		auto& cb = m_changedViewportRendererCallbacks[panelName.asChar()];

		cb.push_back(MUiMessage::add3dViewRendererChangedCallback(
			panelName, FireRenderViewportManager::changedRenderer, NULL, NULL
		));

		cb.push_back(MUiMessage::add3dViewRenderOverrideChangedCallback(
			panelName, FireRenderViewportManager::changedRendererOverride, NULL, NULL
		));

		cb.push_back(MUiMessage::addCameraChangedCallback(
			panelName, FireRenderViewportManager::cameraChanged, NULL, NULL
		));

		cb.push_back(MUiMessage::add3dViewDestroyMsgCallback(
			panelName, FireRenderViewportManager::mayaPanelDestroyed, NULL, NULL));

		cb.push_back(MUiMessage::add3dViewPostRenderMsgCallback(
			panelName, FireRenderViewportManager::postRenderMsgCallback, NULL, NULL));
	}
}

void FireRenderViewportManager::removeCallbacks(const MString& panelName)
{
	auto it = m_changedViewportRendererCallbacks.find(panelName.asChar());
	if (it != m_changedViewportRendererCallbacks.end())
	{
		for (auto callbackId : it->second)
			MMessage::removeCallback(callbackId);

		m_changedViewportRendererCallbacks.erase(it);
	}
}
