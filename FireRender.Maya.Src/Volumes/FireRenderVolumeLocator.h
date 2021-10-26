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
#include <maya/MPxGeometryOverride.h>
#include <maya/MMessage.h>
#include <maya/MNodeMessage.h>
#include <maya/MEventMessage.h>
#include <memory>
#include <map>

/**
* The RPR Volume locator contains the
* system attributes.
*/

class VDBGridSize;

class FireRenderVolumeLocator : public MPxLocatorNode
{
public:
	FireRenderVolumeLocator();
	virtual ~FireRenderVolumeLocator();

	virtual MStatus compute(const MPlug& plug, MDataBlock& data) override;

	virtual MStatus setDependentsDirty(const MPlug& plugBeingDirtied, MPlugArray&	affectedPlugs) override;

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

	using GridParams = std::map<std::string, VDBGridSize>;

public:
	static MTypeId id;
	static MString drawDbClassification;
	static MString drawRegistrantId;

private:
	MCallbackId m_attributeChangedCallback;
	MCallbackId m_timeChangedCallback;

	GridParams m_gridParams;

private:
	static void onAttributeChanged(MNodeMessage::AttributeMessage msg, MPlug &plug, MPlug &otherPlug, void *clientData);
	static void onTimeChanged(void* clientData);
};
