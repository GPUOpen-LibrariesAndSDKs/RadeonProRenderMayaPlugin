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
#include "SingleShaderMeshTranslator.h"

void FireMaya::SingleShaderMeshTranslator::TranslateMesh(
	const frw::Context& context,
	const MFnMesh& fnMesh,
	std::vector<frw::Shape>& elements,
	MeshTranslator::MeshPolygonData& meshData)
{
	// output indices of vertexes (3 indices for each triangle)
	std::vector<int> triangleVertexIndices;
	triangleVertexIndices.reserve(meshData.triangleVertexIndicesCount);

	// output indices of normals (3 indices for each triangle)
	std::vector<int> triangleNormalIndices;
	triangleNormalIndices.reserve(meshData.triangleVertexIndicesCount);

	// output indices of UV coordinates (3 indices for each triangle)
	// up to 2 UV channels is supported, thus vector of vectors
	std::vector<std::vector<int>> uvIndices;
	unsigned int uvSetCount = meshData.uvSetNames.length();
	uvIndices.reserve(uvSetCount);
	for (unsigned int currentChannelUV = 0; currentChannelUV < uvSetCount; ++currentChannelUV)
	{
		uvIndices.emplace_back(); // create vector for uv indices
		uvIndices[currentChannelUV].reserve(meshData.triangleVertexIndicesCount);
	}

	std::vector<MColor> vertexColors;
	vertexColors.resize(meshData.countVertices);
	std::vector<int> vertexIndices;
	vertexIndices.resize(meshData.countVertices);

	// iterate through mesh

#ifdef OPTIMIZATION_CLOCK
	std::chrono::steady_clock::time_point start_AddPolygon = std::chrono::steady_clock::now();
#endif
	for (auto it = MItMeshPolygon(fnMesh.object()); !it.isDone(); it.next())
	{
#ifdef OPTIMIZATION_CLOCK
		std::chrono::steady_clock::time_point start_inner_AddPolygon = std::chrono::steady_clock::now();
#endif

		AddPolygonSingleShader(it, meshData.uvSetNames, triangleVertexIndices, triangleNormalIndices, uvIndices, vertexColors, vertexIndices);

#ifdef OPTIMIZATION_CLOCK
		std::chrono::steady_clock::time_point fin_inner_AddPolygon = std::chrono::steady_clock::now();
		std::chrono::microseconds elapsed_inner_AddPolygon = std::chrono::duration_cast<std::chrono::microseconds>(fin_inner_AddPolygon - start_inner_AddPolygon);

		FireRenderContext::timeInInnerAddPolygon += elapsed_inner_AddPolygon.count();
#endif
	}
#ifdef OPTIMIZATION_CLOCK
	std::chrono::steady_clock::time_point fin_AddPolygon = std::chrono::steady_clock::now();
	std::chrono::milliseconds elapsed_AddPolygon = std::chrono::duration_cast<std::chrono::milliseconds>(fin_AddPolygon - start_AddPolygon);
	FireRenderContext::overallAddPolygon += elapsed_AddPolygon.count();

#endif

	// auxiliary array for passing data to RPR
	std::vector<const rpr_int*>	puvIndices;
	puvIndices.reserve(uvSetCount);
	for (unsigned int idx = 0; idx < uvSetCount; ++idx)
	{
		puvIndices.push_back(uvIndices[idx].size() > 0 ? uvIndices[idx].data() : nullptr);
	}

	std::vector<int> multiUV_texcoord_strides(uvSetCount, sizeof(Float2));
	std::vector<int> texIndexStride(uvSetCount, sizeof(int));

	if (meshData.sizeCoords.size() == 0 || puvIndices.size() == 0 || meshData.sizeCoords[0] == 0 || puvIndices[0] == nullptr)
	{
		// no uv set
		uvSetCount = 0;
	}

	// create mesh in RPR
#ifdef OPTIMIZATION_CLOCK
	std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
#endif

	rpr_mesh_info mesh_properties[16] = { 0 };

	if (meshData.motionSamplesCount > 0)
	{
		mesh_properties[0] = (rpr_mesh_info)RPR_MESH_MOTION_DIMENSION;
		mesh_properties[1] = (rpr_mesh_info)meshData.motionSamplesCount;
		mesh_properties[2] = (rpr_mesh_info)0;
	}

	elements[0] = context.CreateMeshEx(
		meshData.GetVertices(), meshData.GetTotalVertexCount(), sizeof(Float3),
		meshData.GetNormals(), meshData.GetTotalNormalCount(), sizeof(Float3),
		nullptr, 0, 0,
		uvSetCount, meshData.puvCoords.data(), meshData.sizeCoords.data(), multiUV_texcoord_strides.data(),
		triangleVertexIndices.data(), sizeof(rpr_int),
		triangleNormalIndices.data(), sizeof(rpr_int),
		puvIndices.data(), texIndexStride.data(),
		std::vector<int>(triangleVertexIndices.size() / 3, 3).data(), triangleVertexIndices.size() / 3, mesh_properties, fnMesh.name().asChar());

	if (!vertexColors.empty())
	{
		elements[0].SetVertexColors(vertexIndices, vertexColors, (rpr_int) meshData.countVertices);
	}

#ifdef OPTIMIZATION_CLOCK
	std::chrono::steady_clock::time_point fin = std::chrono::steady_clock::now();
	std::chrono::milliseconds elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(fin - start);
	FireRenderContext::overallCreateMeshEx += elapsed.count();
#endif
}

void FireMaya::SingleShaderMeshTranslator::AddPolygonSingleShader(
	MItMeshPolygon& meshPolygonIterator,
	const MStringArray& uvSetNames,
	std::vector<int>& outTriangleVertexIndices,
	std::vector<int>& outNormalIndices,
	std::vector<std::vector<int> >& outIndicesUV,
	std::vector<MColor>& outVertexColors,
	std::vector<int>& outColorVertexIndices)
{
	MStatus mayaStatus;

#ifdef OPTIMIZATION_CLOCK
	std::chrono::steady_clock::time_point start_Vtx = std::chrono::steady_clock::now();
#endif

	unsigned int uvSetCount = uvSetNames.length();
	// get indices of vertexes of polygon
	// - these are indices of verts of polygon, not triangles!!!
	MIntArray vertices;
	mayaStatus = meshPolygonIterator.getVertices(vertices);
	assert(MStatus::kSuccess == mayaStatus);

	// get indices of vertices of triangles of current polygon
	// - these are indices of verts in triangles!
	MIntArray trianglesVertexList;
	MPointArray points;
	mayaStatus = meshPolygonIterator.getTriangles(points, trianglesVertexList);
	assert(MStatus::kSuccess == mayaStatus);

#ifdef OPTIMIZATION_CLOCK
	std::chrono::steady_clock::time_point fin_1GetData = std::chrono::steady_clock::now();
	std::chrono::nanoseconds elapsed_1GetData = std::chrono::duration_cast<std::chrono::nanoseconds>(fin_1GetData - start_Vtx);
	FireRenderContext::timeGetDataFromMaya += elapsed_1GetData.count();
#endif

	// write indices of triangles in mesh into output triangle indices array
	for (unsigned int globalVertexIndex = 0; globalVertexIndex < trianglesVertexList.length(); ++globalVertexIndex)
	{
		outTriangleVertexIndices.push_back(trianglesVertexList[globalVertexIndex]);
	}

	// create table to convert global index in vertex indices array to local one [0...number of vertex in polygon]
	std::map<int, int> vertexIndexGlobalToLocal;

	MColorArray polygonColors;
	meshPolygonIterator.getColors(polygonColors);

	for (unsigned localVertexIndex = 0; localVertexIndex < vertices.length(); ++localVertexIndex)
	{
		unsigned globalVertexIndex = vertices[localVertexIndex];
		vertexIndexGlobalToLocal[globalVertexIndex] = localVertexIndex;
		
		if (polygonColors.length() > localVertexIndex)
		{
			outVertexColors[globalVertexIndex] = polygonColors[localVertexIndex];
			outColorVertexIndices[globalVertexIndex] = globalVertexIndex;
		} 
	}

	FillNormalsIndices(trianglesVertexList, vertexIndexGlobalToLocal, meshPolygonIterator, outNormalIndices);

	FillIndicesUV(uvSetCount, trianglesVertexList, vertexIndexGlobalToLocal, uvSetNames, meshPolygonIterator, outIndicesUV);

#ifdef OPTIMIZATION_CLOCK
	std::chrono::steady_clock::time_point fin_Vtx = std::chrono::steady_clock::now();
	std::chrono::nanoseconds elapsed_Vtxp = std::chrono::duration_cast<std::chrono::nanoseconds>(fin_Vtx - start_Vtx);

	FireRenderContext::translateData += elapsed_Vtxp.count();
#endif
}

void FireMaya::SingleShaderMeshTranslator::FillIndicesUV(
	const unsigned uvSetCount,
	const MIntArray& vertexList,
	const std::map<int, int>& vertexIdxGlobalToLocal,
	const MStringArray& uvSetNames,
	MItMeshPolygon& meshPolygonIterator,
	std::vector<std::vector<int>>& outIndicesUV)
{
	// up to 2 UV channels is supported
	for (unsigned int currentChannelUV = 0; currentChannelUV < uvSetCount; ++currentChannelUV)
	{
		// write indices 
		for (unsigned int idx = 0; idx < vertexList.length(); ++idx)
		{
			int uvIndex = 0;
			int globalVertexIndex = vertexList[idx];

			auto it = vertexIdxGlobalToLocal.find(globalVertexIndex);
			assert(it != vertexIdxGlobalToLocal.end());

			const MString& name = uvSetNames[currentChannelUV];

#ifdef OPTIMIZATION_CLOCK
			std::chrono::steady_clock::time_point start_3GetData = std::chrono::steady_clock::now();
#endif
			MStatus status = meshPolygonIterator.getUVIndex(it->second, uvIndex, &name);
#ifdef OPTIMIZATION_CLOCK
			std::chrono::steady_clock::time_point fin_3GetData = std::chrono::steady_clock::now();
			std::chrono::nanoseconds elapsed_3GetData = std::chrono::duration_cast<std::chrono::nanoseconds>(fin_3GetData - start_3GetData);
			FireRenderContext::timeGetDataFromMaya += elapsed_3GetData.count();
#endif

			if (status == MStatus::kSuccess)
			{
				outIndicesUV[currentChannelUV].push_back(uvIndex);
			}
			else
			{
				// in case if uv coordinate not assigned to polygon set it index to 0
				outIndicesUV[currentChannelUV].push_back(0);
			}
		}
	}
}

void FireMaya::SingleShaderMeshTranslator::FillNormalsIndices(
	const MIntArray& trianglesVertexList,
	const std::map<int, int>& vertexIdxGlobalToLocal,
	MItMeshPolygon& meshPolygonIterator,
	std::vector<int>& outNormalIndices)
{
	// write indices of normals of vertices (parallel to triangle vertices) into output array
	for (unsigned int globalVertexIndex = 0; globalVertexIndex < trianglesVertexList.length(); ++globalVertexIndex)
	{
		auto it = vertexIdxGlobalToLocal.find(trianglesVertexList[globalVertexIndex]);
		assert(it != vertexIdxGlobalToLocal.end());

#ifdef OPTIMIZATION_CLOCK
		std::chrono::steady_clock::time_point start_2GetData = std::chrono::steady_clock::now();
#endif
		unsigned int normalIndex = meshPolygonIterator.normalIndex(it->second);
#ifdef OPTIMIZATION_CLOCK
		std::chrono::steady_clock::time_point fin_2GetData = std::chrono::steady_clock::now();
		std::chrono::nanoseconds elapsed_2GetData = std::chrono::duration_cast<std::chrono::nanoseconds>(fin_2GetData - start_2GetData);
		FireRenderContext::timeGetDataFromMaya += elapsed_2GetData.count();
#endif

		outNormalIndices.push_back(normalIndex);
	}
}
