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
#include <maya/MFnStringArrayData.h>
#include <maya/MArrayDataBuilder.h>

#include "FireRenderVolumeLocator.h"
#include "VolumeAttributes.h"
#include "FireMaya.h"

MTypeId FireRenderVolumeLocator::id(FireMaya::TypeId::FireRenderVolumeLocator);
MString FireRenderVolumeLocator::drawDbClassification("drawdb/geometry/FireRenderVolumeLocator");
MString FireRenderVolumeLocator::drawRegistrantId("FireRenderVolumeNode");

FireRenderVolumeLocator::FireRenderVolumeLocator() 
	: m_attributeChangedCallback(0)
	, m_timeChangedCallback(0)
{
}

FireRenderVolumeLocator::~FireRenderVolumeLocator()
{
	if (m_attributeChangedCallback != 0)
	{
		MNodeMessage::removeCallback(m_attributeChangedCallback);
	}
	if (m_timeChangedCallback != 0)
	{
		MEventMessage::removeCallback(m_timeChangedCallback);
	}
}

void SetDefaultStringArrayAttrValue(MPlug& plug, MDataBlock& block)
{
	MStatus status;

	MDataHandle wHandle = plug.constructHandle(block);
	MArrayDataHandle arrayHandle(wHandle, &status);
	MArrayDataBuilder arrayBuilder = arrayHandle.builder(&status);

	MDataHandle handle = arrayBuilder.addElement(0, &status);
	MString val = MString("Not used");
	handle.set(val);

	status = arrayHandle.set(arrayBuilder);

	plug.setValue(wHandle);
	plug.destructHandle(wHandle);
}

void FireRenderVolumeLocator::postConstructor()
{
	MStatus status;
	MObject mobj = thisMObject();
	m_attributeChangedCallback = MNodeMessage::addAttributeChangedCallback(mobj, FireRenderVolumeLocator::onAttributeChanged, this, &status);
	assert(status == MStatus::kSuccess);
	m_timeChangedCallback = MEventMessage::addEventCallback("timeChanged", FireRenderVolumeLocator::onTimeChanged, this, &status);
	assert(status == MStatus::kSuccess);

	setMPSafe(true);

	status = setExistWithoutInConnections(true);
	CHECK_MSTATUS(status);

	// set default value for loaded grids list
	MPlug lgPlug(thisMObject(), RPRVolumeAttributes::loadedGrids);
	MDataBlock block = this->forceCache();
	SetDefaultStringArrayAttrValue(lgPlug, block);

	// set default values for ramps
	// - if we don't then there will be no default control point and Maya will display a ui error
	// - this happens despite api documentation saying that default control point is created automatically - it's not.
	// - Since we need to set these values anyway, we set them to what user most likely wants
	MPlug albedoRampPlug(thisMObject(), RPRVolumeAttributes::albedoRamp);
	const float albedoSrc[][4] = { 0.5f, 0.5f, 0.5f, 1.0f, 0.5f, 0.5f, 0.5f, 1.0f };
	MColorArray albedoValues(albedoSrc, 2);
	SetRampValues(albedoRampPlug, albedoValues);

	MPlug emissionRampPlug(thisMObject(), RPRVolumeAttributes::emissionRamp);
	const float emissionSrc[][4] = {
		 0.0f, 0.0f, 0.0f, 1.0f,
		 0.0f, 0.0f, 0.0f, 1.0f,
		 0.2f, 0.0f, 0.0f, 1.0f,
		 0.6f, 0.2f, 0.0f, 1.0f,
		 0.6f, 1.0f, 0.0f, 1.0f,
		 1.0f, 1.0f, 1.0f, 1.0f
	};
	MColorArray emissionValues(emissionSrc, 6);
	SetRampValues(emissionRampPlug, emissionValues);

	MPlug densityRampPlug(thisMObject(), RPRVolumeAttributes::densityRamp);
	const float densitySrc[] = { 0.5f, 0.7f, 0.8f, 1.0f };
	MFloatArray densityValues(densitySrc, 4);
	SetRampValues(densityRampPlug, densityValues);
}

MStatus FireRenderVolumeLocator::setDependentsDirty(const MPlug& plugBeingDirtied,	MPlugArray&	affectedPlugs)
{
	std::string name = plugBeingDirtied.partialName().asChar();
	if (name.find("loag") == std::string::npos)
	{
		return MS::kSuccess;
	}

	if (plugBeingDirtied.attribute() != RPRVolumeAttributes::loadedGrids) 
	{
		return MS::kSuccess;
	}

	MObject thisNode = thisMObject();
	MPlug palbedoSelectedGrid(thisNode, RPRVolumeAttributes::albedoSelectedGrid);
	affectedPlugs.append(palbedoSelectedGrid);

	return(MS::kSuccess);
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
	// get the size
	MObject thisNode = thisMObject();
	MFnDependencyNode fnNode(thisNode);

	volatile std::string thisNodeName = fnNode.name().asChar();

	MDataHandle volumeGirdSizes = RPRVolumeAttributes::GetVolumeGridDimentions(fnNode);
	short nx = volumeGirdSizes.asShort3()[0];
	short ny = volumeGirdSizes.asShort3()[1];
	short nz = volumeGirdSizes.asShort3()[2];

	MDataHandle volumeVoxelSizes = RPRVolumeAttributes::GetVolumeVoxelSize(fnNode);
	double vx = volumeVoxelSizes.asDouble3()[0];
	double vy = volumeVoxelSizes.asDouble3()[1];
	double vz = volumeVoxelSizes.asDouble3()[2];

	const float sizeX = nx * vx;
	const float sizeY = ny * vy;
	const float sizeZ = nz * vz;

	MPoint corner1(-sizeX/2.0f, -sizeY/2.0f, -sizeZ/2.0f);
	MPoint corner2(sizeX/2.0f, sizeY/2.0f, sizeZ/2.0f);

	return MBoundingBox(corner1, corner2);
}

void* FireRenderVolumeLocator::creator()
{
	return new FireRenderVolumeLocator();
}

void FireRenderVolumeLocator::onAttributeChanged(MNodeMessage::AttributeMessage msg, MPlug &plug, MPlug &otherPlug, void *clientData)
{
	if (!(msg | MNodeMessage::AttributeMessage::kAttributeSet))
	{
		return;
	}

	FireRenderVolumeLocator* rprVolumeLocatorNode = static_cast<FireRenderVolumeLocator*> (clientData);
	MObject mobj = rprVolumeLocatorNode->thisMObject();

	if ((plug == RPRVolumeAttributes::vdbFile) || (plug == RPRVolumeAttributes::namingSchema))
	{
		RPRVolumeAttributes::SetupVolumeFromFile(mobj, rprVolumeLocatorNode->m_gridParams, rprVolumeLocatorNode->m_maxGridParams, true);
	}

	else if ((plug == RPRVolumeAttributes::albedoSelectedGrid) || (plug == RPRVolumeAttributes::emissionSelectedGrid) || (plug == RPRVolumeAttributes::densitySelectedGrid))
	{
		auto& tmpGridParams = (rprVolumeLocatorNode->m_maxGridParams.size() > 0) ? 
			rprVolumeLocatorNode->m_maxGridParams : 
			rprVolumeLocatorNode->m_gridParams;
		RPRVolumeAttributes::SetupGridSizeFromFile(mobj, plug, tmpGridParams);
	}
}

void FireRenderVolumeLocator::onTimeChanged(void* clientData)
{
	if (clientData == nullptr)
	{
		return;
	}
	FireRenderVolumeLocator* rprVolumeLocatorNode = static_cast<FireRenderVolumeLocator*> (clientData);
	MObject mobj = rprVolumeLocatorNode->thisMObject();

	MFnDependencyNode depNode(mobj);
	MGlobal::executeCommand(MString("dgdirty " + depNode.name()));

	RPRVolumeAttributes::SetupVolumeFromFile(mobj, rprVolumeLocatorNode->m_gridParams, rprVolumeLocatorNode->m_maxGridParams, false);
}

