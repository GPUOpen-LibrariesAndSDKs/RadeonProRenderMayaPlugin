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
