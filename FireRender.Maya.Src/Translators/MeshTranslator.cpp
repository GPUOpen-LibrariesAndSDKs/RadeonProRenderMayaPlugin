// If you experience performance issues on mesh creation functionality in Debug
// you can set (only for this file) Optimization option to "Maximize speed" and switch off Basic runtime checks to "Default"

#include "MeshTranslator.h"
#include "DependencyNode.h"
#include "FireRenderThread.h"

#include <maya/MFnMesh.h>
#include <maya/MFnSubd.h>
#include <maya/MFloatPointArray.h>
#include <maya/MFloatVectorArray.h>
#include <maya/MFloatArray.h>
#include <maya/MPointArray.h>
#include <maya/MItMeshPolygon.h>

#include <unordered_map>

#include "FireRenderContext.h"

#ifdef OPTIMIZATION_CLOCK
	#include <chrono>
#endif

namespace FireMaya
{
namespace MeshTranslator
{

MObject Smoothed2ndUV(const MObject& object, MStatus& status)
{
	MFnMesh mesh(object);

	// clone original mesh
	MObject clonedMesh = mesh.copy(object);

	// get UVs from original mesh from second uv set
	MStringArray uvsetNames;
	mesh.getUVSetNames(uvsetNames);
	MFloatArray uArray;
	MFloatArray vArray;
	status = mesh.getUVs(uArray, vArray, &uvsetNames[1]);
	MIntArray uvCounts;
	MIntArray uvIds;
	status = mesh.getAssignedUVs(uvCounts, uvIds, &uvsetNames[1]);

	// get cloned mesh
	MDagPath item;
	MFnDagNode cloned_node(clonedMesh);
	cloned_node.getPath(item);
	item.extendToShape();
	clonedMesh = item.node();

	if (!clonedMesh.hasFn(MFn::kMesh))
		return MObject::kNullObj;

	// assign UVs from second UV set to cloned mesh	
	MFnMesh fnClonedMesh(clonedMesh);
	MStringArray uvSetNamesCloned;
	fnClonedMesh.getUVSetNames(uvSetNamesCloned);

	status = fnClonedMesh.deleteUVSet(uvSetNamesCloned[1]);
	fnClonedMesh.clearUVs();
	status = fnClonedMesh.setUVs(uArray, vArray);
	status = fnClonedMesh.assignUVs(uvCounts, uvIds);

	// proceed with smoothing
	MFnDagNode dagClonedNode(clonedMesh);
	MObject clonedSmoothedMesh = fnClonedMesh.generateSmoothMesh(dagClonedNode.parent(0), NULL, &status);

	// destroy temporary object
	MGlobal::deleteNode(clonedMesh);

	return clonedSmoothedMesh;
}

MObject GenerateSmoothMesh(const MObject& object, const MObject& parent, MStatus& status)
{
	status = MStatus::kSuccess;

	DependencyNode attributes(object);
	bool smoothPreview = attributes.getBool("displaySmoothMesh");

	if (!smoothPreview)
		return MObject::kNullObj;

	MFnMesh mesh(object);

	{ // generateSmoothMesh is loosing material data if more then 1 material is present
		MIntArray materialIndices;
		MObjectArray shaders;
		unsigned int instanceNumber = 0;
		MStatus tmpRes = mesh.getConnectedShaders(instanceNumber, shaders, materialIndices);
		int countShaders = shaders.length();

		if (smoothPreview && (countShaders > 1))
			return MObject::kNullObj;
	}

	int in_numUVSets = mesh.numUVSets();

	MObject clonedSmoothedMesh;

	if (in_numUVSets != 1)
	{
		clonedSmoothedMesh = Smoothed2ndUV(object, status);
	}

	// for smooth preview case:
	MObject smoothedMesh = mesh.generateSmoothMesh(parent, NULL, &status);

	if (clonedSmoothedMesh != MObject::kNullObj)
	{
		// copy data of UV 0 of clonedSmoothedMesh into UV 1 of smoothedMesh
		MFnMesh fnCloned(clonedSmoothedMesh);
		MStringArray clonedUVSetNames;
		fnCloned.getUVSetNames(clonedUVSetNames);
		MFloatArray uArrayCloned;
		MFloatArray vArrayCloned;
		status = fnCloned.getUVs(uArrayCloned, vArrayCloned, &clonedUVSetNames[0]);

		MIntArray uvCountsCloned;
		MIntArray uvIdsCloned;
		status = fnCloned.getAssignedUVs(uvCountsCloned, uvIdsCloned, &clonedUVSetNames[0]);

		MFnMesh fnOrigSmoothed(smoothedMesh);

		MString newUVSetName("uv2");
		fnOrigSmoothed.createUVSetWithName("uv2", NULL, &status);

		status = fnOrigSmoothed.setUVs(uArrayCloned, vArrayCloned, &newUVSetName);
		status = fnOrigSmoothed.assignUVs(uvCountsCloned, uvIdsCloned, &newUVSetName);

		MFnDagNode fnclonedSmoothedMesh(clonedSmoothedMesh);
		MGlobal::deleteNode(fnclonedSmoothedMesh.parent(0));
	}

	return smoothedMesh;
}

MObject MeshTranslator::TessellateNurbsSurface(const MObject& object, const MObject& parent, MStatus& status)
{
	// Read tessellation attributes.
	DependencyNode attributes(object);

	int modeU = attributes.getInt("modeU");
	int numberU = attributes.getInt("numberU");
	int modeV = attributes.getInt("modeV");
	int numberV = attributes.getInt("numberV");

	bool smoothEdge = attributes.getBool("smoothEdge");

	bool useChordHeightRatio = attributes.getBool("useChordHeightRatio");
	bool edgeSwap = attributes.getBool("edgeSwap");
	bool useMinScreen = attributes.getBool("useMinScreen");

	double chordHeightRatio = attributes.getDouble("chordHeightRatio");
	double minScreen = attributes.getDouble("minScreen");

	// Construct tessellation parameters.
	MTesselationParams params(
		MTesselationParams::kGeneralFormat,
		MTesselationParams::kTriangles);

	const std::map<int, int> modeToTesParam = { {1, MTesselationParams::kSurface3DEquiSpaced },
										{ 2, MTesselationParams::kSurfaceEquiSpaced },
										{ 3, MTesselationParams::kSpanEquiSpaced },
										{ 4, MTesselationParams::kSurfaceEquiSpaced } };

	std::map<int, int>::const_iterator it = modeToTesParam.find(modeU);
	assert(it != modeToTesParam.end());
	params.setUIsoparmType( (MTesselationParams::IsoparmType) it->second);

	it = modeToTesParam.find(modeV);
	assert(it != modeToTesParam.end());
	params.setVIsoparmType((MTesselationParams::IsoparmType) it->second);

	params.setUNumber(numberU);
	params.setVNumber(numberV);
	params.setSubdivisionFlag(MTesselationParams::kUseChordHeightRatio, useChordHeightRatio);
	params.setChordHeightRatio(chordHeightRatio);
	params.setSubdivisionFlag(MTesselationParams::kUseMinScreenSize, useMinScreen);
	params.setMinScreenSize(minScreen, minScreen);
	params.setSubdivisionFlag(MTesselationParams::kUseEdgeSmooth, smoothEdge);
	params.setSubdivisionFlag(MTesselationParams::kUseTriangleEdgeSwapping, edgeSwap);

	// Tessellate the surface and return the resulting mesh object.
	MFnNurbsSurface surface(object);

	return surface.tesselate(params, parent, &status);
}

MObject MeshTranslator::GetTesselatedObjectIfNecessary(const MObject& originalObject, MStatus& mstatus)
{
#ifdef OPTIMIZATION_CLOCK
	std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
#endif
	MFnDagNode node(originalObject);

	DebugPrint("TranslateMesh: %s", node.fullPathName().asUTF8());

	MObject parent = node.parent(0);

	MObject tessellated;
	// tessellate to mesh if we aren't already one
	if (originalObject.hasFn(MFn::kMesh))
	{
		// all good :)
		tessellated = GenerateSmoothMesh(originalObject, parent, mstatus);
		if (mstatus != MStatus::kSuccess)
		{
			mstatus.perror("MFnMesh::generateSmoothMesh");
		}
	}
	else if (originalObject.hasFn(MFn::kNurbsSurface))
	{
		tessellated = TessellateNurbsSurface(originalObject, parent, mstatus);
		if (mstatus != MStatus::kSuccess)
		{
			mstatus.perror("MFnNurbsSurface::tessellate");
		}
	}
	else if (originalObject.hasFn(MFn::kSubdiv))
	{
		MFnSubd surface(originalObject);
		tessellated = surface.tesselate(false, 1, 1, parent, &mstatus);
		if (mstatus != MStatus::kSuccess)
		{
			mstatus.perror("MFnSubd::tessellate");
		}
	}

#ifdef OPTIMIZATION_CLOCK
	std::chrono::steady_clock::time_point fin = std::chrono::steady_clock::now();
	std::chrono::microseconds elapsed = std::chrono::duration_cast<std::chrono::microseconds>(fin - start);

	FireRenderContext::getTessellatedObj += elapsed.count();
#endif

	return tessellated;
}

struct MeshIdxDictionary
{
	// output coords of vertices
	std::vector<Float3> vertexCoords;

	// output coords of normals
	std::vector<Float3> normalCoords;

	// output coords of uv's
	std::vector<Float2> uvSubmeshCoords[2];

	// output indices of vertexes (3 indices for each triangle)
	std::vector<int> vertexIndices;

	// output indices of normals (3 indices for each triangle)
	std::vector<int> normalIndices;

	// table to convert global index of vertex (index of vertex in pVertices array) to one in vertexCoords
	// - local to submesh
	std::unordered_map<int, int> vertexCoordIdxGlobal2Local;

	// table to convert global index of normal (index of normal in pNormals array) to one in vertexCoords
	// - local to submesh
	std::unordered_map<int, int> normalCoordIdxGlobal2Local;

	// table to convert global index of uv coord (index of coord in uvIndices array) to one in normalCoords
	// - local to submesh
	std::unordered_map<int, int> uvCoordIdxGlobal2Local[2]; // size is always 1 or 2

	// output indices of UV coordinates (3 indices for each triangle)
	// up to 2 UV channels is supported, thus vector of vectors
	std::vector<int> uvIndices[2];
};

// make UVCoords and UVIndices arrays have the same size (RPR crashes if they are not)
void ChangeUVArrsSizes(MeshIdxDictionary* shaderData,
	const int elementCount,
	const unsigned int uvSetCount)
{
	if (uvSetCount == 1)
		return;

	for (int shaderId = 0; shaderId < elementCount; shaderId++)
	{
		MeshIdxDictionary& currShaderData = shaderData[shaderId];
	
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

inline void CreateRPRMeshes(std::vector<frw::Shape>& elements, 
	frw::Context& context,
	const MeshIdxDictionary* shaderData, 
	std::vector<std::vector<Float2> > &uvCoords, 
	const int elementCount, 
	const unsigned int uvSetCount)
{
#ifdef OPTIMIZATION_CLOCK
	std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
#endif

	for (int shaderId = 0; shaderId < elementCount; shaderId++)
	{
		const MeshIdxDictionary& currShaderData = shaderData[shaderId];

		// axillary data structures for passing data to RPR
		size_t num_faces = currShaderData.vertexIndices.size() / 3;
		std::vector<int> num_face_vertices(currShaderData.vertexIndices.size() / 3, 3);

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
			(const rpr_float *)(currShaderData.vertexCoords.data()), currShaderData.vertexCoords.size(), sizeof(Float3),
			(const rpr_float *)(currShaderData.normalCoords.data()), currShaderData.normalCoords.size(), sizeof(Float3),
			nullptr, 0, 0,
			uvSetCount, output_submeshUVCoords.data(), output_submeshSizeCoords.data(), multiUV_texcoord_strides.data(),
			currShaderData.vertexIndices.data(), sizeof(rpr_int),
			currShaderData.normalIndices.data(), sizeof(rpr_int),
			puvIndices.data(), texIndexStride.data(),
			num_face_vertices.data(), num_faces
		);
	}

#ifdef OPTIMIZATION_CLOCK
	std::chrono::steady_clock::time_point fin = std::chrono::steady_clock::now();
	std::chrono::milliseconds elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(fin - start);
	//LogPrint("Elapsed time in CreateMeshEx: %d", elapsed);
#endif
}

MeshPolygonData::MeshPolygonData()
	: pVertices(nullptr)
	, countVertices(0)
	, pNormals(nullptr)
	, countNormals(0)
	, numIndices(0)
{}

void MeshPolygonData::Initialize(MFnMesh& fnMesh)
{
	GetUVCoords(fnMesh, uvSetNames, uvCoords, puvCoords, sizeCoords);
	unsigned int uvSetCount = uvSetNames.length();

	MStatus mstatus;

	// pointer to array of vertices coordinates in Maya
	pVertices = fnMesh.getRawPoints(&mstatus);
	assert(MStatus::kSuccess == mstatus);
	countVertices = fnMesh.numVertices(&mstatus);
	assert(MStatus::kSuccess == mstatus);

	// pointer to array of normal coordinates in Maya
	pNormals = fnMesh.getRawNormals(&mstatus);
	assert(MStatus::kSuccess == mstatus);
	countNormals = fnMesh.numNormals(&mstatus);
	assert(MStatus::kSuccess == mstatus);

	{
		// get triangle count (max possible count; this number is used for reserve only)
		MIntArray triangleCounts; // basically number of triangles in polygons; size of array equal to number of polygons in mesh
		MIntArray triangleVertices; // indices of points in triangles (3 indices per triangle)
		mstatus = fnMesh.getTriangles(triangleCounts, triangleVertices);
		numIndices = triangleVertices.length();
	}
}

inline void AddPolygon(MItMeshPolygon& it,
	const MStringArray& uvSetNames,
	std::vector<int>& vertexIndices,
	std::vector<int>& normalIndices,
	std::vector<std::vector<int> >& uvIndices)
{
	MStatus mstatus;

#ifdef OPTIMIZATION_CLOCK
	std::chrono::steady_clock::time_point start_Vtx = std::chrono::steady_clock::now();
#endif

	unsigned int uvSetCount = uvSetNames.length();
	// get indices of vertexes of polygon
	// - these are indices of verts of polygon, not triangles!!!
	MIntArray vertices;
	mstatus = it.getVertices(vertices);
	assert(MStatus::kSuccess == mstatus);

	// get indices of vertices of triangles of current polygon
	// - these are indices of verts in triangles!
	MIntArray vertexList;
	MPointArray points;
	mstatus = it.getTriangles(points, vertexList);
	assert(MStatus::kSuccess == mstatus);

#ifdef OPTIMIZATION_CLOCK
	std::chrono::steady_clock::time_point fin_1GetData = std::chrono::steady_clock::now();
	std::chrono::nanoseconds elapsed_1GetData = std::chrono::duration_cast<std::chrono::nanoseconds>(fin_1GetData - start_Vtx);
	FireRenderContext::timeGetDataFromMaya += elapsed_1GetData.count();
#endif

	// write indices of triangles in mesh into output triangle indices array
	for (unsigned int idx = 0; idx < vertexList.length(); ++idx)
	{
		vertexIndices.push_back(vertexList[idx]);
	}

	// create table to convert global index in vertex indices array to local one [0...number of vertex in polygon]
	std::map<int, int> vertexIdxGlobalToLocal;
	int count = vertices.length();

	for (int idx = 0; idx < count; ++idx)
	{
		vertexIdxGlobalToLocal[vertices[idx]] = idx;
	}

	// write indices of normals of vertices (parallel to triangle vertices) into output array
	for (unsigned int idx = 0; idx < vertexList.length(); ++idx)
	{
		std::map<int, int>::iterator localNormalIdxIt = vertexIdxGlobalToLocal.find(vertexList[idx]);
		assert(localNormalIdxIt != vertexIdxGlobalToLocal.end());

#ifdef OPTIMIZATION_CLOCK
		std::chrono::steady_clock::time_point start_2GetData = std::chrono::steady_clock::now();
#endif
		unsigned int normal_idx = it.normalIndex(localNormalIdxIt->second);
#ifdef OPTIMIZATION_CLOCK
		std::chrono::steady_clock::time_point fin_2GetData = std::chrono::steady_clock::now();
		std::chrono::nanoseconds elapsed_2GetData = std::chrono::duration_cast<std::chrono::nanoseconds>(fin_2GetData - start_2GetData);
		FireRenderContext::timeGetDataFromMaya += elapsed_2GetData.count();
#endif

		normalIndices.push_back(normal_idx);
	}

	// up to 2 UV channels is supported
	for (unsigned int currUVCHannel = 0; currUVCHannel < uvSetCount; ++currUVCHannel)
	{
		// write indices 
		for (unsigned int idx = 0; idx < vertexList.length(); ++idx)
		{
			int uvIdx = 0;
			int index = vertexList[idx];

			std::map<int, int>::iterator localUVIdxIt = vertexIdxGlobalToLocal.find(index);

			assert(localUVIdxIt != vertexIdxGlobalToLocal.end());

			const MString& name = uvSetNames[currUVCHannel];

#ifdef OPTIMIZATION_CLOCK
			std::chrono::steady_clock::time_point start_3GetData = std::chrono::steady_clock::now();
#endif
			mstatus = it.getUVIndex(localUVIdxIt->second, uvIdx, &name);
#ifdef OPTIMIZATION_CLOCK
			std::chrono::steady_clock::time_point fin_3GetData = std::chrono::steady_clock::now();
			std::chrono::nanoseconds elapsed_3GetData = std::chrono::duration_cast<std::chrono::nanoseconds>(fin_3GetData - start_3GetData);
			FireRenderContext::timeGetDataFromMaya += elapsed_3GetData.count();
#endif

			if (mstatus == MStatus::kSuccess)
			{
				uvIndices[currUVCHannel].push_back(uvIdx);
			}
			else
			{
				// in case if uv coordinate not assigned to polygon set it index to 0
				uvIndices[currUVCHannel].push_back(0);
			}
		}
	}

#ifdef OPTIMIZATION_CLOCK
	std::chrono::steady_clock::time_point fin_Vtx = std::chrono::steady_clock::now();
	std::chrono::nanoseconds elapsed_Vtxp = std::chrono::duration_cast<std::chrono::nanoseconds>(fin_Vtx - start_Vtx);

	FireRenderContext::translateData += elapsed_Vtxp.count();
#endif
}

void TranslateMeshSingleShader(frw::Context context, 
	MFnMesh& fnMesh,
	std::vector<frw::Shape>& elements,
	MeshPolygonData& meshPolygonData)
{
	// output indices of vertexes (3 indices for each triangle)
	std::vector<int> vertexIndices;
	vertexIndices.reserve(meshPolygonData.numIndices);

	// output indices of normals (3 indices for each triangle)
	std::vector<int> normalIndices;
	normalIndices.reserve(meshPolygonData.numIndices);

	// output indices of UV coordinates (3 indices for each triangle)
	// up to 2 UV channels is supported, thus vector of vectors
	std::vector<std::vector<int> > uvIndices;
	unsigned int uvSetCount = meshPolygonData.uvSetNames.length();
	uvIndices.reserve(uvSetCount);
	for (unsigned int currUVCHannel = 0; currUVCHannel < uvSetCount; ++currUVCHannel)
	{
		uvIndices.emplace_back(); // create vector for uv indices
		uvIndices[currUVCHannel].reserve(meshPolygonData.numIndices);
	}

	// iterate through mesh

#ifdef OPTIMIZATION_CLOCK
	std::chrono::steady_clock::time_point start_AddPolygon = std::chrono::steady_clock::now();
#endif
	for (auto it = MItMeshPolygon(fnMesh.object()); !it.isDone(); it.next())
	{
#ifdef OPTIMIZATION_CLOCK
		std::chrono::steady_clock::time_point start_inner_AddPolygon = std::chrono::steady_clock::now();
#endif

		AddPolygon(it, meshPolygonData.uvSetNames, vertexIndices, normalIndices, uvIndices);
		
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

	if (meshPolygonData.sizeCoords.size() == 0 || puvIndices.size() == 0 || meshPolygonData.sizeCoords[0] == 0 || puvIndices[0] == nullptr)
	{
		// no uv set
		uvSetCount = 0;
	}

	// create mesh in RPR
#ifdef OPTIMIZATION_CLOCK
	std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
#endif

	elements[0] = context.CreateMeshEx(
		meshPolygonData.pVertices, meshPolygonData.countVertices, sizeof(Float3),
		meshPolygonData.pNormals, meshPolygonData.countNormals, sizeof(Float3),
		nullptr, 0, 0,
		uvSetCount, meshPolygonData.puvCoords.data(), meshPolygonData.sizeCoords.data(), multiUV_texcoord_strides.data(),
		vertexIndices.data(), sizeof(rpr_int),
		normalIndices.data(), sizeof(rpr_int),
		puvIndices.data(), texIndexStride.data(),
		std::vector<int>(vertexIndices.size() / 3, 3).data(), vertexIndices.size() / 3);

#ifdef OPTIMIZATION_CLOCK
	std::chrono::steady_clock::time_point fin = std::chrono::steady_clock::now();
	std::chrono::milliseconds elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(fin - start);
	FireRenderContext::overallCreateMeshEx += elapsed.count();
#endif
}

const size_t coordsPerPolygon = 12;
const size_t indicesPerPolygon = 6;

void ReserveShaderData(MFnMesh& fnMesh, MeshIdxDictionary* shaderData, MIntArray& faceMaterialIndices, int elementCount)
{
	struct IdxSizes
	{
		size_t coords_size;
		size_t indices_size;

		IdxSizes()
			: coords_size(0)
			, indices_size(0)
		{}
	};

	std::vector<IdxSizes> idxSizes;
	idxSizes.resize(elementCount);

	for (auto it = MItMeshPolygon(fnMesh.object()); !it.isDone(); it.next())
	{
		int shaderId = faceMaterialIndices[it.index()];

		idxSizes[shaderId].coords_size += coordsPerPolygon;
		idxSizes[shaderId].indices_size += indicesPerPolygon;
	}

	for (int shaderId = 0; shaderId < elementCount; shaderId++)
	{
		shaderData[shaderId].vertexCoords.reserve(idxSizes[shaderId].coords_size);
		shaderData[shaderId].normalCoords.reserve(idxSizes[shaderId].coords_size);
		shaderData[shaderId].vertexIndices.reserve(idxSizes[shaderId].indices_size);
		shaderData[shaderId].normalIndices.reserve(idxSizes[shaderId].indices_size);
	}
}

std::vector<frw::Shape> MeshTranslator::TranslateMesh(frw::Context context, const MObject& originalObject)
{
	MAIN_THREAD_ONLY;


#ifdef OPTIMIZATION_CLOCK
	std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
#endif

	std::vector<frw::Shape> elements;
	MStatus mstatus;
	MString errMsg;

	MFnDagNode node(originalObject);

	DebugPrint("TranslateMesh: %s", node.fullPathName().asUTF8());

	// Don't render intermediate object
	if (node.isIntermediateObject(&mstatus))
		return elements;

	MObject parent = node.parent(0);

	MObject tessellated = GetTesselatedObjectIfNecessary(originalObject, mstatus);

	// back-off
	if (mstatus != MStatus::kSuccess)
	{
		return elements;
	}

	MObject object = !tessellated.isNull() ? tessellated : originalObject;

	// back-off
	MFnMesh fnMesh(object, &mstatus);
	if (MStatus::kSuccess != mstatus)
	{
		mstatus.perror("MFnMesh constructor");
		return elements;
	}

	// get number of submeshes in mesh (number of materials used in this mesh)
	MIntArray faceMaterialIndices;
	int elementCount = GetFaceMaterials(fnMesh, faceMaterialIndices);
	elements.resize(elementCount);
	assert(faceMaterialIndices.length() == fnMesh.numPolygons());

	// get common data from mesh
	MeshPolygonData meshPolygonData;
	meshPolygonData.Initialize(fnMesh);

	// use special case TranslateMesh that is optimized for 1 shader
	if (elementCount == 1)
	{
		TranslateMeshSingleShader(context, fnMesh, elements, meshPolygonData);
#ifdef OPTIMIZATION_CLOCK
		std::chrono::steady_clock::time_point fin = std::chrono::steady_clock::now();
		std::chrono::milliseconds elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(fin - start);
#endif
	}
	else
	{
		// create mesh data container
		std::vector<MeshIdxDictionary> shaderData;
		shaderData.resize(elementCount);

		// reserve space for indices and coordinates
		ReserveShaderData(fnMesh, shaderData.data(), faceMaterialIndices, elementCount);

		// iterate through mesh
		for (MItMeshPolygon it = MItMeshPolygon(fnMesh.object()); !it.isDone(); it.next())
		{
			int shaderId = faceMaterialIndices[it.index()];

			AddPolygon(it, meshPolygonData, shaderData[shaderId]);
		}

		// make UVCoords and UVIndices arrays have the same size (RPR crashes if they are not)
		ChangeUVArrsSizes(shaderData.data(), elementCount, meshPolygonData.uvSetNames.length());

		// export shader data to context
		CreateRPRMeshes(elements, context, shaderData.data(), meshPolygonData.uvCoords, elementCount, meshPolygonData.uvSetNames.length());
	}

	// Export shape names
	for (size_t i = 0; i < elements.size(); i++)
	{
		elements[i].SetName((std::string(node.name().asChar()) + "_" + std::to_string(i)).c_str());
	}

	// Now remove any temporary mesh we created.
	if (!tessellated.isNull())
	{
		FireRenderThread::RunProcOnMainThread([&]
		{
#ifdef OPTIMIZATION_CLOCK
			std::chrono::steady_clock::time_point start_del = std::chrono::steady_clock::now();
#endif
			MFnDagNode parentNode(parent, &mstatus);
			if (MStatus::kSuccess == mstatus)
			{
				mstatus = parentNode.removeChild(tessellated);
				if (MStatus::kSuccess != mstatus)
					mstatus.perror("MFnDagNode::removeChild");
			}

			if (!tessellated.isNull()) // double-check if node hasn't already been removed
				MGlobal::deleteNode(tessellated);

#ifdef OPTIMIZATION_CLOCK
			std::chrono::steady_clock::time_point fin_del = std::chrono::steady_clock::now();
			std::chrono::microseconds elapsed_del = std::chrono::duration_cast<std::chrono::microseconds>(fin_del - start_del);
			FireRenderContext::deleteNodes += elapsed_del.count();
#endif
		});
	}

#ifdef OPTIMIZATION_CLOCK
	std::chrono::steady_clock::time_point fin = std::chrono::steady_clock::now();
	std::chrono::milliseconds elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(fin - start);
	FireRenderContext::inTranslateMesh += elapsed.count();
#endif

	return elements;
}

inline void AddPolygon(MItMeshPolygon& it,
	const MeshPolygonData& meshPolygonData,
	MeshIdxDictionary& meshIdxDictionary)
{
	MStatus mstatus;

	// get indices of vertices of triangles of current polygon
	// - these are indices of verts in triangles! (6 indices per polygon basically)
	MIntArray vertexIndicesList;
	MPointArray points;
	mstatus = it.getTriangles(points, vertexIndicesList);
	assert(MStatus::kSuccess == mstatus);

	// if coords of vertex not in vertex coord array => write them there
	for (unsigned int idx = 0; idx < vertexIndicesList.length(); ++idx)
	{
		std::unordered_map<int, int>::iterator it = meshIdxDictionary.vertexCoordIdxGlobal2Local.find(vertexIndicesList[idx]);

		if (it == meshIdxDictionary.vertexCoordIdxGlobal2Local.end())
		{
			unsigned int coordIdx = vertexIndicesList[idx] * 3;
			Float3 vtx;
			vtx.x = meshPolygonData.pVertices[coordIdx];
			vtx.y = meshPolygonData.pVertices[coordIdx + 1];
			vtx.z = meshPolygonData.pVertices[coordIdx + 2];
			meshIdxDictionary.vertexCoordIdxGlobal2Local[vertexIndicesList[idx]] = meshIdxDictionary.vertexCoords.size();
			meshIdxDictionary.vertexIndices.push_back(meshIdxDictionary.vertexCoords.size()); // <= write indices of triangles in mesh into output triangle indices array
			meshIdxDictionary.vertexCoords.push_back(vtx);
		}
		else
		{
			// write indices of triangles in mesh into output triangle indices array
			meshIdxDictionary.vertexIndices.push_back(it->second);
		}
	}
	
	// get indices of vertexes of polygon
	// - these are indices of verts of polygon, not triangles!!!
	MIntArray indicesInPolygon;
	mstatus = it.getVertices(indicesInPolygon);
	assert(MStatus::kSuccess == mstatus);

	// create table to convert global index in vertex indices array to local one [0...number of vertex in polygon]
	// this table is needed for normals and UVs
	// - local to polygon
	std::map<int, int> vertexIdxGlobalToLocal;
	int count = indicesInPolygon.length();

	for (int idx = 0; idx < count; ++idx)
	{
		vertexIdxGlobalToLocal[indicesInPolygon[idx]] = idx; 
	}

	// write indices of normals of vertices (parallel to triangle vertices) into output array
	for (unsigned int idx = 0; idx < vertexIndicesList.length(); ++idx)
	{
		std::map<int, int>::iterator localNormalIdxIt = vertexIdxGlobalToLocal.find(vertexIndicesList[idx]);
		assert(localNormalIdxIt != vertexIdxGlobalToLocal.end());		

		int globalNormalIdx = it.normalIndex(localNormalIdxIt->second);
		std::unordered_map<int, int>::iterator normal_it = meshIdxDictionary.normalCoordIdxGlobal2Local.find(globalNormalIdx);

		if (normal_it == meshIdxDictionary.normalCoordIdxGlobal2Local.end())
		{
			Float3 normal;
			normal.x = meshPolygonData.pNormals[globalNormalIdx*3];
			normal.y = meshPolygonData.pNormals[globalNormalIdx*3+1];
			normal.z = meshPolygonData.pNormals[globalNormalIdx*3+2];
			meshIdxDictionary.normalCoordIdxGlobal2Local[globalNormalIdx] = meshIdxDictionary.normalCoords.size();
			meshIdxDictionary.normalCoords.push_back(normal);
		}

		meshIdxDictionary.normalIndices.push_back(meshIdxDictionary.normalCoordIdxGlobal2Local[globalNormalIdx]);
	}

	// up to 2 UV channels is supported
	unsigned int uvSetCount = meshPolygonData.uvSetNames.length();

	for (unsigned int currUVCHannel = 0; currUVCHannel < uvSetCount; ++currUVCHannel)
	{
		// write indices 
		for (unsigned int idx = 0; idx < vertexIndicesList.length(); ++idx)
		{
			std::map<int, int>::iterator localUVIdxIt = vertexIdxGlobalToLocal.find(vertexIndicesList[idx]);
			assert(localUVIdxIt != vertexIdxGlobalToLocal.end());

			int uvIdx = 0;
			MString name = meshPolygonData.uvSetNames[currUVCHannel];
			mstatus = it.getUVIndex(localUVIdxIt->second, uvIdx, &name);

			if (mstatus != MStatus::kSuccess)
			{
				// in case if uv coordinate not assigned to polygon set it index to 0
				meshIdxDictionary.uvIndices[currUVCHannel].push_back(0);
				continue;
			}

			auto uv_it = meshIdxDictionary.uvCoordIdxGlobal2Local[currUVCHannel].find(uvIdx);

			if (uv_it == meshIdxDictionary.uvCoordIdxGlobal2Local[currUVCHannel].end())
			{
				Float2 uv;
				uv.x = meshPolygonData.puvCoords[currUVCHannel][uvIdx*2];
				uv.y = meshPolygonData.puvCoords[currUVCHannel][uvIdx*2 + 1];
				meshIdxDictionary.uvCoordIdxGlobal2Local[currUVCHannel][uvIdx] = meshIdxDictionary.uvSubmeshCoords[currUVCHannel].size();
				meshIdxDictionary.uvSubmeshCoords[currUVCHannel].push_back(uv);
			}

			meshIdxDictionary.uvIndices[currUVCHannel].push_back(meshIdxDictionary.uvCoordIdxGlobal2Local[currUVCHannel][uvIdx]);
		}
	}
}

void GetUVCoords(const MFnMesh& fnMesh, 
					MStringArray& uvSetNames,
					std::vector<std::vector<Float2> >& uvCoords,
					std::vector<const float*>& puvCoords,
					std::vector<size_t>& sizeCoords)
{
	fnMesh.getUVSetNames(uvSetNames);
	unsigned int uvSetCount = uvSetNames.length();

	// RPR supports only 2 UV sets. There is no way to get this value from RPR so it's hardcoded
	static const int rprMaxUVSetCount = 2;

	if (uvSetCount > rprMaxUVSetCount)
	{
		uvSetCount = rprMaxUVSetCount;
		FireRenderError err("UV set error", "One or more objects have multiple UV sets. Only two UV sets per object supported. Scene will be rendered with first two UV sets.", true);
	}

	uvSetNames.setLength(uvSetCount);

	puvCoords.reserve(uvSetCount);
	sizeCoords.reserve(uvSetCount);

	MStatus mstatus;

	// Core accepts only equal sized UV coordinate arrays
	// We should fill less array with zeros
	size_t maxUVSize = 0;

	// - fill these arrays with data
	// - up to 2 UV channels is supported
	for (unsigned int currUVCHannel = 0; currUVCHannel < uvSetCount; ++currUVCHannel)
	{
		// - get data from Maya
		MFloatArray uArray;
		MFloatArray vArray;
		mstatus = fnMesh.getUVs(uArray, vArray, &uvSetNames[currUVCHannel]);
		assert(MStatus::kSuccess == mstatus);
		assert(uArray.length() == vArray.length());

		// - RPR needs UV pairs instead of 2 parallel arrays (U and V) that Maya returns
		uvCoords.emplace_back();
		uvCoords[currUVCHannel].reserve(uArray.length());
		for (unsigned int idx = 0; idx < uArray.length(); ++idx)
		{
			Float2 uv;
			uv.x = uArray[idx];
			uv.y = vArray[idx];
			uvCoords[currUVCHannel].push_back(uv);
		}

		if (maxUVSize < uvCoords[currUVCHannel].size())
		{
			maxUVSize = uvCoords[currUVCHannel].size();
		}
	}

	// making equal size
	for (unsigned int currUVCHannel = 0; currUVCHannel < uvSetCount; ++currUVCHannel)
	{
		sizeCoords.push_back(maxUVSize);
		uvCoords[currUVCHannel].resize(maxUVSize);
		puvCoords.push_back(uvCoords[currUVCHannel].size() > 0 ? (float*)uvCoords[currUVCHannel].data() : nullptr);
	}
}

} // end of namespace MeshTranslator
} // end of namespace FireMaya
