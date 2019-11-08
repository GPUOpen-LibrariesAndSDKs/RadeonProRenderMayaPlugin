#include "FireRenderMeshMASH.h"

FireRenderMeshMASH::FireRenderMeshMASH(const FireRenderMesh& rhs, const std::string& uuid, const MObject instancer)
	: FireRenderMesh(rhs, uuid), 
	m_Instancer(instancer)
{ 
	m_SelfTransform.setToIdentity();
}

void FireRenderMeshMASH::SetSelfTransform(const MMatrix& matrix)
{
	m_SelfTransform = matrix;
}

bool FireRenderMeshMASH::IsMeshVisible(const MDagPath& meshPath, const FireRenderContext* context) const
{
	(void)meshPath;
	(void)context;
	MDagPath instancerPath;
	MDagPath::getAPathTo(m_Instancer, instancerPath);
	return instancerPath.isVisible();
}

MMatrix FireRenderMeshMASH::GetSelfTransform()
{
	return m_SelfTransform;
}
