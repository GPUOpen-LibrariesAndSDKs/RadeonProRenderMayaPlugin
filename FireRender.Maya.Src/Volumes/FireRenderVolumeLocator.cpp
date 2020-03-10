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

