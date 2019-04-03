#include <maya/MStatus.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MDagModifier.h>

#include "FireRenderVolumeLocator.h"
#include "VolumeAttributes.h"
#include "FireMaya.h"

MTypeId FireRenderVolumeLocator::id(FireMaya::TypeId::FireRenderVolumeLocator);
MString FireRenderVolumeLocator::drawDbClassification("drawdb/geometry/FireRenderVolumeLocator");
MString FireRenderVolumeLocator::drawRegistrantId("FireRenderVolumeNode");

FireRenderVolumeLocator::FireRenderVolumeLocator() :
	m_attributeChangedCallback(0)
{
}

FireRenderVolumeLocator::~FireRenderVolumeLocator()
{
	if (m_attributeChangedCallback != 0)
	{
		MNodeMessage::removeCallback(m_attributeChangedCallback);
	}
}

void FireRenderVolumeLocator::postConstructor()
{
	MStatus status;
	MObject mobj = thisMObject();
	m_attributeChangedCallback = MNodeMessage::addAttributeChangedCallback(mobj, FireRenderVolumeLocator::onAttributeChanged, this, &status);
	assert(status == MStatus::kSuccess);
}

MStatus FireRenderVolumeLocator::compute(const MPlug& plug, MDataBlock& data)
{
	return MS::kUnknownParameter;
}

MStatus FireRenderVolumeLocator::initialize()
{
	RPRVolumeAttributes::Initialize();

	return MStatus::kSuccess;
}

void FireRenderVolumeLocator::draw(
	M3dView& view,
	const MDagPath& dag,
	M3dView::DisplayStyle style,
	M3dView::DisplayStatus status)
{
	// _TODO Add drawing with OpenGL for Legacy Viewport in Maya

}

bool FireRenderVolumeLocator::isBounded() const
{
	return true;
}

MBoundingBox FireRenderVolumeLocator::boundingBox() const
{
	MPoint corner1(-1.0, -1.0, -1.0);
	MPoint corner2(1.0, 1.0, 1.0);

	return MBoundingBox(corner1, corner2);
}

void* FireRenderVolumeLocator::creator()
{
	return new FireRenderVolumeLocator();
}

void FireRenderVolumeLocator::onAttributeChanged(MNodeMessage::AttributeMessage msg, MPlug &plug, MPlug &otherPlug, void *clientData)
{
	FireRenderVolumeLocator* volumeLocatorNode = static_cast<FireRenderVolumeLocator*> (clientData);

	if (!(msg | MNodeMessage::AttributeMessage::kAttributeSet))
	{
		return;
	}

	// change displayed grid
}

