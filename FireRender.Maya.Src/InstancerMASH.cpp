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
#include <maya/MEulerRotation.h>

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

void InstancerMASH::Freshen(bool shouldCalculateHash)
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

InstancerMASH::MASHContext::MASHContext()
	: m_isValid(false)
	, m_objectIndexArray()
	, m_idArray()
	, m_positionArray()
	, m_rotationArray()
	, m_scaleArray()
	, m_shapesCache()
	, m_uuidVectors()
{}

bool InstancerMASH::MASHContext::Init(MFnArrayAttrsData& arrayAttrsData, const InstancerMASH* pInstancer)
{
	assert(pInstancer);
	if (!pInstancer)
		return false;

	MStatus res;
	MFnArrayAttrsData::Type arrType;

	// this data is essential!
	bool ojectIndexArrayExists = arrayAttrsData.checkArrayExist("objectIndex", arrType, &res);
	assert(res == MStatus::kSuccess);
	assert(ojectIndexArrayExists); 
	if (!ojectIndexArrayExists)
		return false;

	m_objectIndexArray = arrayAttrsData.getDoubleData("objectIndex", &res);
	assert(res == MStatus::kSuccess);

	// Generate unique uuid, because we can't use instancer uuid - it initiates infinite Freshen() on whole hierarchy
	m_uuidVectors.resize(m_objectIndexArray.length());

	// Get list of objects that are instanced by mash
	std::vector<MObject> targetObjects = pInstancer->GetTargetObjects();

	for (unsigned int idx = 0; idx < m_objectIndexArray.length(); ++idx)
	{
		size_t objectIndex = (size_t)m_objectIndexArray[idx];

		if (m_shapesCache.count(objectIndex) != 0)
			continue;

		m_shapesCache[(size_t)objectIndex] = GetShapesFromNode(targetObjects.at((size_t)objectIndex));
	}

	// try get id array
	bool idArrayExists = arrayAttrsData.checkArrayExist("id", arrType, &res);
	assert(res == MStatus::kSuccess);
	if (idArrayExists)
	{
		m_idArray = arrayAttrsData.getDoubleData("id", &res);
		assert(res == MStatus::kSuccess);
	}

	// try get position array
	bool positionArrayExists = arrayAttrsData.checkArrayExist("position", arrType, &res);
	assert(res == MStatus::kSuccess);
	if (positionArrayExists)
	{
		m_positionArray = arrayAttrsData.vectorArray("position", &res);
		assert(res == MStatus::kSuccess);
	}

	// try get rotation array
	bool rotationArrayExists = arrayAttrsData.checkArrayExist("rotation", arrType, &res);
	assert(res == MStatus::kSuccess);
	if (rotationArrayExists)
	{
		m_rotationArray = arrayAttrsData.vectorArray("rotation", &res);
		assert(res == MStatus::kSuccess);
	}

	// try get scale array
	bool scaleArrayExists = arrayAttrsData.checkArrayExist("scale", arrType, &res);
	assert(res == MStatus::kSuccess);
	if (scaleArrayExists)
	{
		m_scaleArray = arrayAttrsData.vectorArray("scale", &res);
		assert(res == MStatus::kSuccess);
	}

	m_isValid = true;

	return true;
}

bool InstancerMASH::MASHContext::IsValid(void) const
{
	if (!m_isValid)
		return false;

	if (IsByID())
		return true;

	unsigned int objLen = m_objectIndexArray.length();
	unsigned int posLen = m_positionArray.length();
	unsigned int rotLen = m_rotationArray.length();
	unsigned int scaleLen = m_scaleArray.length();
	bool sizeMatch = (objLen == posLen) && (objLen == rotLen) && (objLen == scaleLen);
	if (IsByParams() && sizeMatch)
		return true;

	return false;
}

bool InstancerMASH::MASHContext::IsByParams(void) const
{ 
	unsigned int posLen = m_positionArray.length();
	unsigned int rotLen = m_rotationArray.length();
	unsigned int scaleLen = m_scaleArray.length();

	return ((posLen != 0) && (rotLen != 0) && (scaleLen != 0));
}

void InstancerMASH::GenerateInstances()
{
	MFnDependencyNode instancerDagNode(m.object);
	MPlug plug(m.object, instancerDagNode.attribute("inp"));
	MObject data = plug.asMDataHandle().data();
	MFnArrayAttrsData arrayAttrsData(data);

	// objectIndex and id arrays are filled with doubles instead of ints in maya for some reason.
	MASHContext mashContext;
	bool IsDataReadSuccessfully = mashContext.Init(arrayAttrsData, this);
	assert(IsDataReadSuccessfully);
	if (!IsDataReadSuccessfully)
		return;

	if (mashContext.IsByID())
	{
		GenerateInstancesById(mashContext);
	}

	if (mashContext.IsByParams())
	{
		GenerateInstancesByParams(mashContext);
	}
}

void InstancerMASH::GenerateInstancesById(
	MASHContext& mashContext
	)
{
	assert(mashContext.IsValid());
	assert(mashContext.IsByID());

	// generate objects
	for (unsigned int idArrayIndex = 0; idArrayIndex < mashContext.m_idArray.length(); ++idArrayIndex)
	{
		// get object(s) to be instanced
		size_t objectIndex = (size_t)mashContext.m_objectIndexArray[idArrayIndex];
		auto it = mashContext.m_shapesCache.find(objectIndex);
		assert(it != mashContext.m_shapesCache.end());
		if (it == mashContext.m_shapesCache.end())
			continue;

		std::vector<MObject>& shapesToBeCreated = it->second;

		// generate objectId(s) for new objects if neceessary
		std::vector<MUuid>& uuidVector = mashContext.m_uuidVectors[objectIndex];
		while (shapesToBeCreated.size() != uuidVector.size())
		{
			MUuid uuid;
			uuid.generate();
			uuidVector.push_back(uuid);
		}

		// proceeed with generating objects
		size_t id = (size_t)mashContext.m_idArray[idArrayIndex];
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

void InstancerMASH::GenerateInstancesByParams(
	MASHContext& mashContext
)
{
	assert(mashContext.IsValid());
	assert(mashContext.IsByParams());

	MStatus res;

	unsigned int countObjects = mashContext.m_objectIndexArray.length();
	for (unsigned int idx = 0; idx < countObjects; ++idx)
	{
		// make transformation matrix for object to be instanced
		MTransformationMatrix matr;
		double vec_scale[3];
		res = mashContext.m_scaleArray[idx].get(vec_scale);
		assert(res == MStatus::kSuccess);
		matr.setScale(vec_scale, MSpace::kTransform);

		MEulerRotation rot(mashContext.m_rotationArray[idx]);
		matr.rotateTo(rot);

		double vec_transform[3];
		res = mashContext.m_positionArray[idx].get(vec_transform);
		assert(res == MStatus::kSuccess);
		matr.setTranslation(vec_transform, MSpace::kTransform);

		// get object(s) to be instanced
		size_t objectIndex = (size_t)mashContext.m_objectIndexArray[idx];
		auto it = mashContext.m_shapesCache.find(objectIndex);
		assert(it != mashContext.m_shapesCache.end());
		if (it == mashContext.m_shapesCache.end())
			continue;

		std::vector<MObject>& shapesToBeCreated = it->second;

		// generate objectId(s) for new objects if neceessary
		std::vector<MUuid>& uuidVector = mashContext.m_uuidVectors[objectIndex];
		while (shapesToBeCreated.size() != uuidVector.size())
		{
			MUuid uuid;
			uuid.generate();
			uuidVector.push_back(uuid);
		}

		// proceeed with generating objects
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
			m_instancedObjects[idx].push_back(instance);

			instance->SetSelfTransform(matr.asMatrix());

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
