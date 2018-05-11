#pragma once

#include "ShadersManager.h"
#include "PhysicalLightData.h"
#include <maya/MPxLocatorNode.h>
#include <maya/MPxGeometryOverride.h>
#include <memory>

/**
* The RPR Physical Light locator contains the
* system attributes and is responsible for
* drawing the viewport representation.
*/

class FireRenderPhysicalLightLocator : public MPxLocatorNode
{
public:
	FireRenderPhysicalLightLocator() {};
	virtual ~FireRenderPhysicalLightLocator() {};

public:
	virtual MStatus compute(const MPlug& plug, MDataBlock& data) override;

	virtual void draw(
		M3dView & view,
		const MDagPath & path,
		M3dView::DisplayStyle style,
		M3dView::DisplayStatus status) override;

	virtual bool isBounded() const override;

	virtual MBoundingBox boundingBox() const override;

	static void* creator();

	static MStatus initialize();

public:
	static MTypeId	id;
	static MString drawDbClassification;
	static MString drawRegistrantId;
};
