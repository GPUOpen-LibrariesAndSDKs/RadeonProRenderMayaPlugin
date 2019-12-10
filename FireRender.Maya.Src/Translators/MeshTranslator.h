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
			MStringArray uvSetNames;
			std::vector<std::vector<Float2> > uvCoords;
			std::vector<const float*> puvCoords;
			std::vector<size_t> sizeCoords;
			const float* pVertices;
			int countVertices;
			const float* pNormals;
			int countNormals;
			int triangleVertexIndicesCount;

			MeshPolygonData();

			// Initializes mesh and returns error status
			bool Initialize(const MFnMesh& fnMesh);
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

		static std::vector<frw::Shape> TranslateMesh(const frw::Context& context, const MObject& originalObject);

	private:

		static MObject GenerateSmoothMesh(const MObject& object, const MObject& parent, MStatus& status);

		/** Tessellate a NURBS surface and return the resulting mesh object. */
		static MObject TessellateNurbsSurface(const MObject& object, const MObject& parent, MStatus& status);

		static MObject GetTesselatedObjectIfNecessary(const MObject& originalObject, MStatus& mstatus);

		static void GetUVCoords(
			const MFnMesh& fnMesh,
			MStringArray& uvSetNames,
			std::vector<std::vector<Float2> >& uvCoords,
			std::vector<const float*>& puvCoords,
			std::vector<size_t>& sizeCoords
		);

		static MObject Smoothed2ndUV(const MObject& object, MStatus& status);

		static void RemoveTesselatedTemporaryMesh(const MFnDagNode& node, MObject tessellated);

	};
}
