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

#include "FireRenderLightCommon.h"

#include <maya/MFnTransform.h>
#include <maya/MEventMessage.h>
#include <maya/MNodeMessage.h>
#include <maya/MDGMessage.h>
#include <maya/MGlobal.h>
#include <maya/MViewport2Renderer.h>

#include <assert.h>

FireRenderLightCommon::FireRenderLightCommon() :
	m_modelEditorChangedCallback(0),
	m_aboutToDeleteCallback(0),
	m_nodeRemovedCallback(0),
	m_transformObject(MObject::kNullObj)
{
}

FireRenderLightCommon::~FireRenderLightCommon()
{
	if (m_nodeRemovedCallback != 0)
	{
		MDGMessage::removeCallback(m_nodeRemovedCallback);
	}

	if (m_aboutToDeleteCallback != 0)
	{
		MNodeMessage::removeCallback(m_aboutToDeleteCallback);
	}

	if (m_modelEditorChangedCallback != 0)
	{
		MNodeMessage::removeCallback(m_modelEditorChangedCallback);
	}
}

void FireRenderLightCommon::onAboutToDelete(MObject &node, MDGModifier& modifier, void* clientData)
{
	// this callback is called just before shape node is removed
	MStatus mstatus;
	MFnDependencyNode nodeFn(node, &mstatus);
	MString nodeName = nodeFn.name();

	// get transform node
	MFnDagNode dagfn(node);
	int parentCount = dagfn.parentCount();
	if (parentCount == 0)
		return;

	MObject parent = dagfn.parent(0, &mstatus);

	// ensure parent node has only one child
	// (shouldn't be deleted otherwise)
	MFnDagNode parentDag(parent, &mstatus);
	int childCount = parentDag.childCount(&mstatus);
	if (childCount > 1)
		return;

	// save parent node in light locator object
	FireRenderLightCommon* pLocator = static_cast<FireRenderLightCommon*>(clientData);
	pLocator->m_transformObject = parent;
	MString nodeTypeName(pLocator->GetNodeTypeName());

	pLocator->m_nodeRemovedCallback = MDGMessage::addNodeRemovedCallback(
		FireRenderLightCommon::onNodeRemoved,
		pLocator->GetNodeTypeName(),
		pLocator,
		&mstatus
	);
	assert(mstatus == MStatus::kSuccess);
}

void FireRenderLightCommon::onNodeRemoved(MObject &node, void *clientData)
{
	// this callback is called just after shape node is removed

	// get stored transform node
	FireRenderLightCommon* pparent = static_cast<FireRenderLightCommon*>(clientData);
	if (!pparent || pparent->m_transformObject == MObject::kNullObj)
	{
		return;
	}

	// delete transform node
	MStatus mstatus;
	mstatus = MGlobal::removeFromModel(pparent->m_transformObject);
	assert(mstatus == MStatus::kSuccess);
}

MSelectionMask FireRenderLightCommon::getShapeSelectionMask() const
{
	return MSelectionMask::kSelectLights;
}

void FireRenderLightCommon::OnModelEditorChanged(void* clientData)
{
	FireRenderLightCommon* light = static_cast<FireRenderLightCommon*>(clientData);
	if (light)
	{
		MHWRender::MRenderer::setGeometryDrawDirty(light->thisMObject());
	}
}

void FireRenderLightCommon::postConstructor()
{
	MStatus status;
	MObject mobj = thisMObject();

	m_aboutToDeleteCallback = MNodeMessage::addNodeAboutToDeleteCallback(mobj, FireRenderLightCommon::onAboutToDelete, this, &status);
	assert(status == MStatus::kSuccess);

	m_modelEditorChangedCallback = MEventMessage::addEventCallback("modelEditorChanged", OnModelEditorChanged, this, &status);
	assert(status == MStatus::kSuccess);
}

bool FireRenderLightCommon::excludeAsLocator() const
{
	return false;
}

