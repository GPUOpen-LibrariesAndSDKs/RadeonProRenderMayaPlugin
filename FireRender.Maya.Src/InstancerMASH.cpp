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
#include <maya/MItDag.h>

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
		std::vector<std::shared_ptr<FireRenderMeshMASH>>& instancedObjects = m_instancedObjects.at(i);
		for (auto& instancedFRObject : instancedObjects)
		{
			const FireRenderMesh& renderMesh = instancedFRObject->GetOriginalFRMeshinstancedObject();

			//Target node translation shouldn't affect the result 
			// translation of shape in group however should
			MFnDagNode meshTransformNode(MFnDagNode(renderMesh.Object()).parent(0));
			MTransformationMatrix targetNodeMatrix = MFnTransform(meshTransformNode.object()).transformation();
			MFnDagNode groupTransformNode(meshTransformNode.parent(0));
			if (groupTransformNode.name() != "world")
			{
				MTransformationMatrix groupNodeMatrix = MFnTransform(groupTransformNode.object()).transformation();
				groupNodeMatrix.setTranslation({ 0., 0., 0. }, MSpace::kObject);
				MMatrix groupTransform = groupNodeMatrix.asMatrix();
				MMatrix meshTransform = targetNodeMatrix.asMatrix();

				targetNodeMatrix = meshTransform * groupTransform;
			}
			else
			{
				targetNodeMatrix.setTranslation({ 0., 0., 0. }, MSpace::kObject);
			}

			MMatrix newTransform = targetNodeMatrix.asMatrix();
			newTransform *= matricesFromMASH.at(i);
			newTransform *= instancerMatrix.asMatrix();

			instancedFRObject->SetSelfTransform(newTransform);
			instancedFRObject->Rebuild();
			instancedFRObject->setDirty();
		}
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
			for (auto it = o.second.begin(); it != o.second.end(); ++it)
				(*it)->setVisibility(false);
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
			targetObjects.push_back(instanceConnection.node());
		}
	}

	return std::move(targetObjects);
}

std::vector<MObject> GetShapesFromNode(MObject node)
{
	MStatus status;

	std::vector<MObject> out;

	MItDag itDag(MItDag::kDepthFirst, MFn::kMesh, &status);
	if (MStatus::kSuccess != status)
		MGlobal::displayError("MItDag::MItDag");

	status = itDag.reset(node, MItDag::kDepthFirst, MFn::kMesh);
	if (MStatus::kSuccess != status)
		MGlobal::displayError("MItDag::MItDag");

	for (; !itDag.isDone(); itDag.next())
	{
		MObject mesh = itDag.currentItem(&status);

		if (MStatus::kSuccess != status)
			continue;

		out.push_back(mesh);
	}

	return out;
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
	MFnDependencyNode instancerDagNode(m.object);
	MPlug plug(m.object, instancerDagNode.attribute("inp"));
	MObject data = plug.asMDataHandle().data();
	MFnArrayAttrsData arrayAttrsData(data);

	// These two arrays are filled with doubles instead of ints in maya for some reason.
	MDoubleArray objectIndexArray = arrayAttrsData.getDoubleData("objectIndex");
	MDoubleArray idArray = arrayAttrsData.getDoubleData("id");

	//Generate unique uuid, because we can't use instancer uuid - it initiates infinite Freshen() on whole hierarchy
	std::vector<std::vector<MUuid>> uuidVectors;
	uuidVectors.resize(objectIndexArray.length());

	//Get list of objects that are instanced by mash
	std::vector<MObject> targetObjects = GetTargetObjects();

	std::map<size_t, std::vector<MObject>> shapesCache;
	for (unsigned int idx = 0; idx < objectIndexArray.length(); ++idx)
	{
		size_t objectIndex = (size_t)objectIndexArray[idx];
		shapesCache[(size_t)objectIndex] = GetShapesFromNode(targetObjects.at((size_t)objectIndex));
	}

	// generate objects
	for (unsigned int idArrayIndex = 0; idArrayIndex < idArray.length(); ++idArrayIndex)
	{
		// get object(s) to be instanced
		size_t objectIndex = (size_t)objectIndexArray[idArrayIndex];
		auto it = shapesCache.find(objectIndex);
		assert(it != shapesCache.end());
		if (it == shapesCache.end())
			continue;

		std::vector<MObject>& shapesToBeCreated = it->second;

		// generate objectId(s) for new objects if neceessary
		std::vector<MUuid>& uuidVector = uuidVectors[objectIndex];
		while (shapesToBeCreated.size() != uuidVector.size())
		{
			MUuid uuid;
			uuid.generate();
			uuidVector.push_back(uuid);
		}

		// proceeed with generating objects
		size_t id = (size_t)idArray[idArrayIndex];
		size_t currObjIdx = 0;
		for (MObject& tmpObj : shapesToBeCreated)
		{
			FireRenderObject* pFoundObj = context()->getRenderObject(tmpObj);
			if (!pFoundObj)
				continue;

			FireRenderMesh* renderMesh = static_cast<FireRenderMesh*>(pFoundObj);
			assert(renderMesh);

			auto instance = std::make_shared<FireRenderMeshMASH>(
				*renderMesh, 
				uuidVector[currObjIdx].asString().asChar(), 
				m.object);
			m_instancedObjects[id].push_back(instance);

			currObjIdx++;
		}
	}
}

bool InstancerMASH::ShouldBeRecreated() const
{
	return
		(GetInstanceCount() != m_instancedObjectsCachedSize)
		||
		GetTargetObjects().empty();
}
