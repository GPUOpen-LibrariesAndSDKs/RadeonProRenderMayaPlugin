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
#include "FireRenderObjects.h"
#include "Context/FireRenderContext.h"

class FireRenderMeshMASH : public FireRenderMesh
{
	/** Contain generated matrix from MASH */
	MMatrix m_SelfTransform;
	MObject m_Instancer;

public:
	FireRenderMeshMASH(const FireRenderMesh& rhs, const std::string& uuid, const MObject instancer);
	void SetSelfTransform(const MMatrix& matrix);

	const FireRenderMesh& GetOriginalFRMeshinstancedObject() const { return m_originalFRMesh; }

	virtual void Rebuild(void) override;
	virtual bool ReloadMesh(unsigned int sampleIdx = 0) override;

protected:
	/** Logic should be changed to not pass DagPath into the function, because it's not used in MASH visibility check */
	virtual bool IsMeshVisible(const MDagPath& meshPath, const FireRenderContext* context) const final override;
	virtual MMatrix GetSelfTransform() final override;

private:
	const FireRenderMesh& m_originalFRMesh;
};

