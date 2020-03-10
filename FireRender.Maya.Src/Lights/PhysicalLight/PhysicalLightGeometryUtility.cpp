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
#include "PhysicalLightGeometryUtility.h"
#include "base_mesh.h"
#include "FireRenderUtils.h"

#include <math.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MPointArray.h>

// Viewport representation
void FillBuffersForRectangle(GizmoVertexVector& vertices, IndexVector& indices)
{
	vertices = { MFloatVector(0.0f, 0.0f, 0.0f),
				 MFloatVector(-1.0f, 1.0f, 0.0f),
				 MFloatVector(1.0f, 1.0f, 0.0f), 
				 MFloatVector(-1.0f, -1.0f, 0.0f), 
				 MFloatVector(1.0f, -1.0f, 0.0f),
				 MFloatVector(0.0f, 0.0f, -1.0f), };

	indices = { 1, 2, 1, 3, 2, 4, 3, 4, 1, 4, 2, 3, 0, 5 };
}

// Viewport representation
void FillBuffersForDisc(GizmoVertexVector& vertices, IndexVector& indices)
{
	const int segments = 40;

	vertices.reserve(segments + 2);

	for (int i = 0; i < segments; ++i)
	{
		double rad = 2 * M_PI * i / segments;

		vertices.push_back(MFloatVector((float) cos(rad), (float) sin(rad), 0.0f));
	}

	vertices.push_back(MFloatVector(0.0f, 0.0f, 0.0f));
	vertices.push_back(MFloatVector(0.0f, 0.0f, -1.0f));

	indices.reserve(4 * segments + 2);

	for (int i = 0; i < segments; ++i)
	{
		double rad = 2 * M_PI * i / segments;

		indices.push_back(segments);
		indices.push_back(i);

		indices.push_back(i);
		indices.push_back((i + 1) % segments);
	}

	indices.push_back(segments);
	indices.push_back(segments + 1);
}

// Viewport representation
void FillBuffersForCylinder(GizmoVertexVector& vertices, IndexVector& indices)
{
	const int segments = 40;

	vertices.reserve(2 * segments);

	for (int i = 0; i < segments; ++i)
	{
		double rad = 2 * M_PI * i / segments;
		float x = (float) cos(rad);
		float y = (float) sin(rad);

		vertices.push_back(MFloatVector(x, y, -1.0f));
		vertices.push_back(MFloatVector(x, y, 1.0f));
	}

	vertices.push_back(MFloatVector(0.0f, 0.0f, -1.0f)); // center of the bottom circle
	vertices.push_back(MFloatVector(0.0f, 0.0f, 1.0f)); //center of the top circle

	const int visualEdgesCount = 8;

	indices.reserve(4 * segments + 6 * segments / visualEdgesCount);
	for (int i = 0; i < segments; ++i)
	{
		indices.push_back(2 * i);
		indices.push_back(2 * (i + 1) % (2 * segments));

		indices.push_back(2 * i + 1);
		indices.push_back((2 * (i + 1) + 1) % (2 * segments));
	}

	for (int i = 0; i < visualEdgesCount; i++)
	{
		int vertexIndex = 2 * i * segments / visualEdgesCount;

		indices.push_back(vertexIndex);
		indices.push_back(vertexIndex + 1);

		if (i % 2 == 0)
		{
			indices.push_back(vertexIndex);
			indices.push_back(2 * segments); // connect to center of the bottom circle

			indices.push_back(vertexIndex + 1);
			indices.push_back(2 * segments + 1); // connect to center of the top circle
		}
	}
}

// Viewport representation
void FillBufferWithSphere(GizmoVertexVector& vertices, IndexVector& indices)
{
	const int vertCount = 1146;

	for (int i = 0; i < vertCount; i += 3)
	{
		vertices.push_back(MFloatVector(lowSpherePoints[i], lowSpherePoints[i + 1], lowSpherePoints[i + 2]));
	}

	const int indicesCount = 3120;
	const int indicesOffset = 0;

	for (int i = 0; i < indicesCount - indicesOffset; ++i)
	{
		indices.push_back(lowSphereWireConnect[indicesOffset + i]);
	}
}

// Getting buffers for visual representation in Viewport 2.0
bool PhysicalLightGeometryUtility::FillGizmoGeometryForAreaLight(PLAreaLightShape shapeType, GizmoVertexVector& vertices, IndexVector& indices)
{
	if (shapeType == PLAMesh)
	{
		return false;
	}

	switch (shapeType)
	{
	case PLADisc:
		FillBuffersForDisc(vertices, indices);
		break;
	case PLARectangle:
		FillBuffersForRectangle(vertices, indices);
		break;
	case PLACylinder:
		FillBuffersForCylinder(vertices, indices);
		break;
	case PLASphere:
		FillBufferWithSphere(vertices, indices);
		break;
	}

	return true;
}

void AddCircleAndEdgesForSpotLight(GizmoVertexVector& vertices, IndexVector& indices, float angle)
{
	const int segments = 40;
	const int edges = 4;

	int indexOffset = (int) vertices.size();
	float radius = tan(angle);
	float radiusAlongZ = 2.5f;

	for (int i = 0; i < segments; ++i)
	{
		double rad = 2 * M_PI * i / segments;
		float x = (float) cos(rad) * radius * radiusAlongZ;
		float y = (float) sin(rad) * radius * radiusAlongZ;

		vertices.push_back(MFloatVector(x, y, -radiusAlongZ));
	}

	vertices.push_back(MFloatVector(0.0f, 0.0f, 0.0f));

	for (int i = 0; i < segments; ++i)
	{
		double rad = 2 * M_PI * i / segments;
		float x = (float) cos(rad);
		float y = (float) sin(rad);

		indices.push_back(indexOffset + i);
		indices.push_back(indexOffset + (i + 1) % segments);
	}

	for (int i = 0; i < segments; i += segments / edges)
	{
		indices.push_back(indexOffset + segments);
		indices.push_back(indexOffset + i);
	}
}

bool PhysicalLightGeometryUtility::FillGizmoGeometryForSpotLight(GizmoVertexVector& vertices, IndexVector& indices, float innerAngle, float outerFalloff)
{
	AddCircleAndEdgesForSpotLight(vertices, indices, outerFalloff);
	AddCircleAndEdgesForSpotLight(vertices, indices, innerAngle);
	return true;
}

void AddArrowForDirectional(GizmoVertexVector& vertices, IndexVector& indices, float x, float y, float arrowWidth, float angle)
{
	float zMin = -1.0f;
	float zMax = 1.0f;
	float partLen = 0.6f;
	size_t vertOffset = vertices.size();
	float arrowWidthHalf = arrowWidth / 2.0f;

	float angleRad = (float) deg2rad(angle);

	vertices.push_back(MFloatVector(x, y, -zMin));
	vertices.push_back(MFloatVector(x, y, -partLen));

	float xArrowPointOffset = arrowWidthHalf * cos(angleRad);
	float yArrowPointOffset = sin(angle) * arrowWidthHalf;

	vertices.push_back(MFloatVector(x - xArrowPointOffset, y - yArrowPointOffset, -partLen));
	vertices.push_back(MFloatVector(x + xArrowPointOffset, y + yArrowPointOffset, -partLen));

	vertices.push_back(MFloatVector(x - xArrowPointOffset, y - yArrowPointOffset, -partLen));
	vertices.push_back(MFloatVector(x, y, -zMax));

	vertices.push_back(MFloatVector(x + xArrowPointOffset, y + yArrowPointOffset, -partLen));
	vertices.push_back(MFloatVector(x, y, -zMax));

	for (size_t i = 0; i < vertices.size() - vertOffset; i++)
	{
		indices.push_back((unsigned int) (vertOffset + i));
	}
}

bool PhysicalLightGeometryUtility::FillGizmoGeometryForDirectionalLight(GizmoVertexVector& vertices, IndexVector& indices)
{
	float arrowWidthSmall = 0.15f;
	float arrowWidth = 0.2f;
	float xOffset = 0.2f;
	float rotAngle = 30.0f;

	AddArrowForDirectional(vertices, indices, 0.0f, -0.3f, arrowWidthSmall, 0.0f);
	AddArrowForDirectional(vertices, indices, 0.0f, 0.0f, arrowWidth, 0.0f);

	AddArrowForDirectional(vertices, indices, xOffset, 0.25f, arrowWidthSmall, rotAngle);
	AddArrowForDirectional(vertices, indices, -xOffset, 0.25f, arrowWidthSmall , -rotAngle);
	return true;
}

bool PhysicalLightGeometryUtility::FillGizmoGeometryForPointLight(GizmoVertexVector& vertices, IndexVector& indices)
{
	return false;
}

// Shape for mesh while rendering in RPR
// if normals are not defined explicitly they will be set automatically
void FillRectangleShapeGeom(GeometryVertexVector& vertices, IndexVector& indices)
{
	vertices =
	{
		{ -1.f, -1.f, 0.f,		0.f, 0.f, -1.f,  0.0f, 0.0f },
		{ 1.f, -1.f, 0.f,		0.f, 0.f, -1.f,  1.0f, 0.0f },
		{ 1.f,  1.f, 0.f,		0.f, 0.f, -1.f,  1.0f, 1.0f },
		{ -1.f,  1.f, 0.f,		0.f, 0.f, -1.f,  0.0f, 1.0f }
	};

	indices =
	{
		0, 2, 1,
		0, 3, 2
	};
}

void FillDiscShapeGeom(GeometryVertexVector& vertices, IndexVector& indices)
{
	const int segments = 40;
	const int vertCount = segments + 1;

	vertices.resize(vertCount);
	indices.resize(3 * segments);

	double rad;
	for (int i = 0; i < segments; ++i)
	{
		rad = 2 * M_PI * i / segments;
		vertices[i].pos = { (float) cos(rad), (float) sin(rad), 0.0f };
		vertices[i].norm = { 0.0f, 0.0f, -1.0f };
		vertices[i].uv = { vertices[i].pos.x / 2.0f + 0.5f, vertices[i].pos.y / 2.0f + 0.5f };
	}

	vertices[segments].pos = { 0.0f, 0.0f, 0.0f };
	vertices[segments].uv = { 0.5f, 0.5f };
	vertices[segments].norm = { 0.0f, 0.0f, -1.0f };

	for (int i = 0; i < segments; ++i)
	{
		indices[3 * i] = segments;
		indices[3 * i + 1] = (i + 1) % segments;	
		indices[3 * i + 2] = i;
	}
}

void FillCylinderShapeGeom(GeometryVertexVector& vertices, IndexVector& indices)
{
	const int segments = 40;
	const int vertCountPerCircle = segments + 1 + 1;
	const int vertCount = 2 * vertCountPerCircle;
	const int facesPerVert = 4;

	vertices.resize(vertCount);
	indices.resize(3 * segments * facesPerVert);

	for (int k = 0; k < 2; k++)
	{
		double rad;

		float posZ = k % 2 == 0 ? 1.0f : -1.0f;

		for (int i = 0; i <= segments; ++i)
		{
			rad = 2 * M_PI * i / segments;
			int index = vertCountPerCircle * k + i;
			vertices[index].pos = { (float) cos(rad), (float) sin(rad), posZ };
			vertices[index].uv = { (float )i / segments,  k % 2 == 0 ? 0.0f : 1.0f};
		}

		vertices[vertCountPerCircle * (k + 1) - 1].pos = { 0.0f, 0.0f, posZ };
		vertices[vertCountPerCircle * (k + 1) - 1].uv = { 0.5f, 0.5f };
	}

	for (int i = 0; i < segments; ++i)
	{
		int base = i * 3 * facesPerVert;

		// top circle
		indices[base++] = segments;
		indices[base++] = i;
		indices[base++] = (i + 1);

		// bottom circle
		indices[base++] = vertCountPerCircle + i;
		indices[base++] = 2 * vertCountPerCircle - 1;
		indices[base++] = vertCountPerCircle + (i + 1);

		// cylinder edge, 2 triangles
		indices[base++] = i;
		indices[base++] = vertCountPerCircle + (i + 1);
		indices[base++] = (i + 1);

		indices[base++] = vertCountPerCircle + (i + 1);
		indices[base++] = i;
		indices[base++] = vertCountPerCircle + i;
	}
}

// Shape for mesh while rendering in RPR
frw::Shape CreateSphereShapeGeom(frw::Context frcontext)
{
	const int verticesCount = 382;
	const int texCoorCount = 439;
	const int polyCount = 400;

	return frcontext.CreateMesh(
		vertices, verticesCount, sizeof(float) * 3,
		normals, verticesCount, sizeof(float) * 3,
		texcoords, texCoorCount, sizeof(float) * 2,
		vertexIndices, sizeof(int),
		normalIndices, sizeof(int),
		texcoord_indices, sizeof(int),
		polyCountArray, polyCount);
}

void GetGeometryForMesh(PLAreaLightShape type, GeometryVertexVector& vertices, IndexVector& indices)
{
	switch (type)
	{
	case PLARectangle:
		FillRectangleShapeGeom(vertices, indices);
		break;
	case PLADisc:
		FillDiscShapeGeom(vertices, indices);
		break;
	case PLACylinder:
		FillCylinderShapeGeom(vertices, indices);
		break;
	}
}

frw::Shape PhysicalLightGeometryUtility::CreateShapeForAreaLight(PLAreaLightShape shapeType, frw::Context frcontext)
{
	if (shapeType == PLASphere)
	{
		return CreateSphereShapeGeom(frcontext);
	}

	GeometryVertexVector vertices;
	IndexVector indices;

	GetGeometryForMesh(shapeType, vertices, indices);

	std::vector<int> numFaceVertices;

	size_t faceCount = indices.size() / 3;
	numFaceVertices.reserve(faceCount);

	for (size_t i = 0; i < faceCount; i++)
	{
		numFaceVertices.push_back(3);
	}

	return frcontext.CreateMesh(
		(rpr_float*) &vertices[0], vertices.size(), sizeof(PLUtilityVertex),
		(rpr_float*)&vertices[0].norm, vertices.size(), sizeof(PLUtilityVertex),
		(rpr_float*) (&vertices[0] + 6), vertices.size(), sizeof(PLUtilityVertex),
		(int*) &indices[0], sizeof(int),
		(int*)&indices[0], sizeof(int),
		(int*)&indices[0], sizeof(int),
		(int*) &numFaceVertices[0], numFaceVertices.size());
}

inline float GetAreaBy3Points(const MPoint& point1, const MPoint& point2, const MPoint& point3, const MMatrix& matrix)
{
	return (float) (((point2 - point1) * matrix) ^ ((point3 - point1) * matrix)).length() / 2.0f;
}

float PhysicalLightGeometryUtility::GetAreaOfMeshPrimitive(PLAreaLightShape shapeType, const MMatrix & transformMatrix)
{
	MTransformationMatrix trMatrix(transformMatrix);

	double scale[3];
	trMatrix.getScale(scale, MSpace::kObject);

	float a = (float) scale[0];
	float b = (float) scale[1];
	float c = (float) scale[2];

	switch (shapeType)
	{
	case PLARectangle:
		return 4 * a * b;
	case PLADisc:
		return (float) M_PI * a * b;
	case PLACylinder:
		return (float) (2 * M_PI * a * b + 2 * M_PI * (a + b) * c);
	case PLASphere:
		{
			// use approximation for area of elipsoid calculation Knud Thomsen’s Formula
			float power = 1.605f;
			float ap = pow(a, power);
			float bp = pow(b, power);
			float cp = pow(c, power);

			return 4 * (float) M_PI * pow((ap * bp + ap * cp + bp * cp) / 3.0f, 1.0f / power);
		}
	}

	return 0.0f;
}

float PhysicalLightGeometryUtility::GetAreaOfMesh(const MFnMesh& mesh, const MMatrix & transformMatrix)
{
	MPointArray points;
	MIntArray indices;
	float area = 0.0f;

	for (MItMeshPolygon iter(mesh.object()); !iter.isDone(); iter.next())
	{
		iter.getTriangles(points, indices);

		for (unsigned int i = 0; i < indices.length(); i += 3)
		{
			area += GetAreaBy3Points(points[i], points[i + 1], points[i + 2], transformMatrix);
		}
	}

	return area;
}
