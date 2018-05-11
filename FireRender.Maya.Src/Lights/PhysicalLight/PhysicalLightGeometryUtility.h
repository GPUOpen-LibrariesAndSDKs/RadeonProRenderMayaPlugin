#pragma once

#include "PhysicalLightData.h"
#include "FireMaya.h"
#include "FireRenderObjects.h"

#include <maya/MFnMesh.h>


typedef std::vector<unsigned int> IndexVector;
typedef std::vector<MFloatVector> GizmoVertexVector;

class PhysicalLightGeometryUtility
{
public:
	struct Float2
	{
		rpr_float x, y;
	};

	struct Float3
	{
		rpr_float x, y, z;
	};

	// Viewport 2.0 representation
	static bool FillGizmoGeometryForAreaLight(PLAreaLightShape shapeType, GizmoVertexVector& vertices, IndexVector& indices);
	static bool FillGizmoGeometryForSpotLight(GizmoVertexVector& vertices, IndexVector& indices, float innerAngle, float outerFalloff);
	static bool FillGizmoGeometryForDirectionalLight(GizmoVertexVector& vertices, IndexVector& indices);
	static bool FillGizmoGeometryForPointLight(GizmoVertexVector& vertices, IndexVector& indices);

	// Mesh Area calculation
	static float GetAreaOfMesh(const MFnMesh& mesh, const MMatrix & transformMatrix);
	static float GetAreaOfMeshPrimitive(PLAreaLightShape shapeType, const MMatrix & transformMatrix);

	// Mesh for RPR engine
	static frw::Shape CreateShapeForAreaLight(PLAreaLightShape shapeType, frw::Context frcontext);
};

struct PLUtilityVertex
{
	PhysicalLightGeometryUtility::Float3 pos;
	PhysicalLightGeometryUtility::Float3 norm;
	PhysicalLightGeometryUtility::Float2 uv;
};

typedef std::vector<PLUtilityVertex> GeometryVertexVector;