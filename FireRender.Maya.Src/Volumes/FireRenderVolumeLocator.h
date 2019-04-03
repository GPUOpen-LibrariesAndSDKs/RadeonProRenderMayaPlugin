#pragma once

#include <maya/MPxLocatorNode.h>
#include <maya/MPxGeometryOverride.h>
#include <maya/MMessage.h>
#include <maya/MNodeMessage.h>
#include <memory>

/**
* The RPR Volume locator contains the
* system attributes.
*/

class FireRenderVolumeLocator : public MPxLocatorNode
{
public:
	FireRenderVolumeLocator();
	virtual ~FireRenderVolumeLocator();

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
	static MTypeId id;
	static MString drawDbClassification;
	static MString drawRegistrantId;

private:
	MCallbackId m_attributeChangedCallback;

private:
	static void onAttributeChanged(MNodeMessage::AttributeMessage msg, MPlug &plug, MPlug &otherPlug, void *clientData);
};
