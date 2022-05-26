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
#pragma once
#include <FireRenderObjects.h>
#include <maya/MFnArrayAttrsData.h>
#include "Context/FireRenderContext.h"
#include <maya/MFnCompoundAttribute.h>
#include <maya/MUuid.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MDoubleArray.h>

// Forward declaration
class FireRenderMeshMASH;

/**
	Instancer class used to pass generated data from MASH into core.
	Handles all instanced objects lifecycle.
	For now supports only one target object, but already retieves all instanced object array
*/
class InstancerMASH: public FireRenderNode
{
	using FireRenderObjectMap = std::map<std::size_t, std::vector<std::shared_ptr<FireRenderMeshMASH>>>;

	/** All objects created by instancer */
	FireRenderObjectMap m_instancedObjects;

	size_t m_instancedObjectsCachedSize;

public:
    InstancerMASH(FireRenderContext* context, const MDagPath& dagPath);
	virtual void RegisterCallbacks(void) override final;
	virtual void Freshen(bool shouldCalculateHash) override final;
	virtual void OnPlugDirty(MObject& node, MPlug& plug) override final;
	virtual bool ShouldForceReload(void) const override { return true; } // Is Mash Instancer
	virtual bool ReloadMesh(unsigned int sampleIdx = 0) override;

private:
	struct MASHContext
	{
		// objectIndex and id arrays are filled with doubles instead of ints in maya for some reason.
		MDoubleArray m_objectIndexArray;
		MDoubleArray m_idArray;
		MVectorArray m_positionArray;
		MVectorArray m_rotationArray;
		MVectorArray m_scaleArray;

		std::map<size_t, std::vector<MObject>> m_shapesCache;
		std::vector<std::vector<MUuid>> m_uuidVectors;

		bool m_isValid;

		MASHContext();
		~MASHContext() {}
		bool Init(MFnArrayAttrsData& arrayAttrsData, const InstancerMASH* pInstancer);
		bool IsValid(void) const;
		bool IsByID(void) const { return m_idArray.length() != 0; }
		bool IsByParams(void) const;
	};

	size_t GetInstanceCount(void) const;
	std::vector<MObject> GetTargetObjects(void) const;
	std::vector<MMatrix> GetTransformMatrices(void) const;
	void GenerateInstances(void);
	void GenerateInstancesById(MASHContext& mashContext);
	void GenerateInstancesByParams(MASHContext& mashContext);
	bool ShouldBeRecreated(void) const;
};

