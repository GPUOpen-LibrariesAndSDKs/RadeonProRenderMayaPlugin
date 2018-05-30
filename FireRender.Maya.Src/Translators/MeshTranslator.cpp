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

namespace FireMaya
{
namespace MeshTranslator
{
MObject GenerateSmoothMesh(const MObject& object, const MObject& parent, MStatus& status)
{
	MFnMesh mesh(object);

	DependencyNode attributes(object);

	bool smoothPreview = attributes.getBool("displaySmoothMesh");

	//for non smooth preview case:
	status = MStatus::kSuccess;

	return (smoothPreview) ? mesh.generateSmoothMesh(parent, 0, &status) : MObject::kNullObj;
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

	return tessellated;
}

std::vector<frw::Shape> MeshTranslator::TranslateMesh(frw::Context context, const MObject& originalObject)
{
	MAIN_THREAD_ONLY;

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

	if (mstatus != MStatus::kSuccess)
	{
		return elements;
	}

	MObject object = !tessellated.isNull() ? tessellated : originalObject;

	MFnMesh fnMesh(object, &mstatus);
	if (MStatus::kSuccess != mstatus)
	{
		mstatus.perror("MFnMesh constructor");
		return elements;
	}

	MIntArray faceMaterialIndices;
	int elementCount = GetFaceMaterials(fnMesh, faceMaterialIndices);
	elements.resize(elementCount);

	assert(faceMaterialIndices.length() == fnMesh.numPolygons());

	MStringArray uvSetNames;
	// UV coordinates
	// - auxillary arrays for passing data to RPR
	std::vector<std::vector<Float2> > uvCoords;
	std::vector<const float*> puvCoords;
	std::vector<size_t> sizeCoords;

	GetUVCoords(fnMesh, uvSetNames, uvCoords, puvCoords, sizeCoords);
	unsigned int uvSetCount = uvSetNames.length();

	//DebugPrint("Elements: %d; Polygons: %d; Vertices: %d; Normals: %d", elementCount, fnMesh.numPolygons(), points.length(), normals.length());

	// pointer to array of vertices coordinates in Maya
	const float* pVertices = fnMesh.getRawPoints(&mstatus);
	assert(MStatus::kSuccess == mstatus);
	int countVertices = fnMesh.numVertices(&mstatus);
	assert(MStatus::kSuccess == mstatus);

	// pointer to array of normal coordinates in Maya
	const float* pNormals = fnMesh.getRawNormals(&mstatus);
	assert(MStatus::kSuccess == mstatus);
	int countNormals = fnMesh.numNormals(&mstatus);
	assert(MStatus::kSuccess == mstatus);

	int numIndices = 0;
	{
		// get triangle count (max possible count; this number is used for reserve only)
		MIntArray triangleCounts; // basically number of triangles in polygons; size of array equal to number of polygons in mesh
		MIntArray triangleVertices; // indices of points in triangles (3 indices per triangle)
		mstatus = fnMesh.getTriangles(triangleCounts, triangleVertices);
		numIndices = triangleVertices.length();
	}

	// different RPR mesh is created for each shader
	for (int shaderId = 0; shaderId < elementCount; shaderId++)
	{
		// output indices of vertexes (3 indices for each triangle)
		std::vector<int> vertexIndices;
		vertexIndices.reserve(numIndices);

		// output indices of normals (3 indices for each triangle)
		std::vector<int> normalIndices;
		normalIndices.reserve(numIndices);

		// output indices of UV coordinates (3 indices for each triangle)
		// up to 2 UV chanels is supported, thus vector of vectors
		std::vector<std::vector<int> > uvIndices;
		uvIndices.reserve(uvSetCount);
		for (unsigned int currUVCHannel = 0; currUVCHannel < uvSetCount; ++currUVCHannel)
		{
			uvIndices.emplace_back();
			uvIndices[currUVCHannel].reserve(numIndices);
		}

		// iterate through mesh
		MItMeshPolygon it = MItMeshPolygon(fnMesh.object());
		for (; !it.isDone(); it.next())
		{

			// skip polygons that have different shader from current one
			if (faceMaterialIndices[it.index()] != shaderId)
				continue;

			AddPolygon(it, uvSetNames, vertexIndices, normalIndices, uvIndices);
		}

		// auxillary array for passing data to RPR
		std::vector<const rpr_int*>	puvIndices;
		for (unsigned int idx = 0; idx < uvSetCount; ++idx)
		{
			puvIndices.push_back(uvIndices[idx].size() > 0 ? uvIndices[idx].data() : nullptr);
		}

		std::vector<int> multiUV_texcoord_strides(uvSetCount, sizeof(Float2));
		std::vector<int> texIndexStride(uvSetCount, sizeof(int));

		if (sizeCoords.size() == 0 || puvIndices.size() == 0 || sizeCoords[0] == 0 || puvIndices[0] == nullptr)
		{
			// no uv set
			uvSetCount = 0;
		}

		// create mesh in RPR
		elements[shaderId] = context.CreateMeshEx(
			pVertices, countVertices, sizeof(Float3),
			pNormals, countNormals, sizeof(Float3),
			nullptr, 0, 0,
			uvSetCount, puvCoords.data(), sizeCoords.data(), multiUV_texcoord_strides.data(),
			vertexIndices.data(), sizeof(rpr_int),
			normalIndices.data(), sizeof(rpr_int),
			puvIndices.data(), texIndexStride.data(),
			std::vector<int>(vertexIndices.size() / 3, 3).data(), vertexIndices.size() / 3);
	}

	// Now remove any temporary mesh we created.
	if (!tessellated.isNull())
	{
		FireRenderThread::RunProcOnMainThread([&]
		{
			MFnDagNode parentNode(parent, &mstatus);
			if (MStatus::kSuccess == mstatus)
			{
				mstatus = parentNode.removeChild(tessellated);
				if (MStatus::kSuccess != mstatus)
					mstatus.perror("MFnDagNode::removeChild");
			}

			if (!tessellated.isNull()) // double-check if node hasn't already been removed
				MGlobal::deleteNode(tessellated);
		});
	}

	return elements;
}

void AddPolygon(MItMeshPolygon& it,
	const MStringArray& uvSetNames,
	std::vector<int>& vertexIndices,
	std::vector<int>& normalIndices,
	std::vector<std::vector<int> >& uvIndices)
{
	MStatus mstatus;

	unsigned int uvSetCount = uvSetNames.length();
	// get indices of vertexes of polygon
	// - these are indices of verts of polygion, not triangles!!!
	MIntArray vertices;
	mstatus = it.getVertices(vertices);
	assert(MStatus::kSuccess == mstatus);

	// get indices of vertices of triangles of current polygon
	// - these are indices of verts in triangles!
	MIntArray vertexList;
	MPointArray points;
	mstatus = it.getTriangles(points, vertexList);
	assert(MStatus::kSuccess == mstatus);

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
		normalIndices.push_back(it.normalIndex(localNormalIdxIt->second));
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

			mstatus = it.getUVIndex(localUVIdxIt->second, uvIdx, &uvSetNames[currUVCHannel]);

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
}

void GetUVCoords(const MFnMesh& fnMesh, 
					MStringArray& uvSetNames,
					std::vector<std::vector<Float2> >& uvCoords,
					std::vector<const float*>& puvCoords,
					std::vector<size_t>& sizeCoords)
{
	//MStringArray uvSetNames;
	fnMesh.getUVSetNames(uvSetNames);
	unsigned int uvSetCount = uvSetNames.length();

	// RPR supports only 2 UV sets. There is no way to get this value from RPR so it's hadrcoded
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
