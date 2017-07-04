#pragma once

#include <maya/MObject.h>
#include <maya/MFloatPoint.h>
#include <maya/M3dView.h>
#include <maya/MHWGeometry.h>
#include <maya/MHWGeometryUtilities.h>
#include <maya/MRenderTargetManager.h>
#include "SkyBuilder.h"
#include <vector>


/**
 * A mesh used to represent the Sun / Sky system in a Maya viewport.
 * It displays a compass rose showing direction information, and an
 * indicator showing the direction of the sun.
 */
class SkyLocatorMesh
{

public:
	// Life Cycle
	// -----------------------------------------------------------------------------

	SkyLocatorMesh(const MObject& object);
	virtual ~SkyLocatorMesh();


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

	/** Compass buffer sizes. */
	const unsigned int c_compassVertexCount = 20;
	const unsigned int c_compassIndexCount = 38;

	/** Sun sphere buffer sizes. */
	const unsigned int c_sunSphereVertexCount = 382;
	const unsigned int c_sunSphereIndexCount = 3120;

	/** Sun vector buffer sizes. */
	const unsigned int c_sunVectorVertexCount = 2;
	const unsigned int c_sunVectorIndexCount = 2;

	/** The height of the mesh above the ground plane. */
	const float c_height = 0.05f;


	// Members
	// -----------------------------------------------------------------------------

	/** The sky builder. */
	SkyBuilder m_skyBuilder;

	/** Mesh vertices. */
	std::vector<MFloatVector> m_vertices;

	/** Mesh indices. */
	std::vector<unsigned int> m_indices;

	/** Cached direction to the sky. */
	MFloatVector m_sunDirection;

	// Private Methods
	// -----------------------------------------------------------------------------

	/** Generate the mesh. */
	void generateMesh();

	/** Generate the compass mesh. */
	void generateCompassMesh();

	/** Generate the sun mesh. */
	void generateSunMesh();

	/** Return the total mesh vertex count. */
	unsigned int getVertexCount();

	/** Return the total mesh index count. */
	unsigned int getIndexCount();
};
