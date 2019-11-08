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

protected:
	/** Logic should be changed to not pass DagPath into the function, because it's not used in MASH visibility check */
	virtual bool IsMeshVisible(const MDagPath& meshPath, const FireRenderContext* context) const final override;
	virtual MMatrix GetSelfTransform() final override;
};

