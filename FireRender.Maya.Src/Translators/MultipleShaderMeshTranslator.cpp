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
#include "MultipleShaderMeshTranslator.h"

void FireMaya::MultipleShaderMeshTranslator::TranslateMesh(
	const frw::Context& context,
	const MFnMesh& fnMesh,
	std::vector<frw::Shape>& outElements,
	MeshTranslator::MeshPolygonData& meshPolygonData,
	const MIntArray& faceMaterialIndices)
{
	// create mesh data container
	std::vector<MeshTranslator::MeshIdxDictionary> shaderData;
	shaderData.resize(outElements.size());

	// reserve space for indices and coordinates
	ReserveShaderData(fnMesh, shaderData.data(), faceMaterialIndices, outElements.size());

	// iterate through mesh
	for (MItMeshPolygon it = MItMeshPolygon(fnMesh.object()); !it.isDone(); it.next())
	{
		int shaderId = faceMaterialIndices[it.index()];
		AddPolygonMultipleShader(it, meshPolygonData, shaderData[shaderId]);
	}

	// make UVCoords and UVIndices arrays have the same size (RPR crashes if they are not)
	ChangeUVArrsSizes(shaderData.data(), outElements.size(), meshPolygonData.uvSetNames.length());

	// export shader data to context
	CreateRPRMeshes(outElements, context, shaderData.data(), meshPolygonData.uvCoords, outElements.size(), meshPolygonData.uvSetNames.length(), fnMesh);
}

void FireMaya::MultipleShaderMeshTranslator::AddPolygonMultipleShader(
	MItMeshPolygon& meshPolygonIterator,
	const MeshTranslator::MeshPolygonData& meshPolygonData,
	MeshTranslator::MeshIdxDictionary& outMeshDictionary)
{
	MStatus mstatus;

	// get indices of vertexes of polygon
	// - these are indices of verts of polygon, not triangles!!!
	MIntArray indicesInPolygon;
	mstatus = meshPolygonIterator.getVertices(indicesInPolygon);
	assert(MStatus::kSuccess == mstatus);

	// create table to convert global index in vertex indices array to local one [0...number of vertex in polygon]
	// this table is needed for normals and UVs
	// - local to polygon
	std::map<int, int> vertexIdxGlobalToLocal;
	for (unsigned int localIndex = 0; localIndex < indicesInPolygon.length(); ++localIndex)
	{
		int globalIndex = indicesInPolygon[localIndex];
		vertexIdxGlobalToLocal[globalIndex] = localIndex;
	}

	// get indices of vertices of triangles of current polygon
	// - these are indices of verts in triangles! (6 indices per polygon basically)
	MIntArray globalVertexIndicesFromTrianglesList;
	MPointArray points;
	mstatus = meshPolygonIterator.getTriangles(points, globalVertexIndicesFromTrianglesList);
	assert(MStatus::kSuccess == mstatus);

	FillDictionaryWithVertexCoords(meshPolygonData, globalVertexIndicesFromTrianglesList, outMeshDictionary);
	FillDictionaryWithColorData(meshPolygonIterator, indicesInPolygon, outMeshDictionary);
	FillDictionaryWithNormals(meshPolygonIterator, meshPolygonData, globalVertexIndicesFromTrianglesList, vertexIdxGlobalToLocal, outMeshDictionary);
	FillDictionaryWithUV(meshPolygonIterator, meshPolygonData, globalVertexIndicesFromTrianglesList, vertexIdxGlobalToLocal, outMeshDictionary);
}


void FireMaya::MultipleShaderMeshTranslator::FillDictionaryWithColorData(
	MItMeshPolygon& meshPolygonIterator,
	const MIntArray& indicesInPolygon,
	MeshTranslator::MeshIdxDictionary& outMeshDictionary)
{
	// vertex colors
	MColorArray polygonColors;
	MStatus result = meshPolygonIterator.getColors(polygonColors);

	// Save polygon color data into dictionary with corresponging indices
	for (unsigned int localVertexIndex = 0; localVertexIndex < indicesInPolygon.length(); localVertexIndex++)
	{
		if (localVertexIndex >= polygonColors.length())
			continue;

		int globalVertexIndex = indicesInPolygon[localVertexIndex];
		int coreVertexIndex = outMeshDictionary.vertexCoordsIndicesGlobalToDictionary[globalVertexIndex];

		outMeshDictionary.vertexColors[globalVertexIndex] = polygonColors[localVertexIndex];
		outMeshDictionary.colorVertexIndices[globalVertexIndex] = globalVertexIndex;
	}
}

void FireMaya::MultipleShaderMeshTranslator::FillDictionaryWithVertexCoords(
	const MeshTranslator::MeshPolygonData& meshPolygonData,
	const MIntArray& globalVertexIndicesFromTrianglesList,
	MeshTranslator::MeshIdxDictionary& outMeshDictionary)
{
	// Save polygon triangles coordinates into dictionary with corresponging indices
	// if coords of vertex not in vertex coord array => write them there
	const float* vertices = meshPolygonData.GetVertices();
	for (unsigned int localVertexIndexFromPolygonTriangle = 0; localVertexIndexFromPolygonTriangle < globalVertexIndicesFromTrianglesList.length(); ++localVertexIndexFromPolygonTriangle)
	{
		int globalVertexIndex = globalVertexIndicesFromTrianglesList[localVertexIndexFromPolygonTriangle];
		auto it = outMeshDictionary.vertexCoordsIndicesGlobalToDictionary.find(globalVertexIndex);

		if (it == outMeshDictionary.vertexCoordsIndicesGlobalToDictionary.end())
		{
			int currentDictionaryVertexIndex = static_cast<int>(outMeshDictionary.vertexCoords.size());

			unsigned int rawVertexDataOffset = globalVertexIndex * 3;
			Float3 vertex;
			vertex.x = vertices[rawVertexDataOffset];
			vertex.y = vertices[rawVertexDataOffset + 1];
			vertex.z = vertices[rawVertexDataOffset + 2];
			outMeshDictionary.vertexCoordsIndicesGlobalToDictionary[globalVertexIndex] = currentDictionaryVertexIndex;
			outMeshDictionary.vertexCoordsIndices.push_back(currentDictionaryVertexIndex); // <= write indices of triangles in mesh into output triangle indices array
			outMeshDictionary.vertexCoords.push_back(vertex);
		}
		else
		{
			// write indices of triangles in mesh into output triangle indices array
			outMeshDictionary.vertexCoordsIndices.push_back(it->second);
		}
	}
}

void FireMaya::MultipleShaderMeshTranslator::FillDictionaryWithNormals(
	MItMeshPolygon& meshPolygonIterator,
	const MeshTranslator::MeshPolygonData& meshPolygonData,
	const MIntArray& globalVertexIndicesFromTrianglesList,
	const std::map<int, int>& vertexIdxGlobalToLocal,
	MeshTranslator::MeshIdxDictionary& outMeshDictionary)
{
	// write indices of normals of vertices (parallel to triangle vertices) into output array

	const float* normals = meshPolygonData.GetNormals();
	for (unsigned int idx = 0; idx < globalVertexIndicesFromTrianglesList.length(); ++idx)
	{
		auto localNormalIdxIt = vertexIdxGlobalToLocal.find(globalVertexIndicesFromTrianglesList[idx]);
		assert(localNormalIdxIt != vertexIdxGlobalToLocal.end());

		int globalNormalIdx = meshPolygonIterator.normalIndex(localNormalIdxIt->second);
		std::unordered_map<int, int>::iterator normal_it = outMeshDictionary.normalCoordIdxGlobal2Local.find(globalNormalIdx);

		if (normal_it == outMeshDictionary.normalCoordIdxGlobal2Local.end())
		{
			Float3 normal;
			normal.x = normals[globalNormalIdx * 3];
			normal.y = normals[globalNormalIdx * 3 + 1];
			normal.z = normals[globalNormalIdx * 3 + 2];
			outMeshDictionary.normalCoordIdxGlobal2Local[globalNormalIdx] = (int)(outMeshDictionary.normalCoords.size());
			outMeshDictionary.normalCoords.push_back(normal);
		}

		outMeshDictionary.normalIndices.push_back(outMeshDictionary.normalCoordIdxGlobal2Local[globalNormalIdx]);
	}
}

void FireMaya::MultipleShaderMeshTranslator::FillDictionaryWithUV(
	MItMeshPolygon& meshPolygonIterator,
	const MeshTranslator::MeshPolygonData& meshPolygonData,
	const MIntArray& globalVertexIndicesFromTrianglesList,
	const std::map<int, int>& vertexIdxGlobalToLocal,
	MeshTranslator::MeshIdxDictionary& outMeshDictionary)
{
	// up to 2 UV channels is supported
	unsigned int uvSetCount = meshPolygonData.uvSetNames.length();

	for (unsigned int currentChannelUV = 0; currentChannelUV < uvSetCount; ++currentChannelUV)
	{
		// write indices 
		for (unsigned int idx = 0; idx < globalVertexIndicesFromTrianglesList.length(); ++idx)
		{
			auto localUVIdxIt = vertexIdxGlobalToLocal.find(globalVertexIndicesFromTrianglesList[idx]);
			assert(localUVIdxIt != vertexIdxGlobalToLocal.end());

			int uvIdx = 0;
			MString name = meshPolygonData.uvSetNames[currentChannelUV];
			MStatus status = meshPolygonIterator.getUVIndex(localUVIdxIt->second, uvIdx, &name);

			if (status != MStatus::kSuccess)
			{
				// in case if uv coordinate not assigned to polygon set it index to 0
				outMeshDictionary.uvIndices[currentChannelUV].push_back(0);
				continue;
			}

			auto uv_it = outMeshDictionary.uvCoordIdxGlobal2Local[currentChannelUV].find(uvIdx);

			if (uv_it == outMeshDictionary.uvCoordIdxGlobal2Local[currentChannelUV].end())
			{
				Float2 uv;
				uv.x = meshPolygonData.puvCoords[currentChannelUV][uvIdx * 2];
				uv.y = meshPolygonData.puvCoords[currentChannelUV][uvIdx * 2 + 1];
				outMeshDictionary.uvCoordIdxGlobal2Local[currentChannelUV][uvIdx] = (int)(outMeshDictionary.uvSubmeshCoords[currentChannelUV].size());
				outMeshDictionary.uvSubmeshCoords[currentChannelUV].push_back(uv);
			}

			outMeshDictionary.uvIndices[currentChannelUV].push_back(outMeshDictionary.uvCoordIdxGlobal2Local[currentChannelUV][uvIdx]);
		}
	}
}

void FireMaya::MultipleShaderMeshTranslator::ReserveShaderData(
	const MFnMesh& fnMesh,
	MeshTranslator::MeshIdxDictionary* shaderData,
	const MIntArray& faceMaterialIndices,
	size_t elementCount)
{
	struct IdxSizes
	{
		size_t coords_size;
		size_t indices_size;

		IdxSizes()
			: coords_size(0)
			, indices_size(0)
		{
		}
	};

	std::vector<IdxSizes> idxSizes;
	idxSizes.resize(elementCount);

	std::map<int, size_t> vertexColorsSize;
	vertexColorsSize[0] = 0;
	vertexColorsSize[1] = 0;

	for (auto it = MItMeshPolygon(fnMesh.object()); !it.isDone(); it.next())
	{
		int shaderId = faceMaterialIndices[it.index()];

		assert(shaderId < idxSizes.size());

		idxSizes[shaderId].coords_size += coordsPerPolygon;
		idxSizes[shaderId].indices_size += indicesPerPolygon;

		MIntArray verticesArray;
		it.getVertices(verticesArray);
		vertexColorsSize[shaderId] += verticesArray.length();
	}

	for (int shaderId = 0; shaderId < elementCount; shaderId++)
	{
		shaderData[shaderId].vertexCoords.reserve(idxSizes[shaderId].coords_size);
		shaderData[shaderId].normalCoords.reserve(idxSizes[shaderId].coords_size);
		shaderData[shaderId].vertexCoordsIndices.reserve(idxSizes[shaderId].indices_size);
		shaderData[shaderId].normalIndices.reserve(idxSizes[shaderId].indices_size);
	}
}

// make UVCoords and UVIndices arrays have the same size (RPR crashes if they are not)
void FireMaya::MultipleShaderMeshTranslator::ChangeUVArrsSizes(
	MeshTranslator::MeshIdxDictionary* shaderData,
	const size_t elementCount,
	const unsigned int uvSetCount)
{
	if (uvSetCount == 1)
		return;

	for (int shaderId = 0; shaderId < elementCount; shaderId++)
	{
		MeshTranslator::MeshIdxDictionary& currShaderData = shaderData[shaderId];

		// - get max size of coords
		size_t maxSize = currShaderData.uvSubmeshCoords[0].size();

		for (unsigned int currUVCHannel = 1; currUVCHannel < uvSetCount; ++currUVCHannel)
		{
			size_t tmpSize = currShaderData.uvSubmeshCoords[currUVCHannel].size();

			if (tmpSize > maxSize)
			{
				maxSize = tmpSize;
			}
		}

		// - fill the array with less than max with zeros
		for (unsigned int currUVCHannel = 0; currUVCHannel < uvSetCount; ++currUVCHannel)
		{
			if (currShaderData.uvSubmeshCoords[currUVCHannel].size() == maxSize)
			{
				continue;
			}

			currShaderData.uvSubmeshCoords[currUVCHannel].resize(maxSize, Float2(0.0f, 0.0f));
		}

		// - get max size of indices
		maxSize = currShaderData.uvIndices[0].size();

		for (unsigned int currUVCHannel = 1; currUVCHannel < uvSetCount; ++currUVCHannel)
		{
			size_t tmpSize = currShaderData.uvIndices[currUVCHannel].size();

			if (tmpSize > maxSize)
			{
				maxSize = tmpSize;
			}
		}

		// - fill the array with less than max with zeros
		for (unsigned int currUVCHannel = 0; currUVCHannel < uvSetCount; ++currUVCHannel)
		{
			if (currShaderData.uvIndices[currUVCHannel].size() == maxSize)
			{
				continue;
			}

			currShaderData.uvIndices[currUVCHannel].resize(maxSize, 0);
		}
	}
}

void FireMaya::MultipleShaderMeshTranslator::CreateRPRMeshes(
	std::vector<frw::Shape>& elements,
	const frw::Context& context,
	const MeshTranslator::MeshIdxDictionary* shaderData,
	std::vector<std::vector<Float2> >& uvCoords,
	const size_t elementCount,
	const unsigned int uvSetCount,
	const MFnMesh& fnMesh)
{
#ifdef OPTIMIZATION_CLOCK
	std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
#endif

	for (int shaderId = 0; shaderId < elementCount; shaderId++)
	{
		const MeshTranslator::MeshIdxDictionary& currShaderData = shaderData[shaderId];

		// axillary data structures for passing data to RPR
		size_t num_faces = currShaderData.vertexCoordsIndices.size() / 3;
		std::vector<int> num_face_vertices(currShaderData.vertexCoordsIndices.size() / 3, 3);

		std::vector<const float*> output_submeshUVCoords;
		output_submeshUVCoords.reserve(uvSetCount);
		std::vector<size_t> output_submeshSizeCoords;
		output_submeshSizeCoords.reserve(uvSetCount);

		for (unsigned int currUVCHannel = 0; currUVCHannel < uvSetCount; ++currUVCHannel)
		{
			output_submeshUVCoords.push_back((const float*)(currShaderData.uvSubmeshCoords[currUVCHannel].data()));
			output_submeshSizeCoords.push_back(currShaderData.uvSubmeshCoords[currUVCHannel].size());
		}

		// dump data from map to auxiliary array (is needed for call to RPR)
		std::vector<const rpr_int*>	puvIndices;
		puvIndices.reserve(uvSetCount);

		for (unsigned int idx = 0; idx < uvSetCount; ++idx)
		{
			puvIndices.push_back(currShaderData.uvIndices[idx].size() > 0 ?
				currShaderData.uvIndices[idx].data() :
				nullptr);
		}

		// arrays with auxiliary data for RPR
		std::vector<int> texIndexStride(uvSetCount, sizeof(int));
		std::vector<int> multiUV_texcoord_strides(uvSetCount, sizeof(Float2));

		// create mesh in RPR
		elements[shaderId] = context.CreateMeshEx(
			(const rpr_float*)(currShaderData.vertexCoords.data()), currShaderData.vertexCoords.size(), sizeof(Float3),
			(const rpr_float*)(currShaderData.normalCoords.data()), currShaderData.normalCoords.size(), sizeof(Float3),
			nullptr, 0, 0,
			uvSetCount, output_submeshUVCoords.data(), output_submeshSizeCoords.data(), multiUV_texcoord_strides.data(),
			currShaderData.vertexCoordsIndices.data(), sizeof(rpr_int),
			currShaderData.normalIndices.data(), sizeof(rpr_int),
			puvIndices.data(), texIndexStride.data(),
			num_face_vertices.data(), num_faces, nullptr, fnMesh.name().asChar()
		);

		if (!currShaderData.vertexColors.empty())
		{
			std::vector<int> colorVertexIndices; 
			colorVertexIndices.resize(currShaderData.colorVertexIndices.size(), 0);
			for (int idx = 0; idx < currShaderData.colorVertexIndices.size(); ++idx)
			{
				const auto it = currShaderData.colorVertexIndices.find(idx);
				colorVertexIndices[idx] = it->second;
			}

			std::vector<MColor> vertexColors;
			vertexColors.resize(currShaderData.vertexColors.size());
			for (int idx = 0; idx < currShaderData.vertexColors.size(); ++idx)
			{
				const auto it = currShaderData.vertexColors.find(idx);
				vertexColors[idx] = it->second;
			}

			elements[shaderId].SetVertexColors(colorVertexIndices, vertexColors, (rpr_int) currShaderData.vertexCoords.size());
		}
	}

#ifdef OPTIMIZATION_CLOCK
	std::chrono::steady_clock::time_point fin = std::chrono::steady_clock::now();
	std::chrono::milliseconds elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(fin - start);
	//LogPrint("Elapsed time in CreateMeshEx: %d", elapsed);
#endif
}
