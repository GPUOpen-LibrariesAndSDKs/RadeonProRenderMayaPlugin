#pragma once

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

#include <maya/MPxLocatorNode.h>
#include <maya/MMessage.h>

class FireRenderLightCommon : public MPxLocatorNode
{
public:
	FireRenderLightCommon();
	virtual ~FireRenderLightCommon();

	virtual void postConstructor() override;
	virtual bool excludeAsLocator() const override;

	MSelectionMask getShapeSelectionMask(void) const override;

protected:
	static void onNodeRemoved(MObject &node, void *clientData);
	static void onAboutToDelete(MObject &node, MDGModifier& modifier, void* clientData);
	static void OnModelEditorChanged(void* clientData);
	static void OnSceneClose(void* clientData);

	virtual const MString GetNodeTypeName(void) const = 0;

protected:
	MCallbackId m_modelEditorChangedCallback;
	MCallbackId m_aboutToDeleteCallback;
	MCallbackId m_nodeRemovedCallback;
	MCallbackId m_newFileCallback;
	MCallbackId m_openFileCallback;

	MObject m_transformObject;
};
