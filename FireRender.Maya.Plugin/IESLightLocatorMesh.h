#pragma once

#include <maya/MObject.h>
#include <maya/MFloatPoint.h>
#include <maya/M3dView.h>
#include <maya/MHWGeometry.h>
#include <maya/MHWGeometryUtilities.h>
#include <maya/MRenderTargetManager.h>

#include <vector>

/**
* A mesh used to represent the IES Light in a Maya viewport.
*/
class IESLightLocatorMesh
{
public:
	// Life Cycle
	// -----------------------------------------------------------------------------
	IESLightLocatorMesh(const MObject& object);
	virtual ~IESLightLocatorMesh();


	// Public Methods
	// -----------------------------------------------------------------------------

	/** Refresh the mesh and return whether it has changed. */
	bool refresh();

	/** Draw legacy GL geometry. */
	void glDraw(M3dView& view);

	/** Populate Viewport 2.0 override geometry. */
	void populateOverrideGeometry(
		const MHWRender::MGeometryRequirements &requirements,
		const MHWRender::MRenderItemList &renderItems,
		MHWRender::MGeometry &data);


private:

	// Constants
	// -----------------------------------------------------------------------------

	/** Sun sphere buffer sizes. */
	const unsigned int c_sunSphereVertexCount = 382;
	const unsigned int c_sunSphereIndexCount = 3120;


	// Members
	// -----------------------------------------------------------------------------

	/** Mesh vertices. */
	std::vector<MFloatVector> m_vertices;

	/** Mesh indices. */
	std::vector<unsigned int> m_indices;

private:
	// Private Methods
	// -----------------------------------------------------------------------------

	/** Generate the mesh. */
	void generateMesh();

	/** Generate the sun mesh. */
	void generateSunMesh();

	/** Return the total mesh vertex count. */
	unsigned int getVertexCount();

	/** Return the total mesh index count. */
	unsigned int getIndexCount();
};
