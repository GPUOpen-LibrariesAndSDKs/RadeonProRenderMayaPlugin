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
			frw::Shape& outShape,
			MeshTranslator::MeshPolygonData& meshPolygonData,
			const MIntArray& faceMaterialIndices,
			std::vector<int>& outFaceMaterialIndices
		);

	private:
		struct MeshIndicesData
		{
			std::vector<int>& triangleVertexIndices;
			std::vector<int>& normalIndices;
			std::vector<std::vector<int>>& uvIndices;
			std::vector<MColor>& vertexColors;
			std::vector<int>& colorVertexIndices;
			std::vector<int>& numFaceVertices;

			MeshIndicesData(
				std::vector<int>& _triangleVertexIndices,
				std::vector<int>& _normalIndices,
				std::vector<std::vector<int>>& _uvIndices,
				std::vector<MColor>& _vertexColors,
				std::vector<int>& _colorVertexIndices,
				std::vector<int>& _numFaceVertices)
				: triangleVertexIndices(_triangleVertexIndices)
				, normalIndices(_normalIndices)
				, uvIndices(_uvIndices)
				, vertexColors(_vertexColors)
				, colorVertexIndices(_colorVertexIndices)
				, numFaceVertices(_numFaceVertices)
			{};
		};

		static void AddPolygonSingleShader(
			MItMeshPolygon& meshPolygonIterator,
			const MStringArray& uvSetNames,
			MeshIndicesData& idxData,
			const MIntArray& faceMaterialIndices,
			std::vector<int>& outFaceMaterialIndices
		);

		static void FillIndicesUV(
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

		static void ProcessIndexesSimplified(
			MItMeshPolygon& meshPolygonIterator,
			const MStringArray& uvSetNames,
			MeshIndicesData& idxData,
			unsigned int localIdx,
			MIntArray& vertices,
			MColorArray& polygonColors
		);

	};
}
