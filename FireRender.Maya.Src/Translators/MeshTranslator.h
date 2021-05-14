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

#include "frWrap.h"
#include "FireRenderUtils.h"

#include <maya/MItMeshPolygon.h>
#include <maya/MObject.h>
#include <vector>
#include <unordered_map>

namespace FireMaya
{
	class MeshTranslator
	{
	public:
		struct MeshPolygonData
		{
		public:
			MStringArray uvSetNames;
			std::vector<std::vector<Float2> > uvCoords;
			std::vector<const float*> puvCoords;
			std::vector<size_t> sizeCoords;

			std::vector<float> arrVertices; // Used in case of deformation motion blur
			std::vector<float> arrNormals;

			size_t countVertices;
			size_t countNormals;
			size_t triangleVertexIndicesCount;

			unsigned int motionSamplesCount;

			MeshPolygonData();

			// Initializes mesh and returns error status
			bool Initialize(MFnMesh& fnMesh, unsigned int deformationFrameCount, MString fullDagPath);
			bool ProcessDeformationFrameCount(MFnMesh& fnMesh, MString fullDagPath);

			size_t GetTotalVertexCount() { return std::max(arrVertices.size() / 3, countVertices); }
			size_t GetTotalNormalCount() { return std::max(arrNormals.size() / 3, countNormals); }

			const float* GetVertices() const { return arrVertices.size() > 0 ? arrVertices.data() : pVertices; }
			const float* GetNormals() const { return arrNormals.size() > 0 ? arrNormals.data() : pNormals; }

		private:
			const float* pVertices;
			const float* pNormals;
		};

		struct MeshIdxDictionary
		{
			// output coords of vertices
			std::vector<Float3> vertexCoords;

			// output coords of normals
			std::vector<Float3> normalCoords;

			// output coords of uv's
			std::vector<Float2> uvSubmeshCoords[2];

			// output indices of vertexes (3 indices for each triangle)
			std::vector<int> vertexCoordsIndices;

			// output indices of normals (3 indices for each triangle)
			std::vector<int> normalIndices;

			// table to convert global index of vertex (index of vertex in pVertices array) to one in vertexCoords
			// - local to submesh
			std::unordered_map<int, int> vertexCoordsIndicesGlobalToDictionary;

			// table to convert global index of normal (index of normal in pNormals array) to one in vertexCoords
			// - local to submesh
			std::unordered_map<int, int> normalCoordIdxGlobal2Local;

			// table to convert global index of uv coord (index of coord in uvIndices array) to one in normalCoords
			// - local to submesh
			std::unordered_map<int, int> uvCoordIdxGlobal2Local[2]; // size is always 1 or 2

			// output indices of UV coordinates (3 indices for each triangle)
			// up to 2 UV channels is supported, thus vector of vectors
			std::vector<int> uvIndices[2];

			// Indices of colored vertices
			std::vector<int> colorVertexIndices;

			// Colors corresponding to vertices
			std::vector<MColor> vertexColors;
		};

		static std::vector<frw::Shape> TranslateMesh(const frw::Context& context, const MObject& originalObject, unsigned int deformationFrameCount = 0, MString fullDagPath="");

	private:

		static MObject GenerateSmoothMesh(const MObject& object, const MObject& parent, MStatus& status);

		/** Tessellate a NURBS surface and return the resulting mesh object. */
		static MObject TessellateNurbsSurface(const MObject& object, const MObject& parent, MStatus& status);

		static MObject GetTesselatedObjectIfNecessary(const MObject& originalObject, MStatus& mstatus);

		static MObject GetSmoothedObjectIfNecessary(const MObject& originalObject, MStatus& mstatus);

		static void GetUVCoords(
			const MFnMesh& fnMesh,
			MStringArray& uvSetNames,
			std::vector<std::vector<Float2> >& uvCoords,
			std::vector<const float*>& puvCoords,
			std::vector<size_t>& sizeCoords
		);

		static MObject Smoothed2ndUV(const MObject& object, MStatus& status);

		static void RemoveTesselatedTemporaryMesh(const MFnDagNode& node, MObject tessellated);
		static void RemoveSmoothedTemporaryMesh(const MFnDagNode& node, MObject smoothed);

	};
}
