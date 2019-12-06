#pragma once
#include "MeshTranslator.h"
#include <maya/MPointArray.h>

namespace FireMaya
{
	class SingleShaderMeshTranslator
	{
	public:
		/** TranslateMesh optimized for meshes with 1 submesh */
		static void TranslateMesh(
			const frw::Context& context,
			const MFnMesh& fnMesh,
			std::vector<frw::Shape>& elements,
			MeshTranslator::MeshPolygonData& meshPolygonData
		);

	private:
		static void AddPolygonSingleShader(
			MItMeshPolygon& it,
			const MStringArray& uvSetNames,
			std::vector<int>& triangleVertexIndices,
			std::vector<int>& normalIndices,
			std::vector<std::vector<int>>& uvIndices,
			std::vector<MColor>& vertexColors,
			std::vector<int>& vertexIndices
		);

		static void FillIndicesUV(
			const unsigned uvSetCount,
			const MIntArray& vertexList,
			const std::map<int, int>& vertexIdxGlobalToLocal,
			const MStringArray& uvSetNames,
			MItMeshPolygon& meshPolygonIterator,
			std::vector<std::vector<int>>& uvIndices
		);

		static void FillNormalsIndices(
			const MIntArray& vertexList,
			const std::map<int, int>& vertexIdxGlobalToLocal,
			MItMeshPolygon& meshPolygonIterator,
			std::vector<int>& normalIndices
		);

	};
}
