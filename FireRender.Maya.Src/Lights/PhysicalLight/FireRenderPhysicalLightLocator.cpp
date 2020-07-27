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
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnNumericAttribute.h>

#include <maya/MSelectionList.h>
#include <maya/MDagModifier.h>
#include <maya/MFnMesh.h>
#include <maya/MFloatPointArray.h>
#include <maya/MFnTransform.h>
#include <maya/MEventMessage.h>
#include <maya/MHWGeometryUtilities.h>
#include <maya/MDGMessage.h>

#include "FireRenderPhysicalLightLocator.h"
#include "PhysicalLightAttributes.h"
#include "FireMaya.h"

MTypeId FireRenderPhysicalLightLocator::id(FireMaya::TypeId::FireRenderPhysicalLightLocator);
MString FireRenderPhysicalLightLocator::drawDbClassification("drawdb/geometry/light/FireRenderPhysicalLightLocator:drawdb/light/directionalLight:light");
MString FireRenderPhysicalLightLocator::drawDbGeomClassification("drawdb/geometry/light/FireRenderPhysicalLightLocator");
MString FireRenderPhysicalLightLocator::drawRegistrantId("FireRenderPhysicalLightNode");

FireRenderPhysicalLightLocator::FireRenderPhysicalLightLocator() :
	m_attributeChangedCallback(0),
	m_selectionChangedCallback(0)
{
}

FireRenderPhysicalLightLocator::~FireRenderPhysicalLightLocator()
{
	if (m_attributeChangedCallback != 0)
	{
		MNodeMessage::removeCallback(m_attributeChangedCallback);
	}

	SubscribeSelectionChangedEvent(false);
}

void FireRenderPhysicalLightLocator::postConstructor()
{
	FireRenderLightCommon::postConstructor();

	MStatus status;
	MObject mobj = thisMObject();
	
	m_attributeChangedCallback = MNodeMessage::addAttributeChangedCallback(mobj, FireRenderPhysicalLightLocator::onAttributeChanged, this, &status);
	assert(status == MStatus::kSuccess);

	// rename node
	MFnDependencyNode nodeFn(thisMObject());
	nodeFn.setName("RPRPhysicalLightShape#");

	MFnDagNode dagNode(thisMObject());
	MObject parent = dagNode.parent(0, &status);
	CHECK_MSTATUS(status);

	MFnDependencyNode parentFn(parent);
	parentFn.setName("RPRPhysicalLight#");
}

const MString FireRenderPhysicalLightLocator::GetNodeTypeName(void) const
{
	return "RPRPhysicalLight";
}

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

void FireRenderPhysicalLightLocator::onAttributeChanged(MNodeMessage::AttributeMessage msg, MPlug &plug, MPlug &otherPlug, void *clientData)
{
	FireRenderPhysicalLightLocator* physicalLightLocatorNode = static_cast<FireRenderPhysicalLightLocator*> (clientData);

	if (! (msg | MNodeMessage::AttributeMessage::kAttributeSet) )
	{
		return;
	}

	if (plug == PhysicalLightAttributes::areaLightSelectingMesh)
	{
		physicalLightLocatorNode->SubscribeSelectionChangedEvent();
	}
}

void FireRenderPhysicalLightLocator::onSelectionChanged(void *clientData)
{
	FireRenderPhysicalLightLocator* physicalLightLocatorNode = static_cast<FireRenderPhysicalLightLocator*> (clientData);

	physicalLightLocatorNode->MakeSelectedMeshAsLight();
	physicalLightLocatorNode->SubscribeSelectionChangedEvent(false);
}

void FireRenderPhysicalLightLocator::SubscribeSelectionChangedEvent(bool subscribe)
{
	if (subscribe)
	{
		if (m_selectionChangedCallback == 0)
		{
			m_selectionChangedCallback = MEventMessage::addEventCallback("SelectionChanged", onSelectionChanged, this);
		}
	}
	else
	{
		if (m_selectionChangedCallback != 0)
		{
			MNodeMessage::removeCallback(m_selectionChangedCallback);
		}

		m_selectionChangedCallback = 0;
	}
}

void FireRenderPhysicalLightLocator::MakeSelectedMeshAsLight()
{
	MSelectionList sList;

	MGlobal::getActiveSelectionList(sList);

	MObject nodeObject = thisMObject();

	bool changed = false;
	for (unsigned int i = 0; i < sList.length(); i++)
	{
		MDagPath path;

		sList.getDagPath(i, path);

		if (!path.node().hasFn(MFn::kTransform))
		{
			continue;
		}

		MObject newParent = path.node();
		path.extendToShape();

		MHWRender::DisplayStatus displayStatus = MHWRender::MGeometryUtilities::displayStatus(path);

		if ((displayStatus == MHWRender::kLead) && path.node().hasFn(MFn::kMesh))
		{
			MObject shapeObject = path.node();
			MDagModifier dagModifier;

			// reparent light to mesh
			MFnDagNode lightDagNode(nodeObject);
			MObject lightTransformObj = lightDagNode.parent(0);

			dagModifier.reparentNode(nodeObject, newParent);

			dagModifier.doIt();

			MDGModifier dgModifier;
			MFnTransform lightTransform(lightTransformObj);
			if (lightTransform.childCount() == 0)
			{
				dgModifier.deleteNode(lightTransformObj);
				dgModifier.doIt();
			}

			MFnMesh meshNode(shapeObject);

			MPlug plugMeshVisibility = meshNode.findPlug("visibility", false);
			if (!plugMeshVisibility.isNull())
			{
				plugMeshVisibility.setBool(false);
			}

			MFnDependencyNode thisDepNode(nodeObject);
			MPlug plug = thisDepNode.findPlug(PhysicalLightAttributes::areaLightMeshSelectedName, false);

			assert(!plug.isNull());

			MString strPath = path.fullPathName();
			plug.setString(strPath);

			changed = true;

			break;
		}
	}

	SubscribeSelectionChangedEvent(false);

	if (!changed)
	{
		MGlobal::displayWarning("Please select a mesh");
	}
}
