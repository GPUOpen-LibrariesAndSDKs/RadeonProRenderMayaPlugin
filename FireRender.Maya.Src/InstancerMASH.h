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
class InstancerMASH: public FireRenderObject
{
	using FireRenderObjectMap = std::map<std::size_t, std::shared_ptr<FireRenderMeshMASH>>;

	/** All objects created by instancer */
	FireRenderObjectMap m_instancedObjects;

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
};

