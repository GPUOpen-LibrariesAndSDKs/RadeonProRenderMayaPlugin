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

// Forward declaration
class FireRenderMeshMASH;

/**
	Instancer class used to pass generated data from MASH into core.
	Handles all instanced objects lifecycle.
	For now supports only one target object, but already retieves all instanced object array
*/
class InstancerMASH: public FireRenderNode
{
	using FireRenderObjectMap = std::map<std::size_t, std::shared_ptr<FireRenderMeshMASH>>;

	/** All objects created by instancer */
	FireRenderObjectMap m_instancedObjects;

	size_t m_instancedObjectsCachedSize;

public:
    InstancerMASH(FireRenderContext* context, const MDagPath& dagPath);
	virtual void RegisterCallbacks() override final;
	virtual void Freshen() override final;
	virtual void OnPlugDirty(MObject& node, MPlug& plug) override final;

private:
	size_t GetInstanceCount() const;
	std::vector<MObject> GetTargetObjects() const;
	std::vector<MMatrix> GetTransformMatrices() const;
	void GenerateInstances();
	bool ShouldBeRecreated() const;
};

