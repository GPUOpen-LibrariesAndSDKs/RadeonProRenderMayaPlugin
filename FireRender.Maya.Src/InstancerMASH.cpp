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
#include <InstancerMASH.h>
#include <FireRenderMeshMASH.h>

InstancerMASH::InstancerMASH(FireRenderContext* context, const MDagPath& dagPath) :
	FireRenderNode(context, dagPath),
	m_instancedObjectsCachedSize(0)
{
	GenerateInstances();
	RegisterCallbacks();
}

void InstancerMASH::RegisterCallbacks()
{
	AddCallback(MNodeMessage::addNodeDirtyPlugCallback(m.object, plugDirty_callback, this));
}

void InstancerMASH::Freshen()
{
	if (GetTargetObjects().empty())
	{
		return;
	}

	if (m_instancedObjects.empty())
	{
		GenerateInstances();
	}

	MTransformationMatrix instancerMatrix = MFnTransform(m.object).transformation();
	std::vector<MMatrix> matricesFromMASH = GetTransformMatrices();

	std::vector<MObject> targetObjects = GetTargetObjects();

	for (size_t i = 0; i < GetInstanceCount(); i++)
	{
		std::shared_ptr<FireRenderMeshMASH> instancedFRObject = m_instancedObjects.at(i);
		const FireRenderMesh& renderMesh = instancedFRObject->GetOriginalFRMeshinstancedObject();

		//Target node translation shouldn't affect the result 
		MTransformationMatrix targetNodeMatrix = MFnTransform(MFnDagNode(renderMesh.Object()).parent(0)).transformation();
		targetNodeMatrix.setTranslation({ 0., 0., 0. }, MSpace::kObject);

		MMatrix newTransform = targetNodeMatrix.asMatrix();
		newTransform *= matricesFromMASH.at(i);
		newTransform *= instancerMatrix.asMatrix();

		instancedFRObject->SetSelfTransform(newTransform);
		instancedFRObject->Rebuild();
		instancedFRObject->setDirty();
	}

	m_instancedObjectsCachedSize = GetInstanceCount();
}

void InstancerMASH::OnPlugDirty(MObject& node, MPlug& plug)
{
	(void) node;
	(void) plug;

	if (ShouldBeRecreated())
	{
		for (auto o : m_instancedObjects)
		{
			o.second->setVisibility(false);
		}
		m_instancedObjects.clear();
	}
	setDirty();
}

size_t InstancerMASH::GetInstanceCount() const
{
	MPlug instanceCountPlug(m.object, MFnDependencyNode(m.object).attribute("instanceCount"));
	MInt64 instanceCount;
	instanceCountPlug.getValue(instanceCount);
	return static_cast<size_t>(instanceCount);
}

std::vector<MObject> InstancerMASH::GetTargetObjects() const
{
	MFnDependencyNode instancerDagNode(m.object);
	MPlugArray dagConnections;
	instancerDagNode.getConnections(dagConnections);

	std::vector<MObject> targetObjects;
	targetObjects.reserve(GetInstanceCount());

	// Sometimes here appear empty input hierarchy nodes. 
	for (const MPlug connection : dagConnections)
	{
		const std::string name(connection.partialName().asChar());
		if (name.find("inh[") == std::string::npos)
		{
			continue;
		}

		MPlugArray connectedTo;
		connection.connectedTo(connectedTo, true, false);
		for (const MPlug instanceConnection : connectedTo)
		{
			MFnDagNode node(instanceConnection.node());
			MObject mesh = node.child(0);
			targetObjects.push_back(mesh);
		}
	}

	return std::move(targetObjects);
}

std::vector<MMatrix> InstancerMASH::GetTransformMatrices() const
{
	std::vector<MMatrix> result;
	result.reserve(GetInstanceCount());

	MFnDependencyNode instancerDagNode(m.object);
	MPlug plug(m.object, instancerDagNode.attribute("inp"));
	MObject data = plug.asMDataHandle().data();
	MFnArrayAttrsData arrayAttrsData(data);

	MVectorArray positionData = arrayAttrsData.getVectorData("position");
	MVectorArray rotationData = arrayAttrsData.getVectorData("rotation");
	MVectorArray scaleData = arrayAttrsData.getVectorData("scale");

	for (unsigned int i = 0; i < static_cast<unsigned int>(GetInstanceCount()); i++)
	{
		MVector position = positionData[i];
		MVector rotation = rotationData[i];
		MVector scale = scaleData[i];

		double rotationRadiansArray[] = { deg2rad(rotation.x), deg2rad(rotation.y), deg2rad(rotation.z) };
		double scaleArray[] = { scale.x, scale.y, scale.z };

		MTransformationMatrix transformFromMASH;
		transformFromMASH.setScale(scaleArray, MSpace::Space::kWorld);
		transformFromMASH.setRotation(rotationRadiansArray, MTransformationMatrix::RotationOrder::kXYZ);
		transformFromMASH.setTranslation(position, MSpace::Space::kWorld);

		result.push_back(transformFromMASH.asMatrix());
	}

	return result;
}

void InstancerMASH::GenerateInstances()
{
	//Generate unique uuid, because we can't use instancer uuid - it initiates infinite Freshen() on whole hierarchy
	std::vector<MUuid> uuidVector;

	//Generate instances with almost copy constructor with custom uuid passed
	std::vector<MObject> targetObjects = GetTargetObjects();

	MFnDependencyNode instancerDagNode(m.object);
	MPlug plug(m.object, instancerDagNode.attribute("inp"));
	MObject data = plug.asMDataHandle().data();
	MFnArrayAttrsData arrayAttrsData(data);

	// These two arrays are filled with doubles instead of ints in maya for some reason.
	MDoubleArray objectIndexArray = arrayAttrsData.getDoubleData("objectIndex");
	MDoubleArray idArray = arrayAttrsData.getDoubleData("id");

	for (unsigned int idArrayIndex = 0; idArrayIndex < idArray.length(); ++idArrayIndex)
	{
		size_t id = (size_t) idArray[idArrayIndex];
		size_t objectIndex = (size_t) objectIndexArray[idArrayIndex];

		if (objectIndex >= uuidVector.size())
		{
			MUuid uuid;
			uuid.generate();
			uuidVector.push_back(uuid);
		}

		FireRenderMesh* renderMesh = static_cast<FireRenderMesh*>(context()->getRenderObject(targetObjects.at(objectIndex)));
		auto instance = std::make_shared<FireRenderMeshMASH>(*renderMesh, uuidVector[objectIndex].asString().asChar(), m.object);
		m_instancedObjects[id] = instance;
	}
}

bool InstancerMASH::ShouldBeRecreated() const
{
	return
		(GetInstanceCount() != m_instancedObjectsCachedSize)
		||
		GetTargetObjects().empty();
}
