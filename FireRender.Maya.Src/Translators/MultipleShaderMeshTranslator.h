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
			size_t elementCount
		);

		static void ChangeUVArrsSizes(
			MeshTranslator::MeshIdxDictionary* shaderData,
			const size_t elementCount,
			const unsigned int uvSetCount
		);

		static void CreateRPRMeshes(
			std::vector<frw::Shape>& elements,
			const frw::Context& context,
			const MeshTranslator::MeshIdxDictionary* shaderData,
			std::vector<std::vector<Float2> >& uvCoords,
			const size_t elementCount,
			const unsigned int uvSetCount,
			const MFnMesh& fnMesh
		);

	};
}
