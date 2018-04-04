#include <maya/MFnEnumAttribute.h>
#include <maya/MFnNumericAttribute.h>

#include "FireRenderPhysicalLightLocator.h"
#include "PhysicalLightAttributes.h"
#include "FireMaya.h"

MTypeId FireRenderPhysicalLightLocator::id(FireMaya::TypeId::FireRenderPhysicalLightLocator);
MString FireRenderPhysicalLightLocator::drawDbClassification("drawdb/geometry/FireRenderPhysicalLightLocator");
MString FireRenderPhysicalLightLocator::drawRegistrantId("FireRenderPhysicalLightNode");

MStatus FireRenderPhysicalLightLocator::compute(const MPlug& plug, MDataBlock& data)
{
	return MS::kUnknownParameter;
}

MStatus FireRenderPhysicalLightLocator::initialize()
{
	PhysicalLightAttributes::Initialize();

	return MStatus::kSuccess;
}

void FireRenderPhysicalLightLocator::draw(
	M3dView& view,
	const MDagPath& dag,
	M3dView::DisplayStyle style,
	M3dView::DisplayStatus status)
{
	// _TODO Add drawing with OpenGL for Legacy Viewport in Maya
	
}

bool FireRenderPhysicalLightLocator::isBounded() const
{
	return true;
}

MBoundingBox FireRenderPhysicalLightLocator::boundingBox() const
{
	MPoint corner1(-1.0, -1.0, -1.0);
	MPoint corner2(1.0, 1.0, 1.0);
	return MBoundingBox(corner1, corner2);
}

void* FireRenderPhysicalLightLocator::creator()
{
	return new FireRenderPhysicalLightLocator();
}
