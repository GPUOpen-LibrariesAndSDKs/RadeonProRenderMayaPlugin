#include "MeshTranslator.h"
#include <maya/MPointArray.h>

namespace FireMaya
{
	class MultipleShaderMeshTranslator
	{
		static const size_t coordsPerPolygon = 12;
		static const size_t indicesPerPolygon = 6;

	public:
		/** TranslateMesh optimized for meshes with more then 1 submeshes */
		static void TranslateMesh(
			const frw::Context& context,
			const MFnMesh& fnMesh,
			std::vector<frw::Shape>& elements,
			MeshTranslator::MeshPolygonData& meshPolygonData,
			const MIntArray& faceMaterialIndices
		);

	private:
		static void AddPolygonMultipleShader(
			MItMeshPolygon& meshPolygonIterator,
			const MeshTranslator::MeshPolygonData& meshPolygonData,
			MeshTranslator::MeshIdxDictionary& meshIdxDictionary
		);

		static void FillDictionaryWithColorData(
			MItMeshPolygon& meshPolygonIterator,
			const MIntArray& indicesInPolygon,
			MeshTranslator::MeshIdxDictionary& outMeshDictionary
		);

		static void FillDictionaryWithVertexCoords(
			const MeshTranslator::MeshPolygonData& meshPolygonData,
			const MIntArray& indicesInPolygon,
			MeshTranslator::MeshIdxDictionary& outMeshDictionary
		);

		static void FillDictionaryWithNormals(
			MItMeshPolygon& meshPolygonIterator,
			const MeshTranslator::MeshPolygonData& meshPolygonData,
			const MIntArray& globalVertexIndicesFromTrianglesList,
			const std::map<int, int>& vertexIdxGlobalToLocal,
			MeshTranslator::MeshIdxDictionary& outMeshDictionary
		);

		static void FillDictionaryWithUV(
			MItMeshPolygon& meshPolygonIterator,
			const MeshTranslator::MeshPolygonData& meshPolygonData,
			const MIntArray& globalVertexIndicesFromTrianglesList,
			const std::map<int, int>& vertexIdxGlobalToLocal,
			MeshTranslator::MeshIdxDictionary& outMeshDictionary
		);

		static void ReserveShaderData(
			const MFnMesh& fnMesh,
			MeshTranslator::MeshIdxDictionary* shaderData,
			const MIntArray& faceMaterialIndices,
			int elementCount
		);

		static void ChangeUVArrsSizes(
			MeshTranslator::MeshIdxDictionary* shaderData,
			const int elementCount,
			const unsigned int uvSetCount
		);

		static void CreateRPRMeshes(
			std::vector<frw::Shape>& elements,
			const frw::Context& context,
			const MeshTranslator::MeshIdxDictionary* shaderData,
			std::vector<std::vector<Float2> >& uvCoords,
			const int elementCount,
			const unsigned int uvSetCount
		);

	};
}
