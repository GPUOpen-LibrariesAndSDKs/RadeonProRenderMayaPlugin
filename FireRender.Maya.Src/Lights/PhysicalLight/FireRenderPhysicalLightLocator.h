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

#include "ShadersManager.h"
#include "PhysicalLightData.h"
#include <maya/MPxLocatorNode.h>
#include <maya/MPxGeometryOverride.h>
#include <memory>

/**
* The RPR Physical Light locator contains the
* system attributes.
*/

class FireRenderPhysicalLightLocator : public MPxLocatorNode
{
public:
	FireRenderPhysicalLightLocator();
	virtual ~FireRenderPhysicalLightLocator();

	virtual MStatus compute(const MPlug& plug, MDataBlock& data) override;

	virtual void draw(
		M3dView & view,
		const MDagPath & path,
		M3dView::DisplayStyle style,
		M3dView::DisplayStatus status) override;

	virtual bool isBounded() const override;

	virtual MBoundingBox boundingBox() const override;

	static void* creator();

	void postConstructor() override;

	static MStatus initialize();

public:
	static MTypeId	id;
	static MString drawDbClassification;
	static MString drawDbGeomClassification;
	static MString drawRegistrantId;

private:
	MCallbackId m_attributeChangedCallback;
	MCallbackId m_selectionChangedCallback;

private:
	void SubscribeSelectionChangedEvent(bool subscribe = true);
	void MakeSelectedMeshAsLight();

	static void onSelectionChanged(void *clientData);
	static void onAttributeChanged(MNodeMessage::AttributeMessage msg, MPlug &plug, MPlug &otherPlug, void *clientData);
};
