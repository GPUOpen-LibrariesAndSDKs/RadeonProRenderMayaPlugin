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

	static bool FillGizmoGeometryForDiskLight(GizmoVertexVector& vertexVector, IndexVector& indexVector, float diskAngle, float diskRadius);
	static bool FillGizmoGeometryForSphereLight(GizmoVertexVector& vertexVector, IndexVector& indexVector, float sphereRadius);

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