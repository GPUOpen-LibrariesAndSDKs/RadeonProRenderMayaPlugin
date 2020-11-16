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

#include "../Context/FireRenderContext.h"
#include "SingleShaderMeshTranslator.h"
#include "MultipleShaderMeshTranslator.h"

#ifdef OPTIMIZATION_CLOCK
#include <chrono>
#endif

FireMaya::MeshTranslator::MeshPolygonData::MeshPolygonData()
	: pVertices(nullptr)
	, countVertices(0)
	, pNormals(nullptr)
	, countNormals(0)
	, triangleVertexIndicesCount(0)
{
}

bool FireMaya::MeshTranslator::MeshPolygonData::Initialize(const MFnMesh& fnMesh)
{
	GetUVCoords(fnMesh, uvSetNames, uvCoords, puvCoords, sizeCoords);
	unsigned int uvSetCount = uvSetNames.length();

	MStatus mstatus;

	// pointer to array of vertices coordinates in Maya
	pVertices = fnMesh.getRawPoints(&mstatus);
	assert(MStatus::kSuccess == mstatus);

	// For empty meshes vertices is null
	if (pVertices == nullptr)
	{
		return false;
	}

	countVertices = fnMesh.numVertices(&mstatus);
	assert(MStatus::kSuccess == mstatus);

	// pointer to array of normal coordinates in Maya
	pNormals = fnMesh.getRawNormals(&mstatus);
	assert(MStatus::kSuccess == mstatus);
	countNormals = fnMesh.numNormals(&mstatus);
	assert(MStatus::kSuccess == mstatus);

	// get triangle count (max possible count; this number is used for reserve only)
	MIntArray triangleCounts; // basically number of triangles in polygons; size of array equal to number of polygons in mesh
	MIntArray triangleVertices; // indices of points in triangles (3 indices per triangle)
	mstatus = fnMesh.getTriangles(triangleCounts, triangleVertices);
	triangleVertexIndicesCount = triangleVertices.length();

	return true;
}

std::vector<frw::Shape> FireMaya::MeshTranslator::TranslateMesh(const frw::Context& context, const MObject& originalObject)
{
	MAIN_THREAD_ONLY;

#ifdef OPTIMIZATION_CLOCK
	std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
#endif

	std::vector<frw::Shape> resultShapes;
	MStatus mayaStatus;

	MFnDagNode node(originalObject);

	DebugPrint("TranslateMesh: %s", node.fullPathName().asUTF8());

	// Don't render intermediate object
	if (node.isIntermediateObject(&mayaStatus))
	{
		return resultShapes;
	}

	// Create tesselated object
	MObject tessellated = GetTesselatedObjectIfNecessary(originalObject, mayaStatus);
	if (MStatus::kSuccess != mayaStatus)
	{
		mayaStatus.perror("Tesselation error");
		return resultShapes;
	}

	// Get mesh from tesselated object
	MObject object = !tessellated.isNull() ? tessellated : originalObject;
	MFnMesh fnMesh(object, &mayaStatus);
	if (MStatus::kSuccess != mayaStatus)
	{
		mayaStatus.perror("MFnMesh constructor");
		return resultShapes;
	}

	// get number of submeshes in mesh (number of materials used in this mesh)
	MIntArray faceMaterialIndices;
	int elementCount = GetFaceMaterials(fnMesh, faceMaterialIndices);
	resultShapes.resize(elementCount);
	assert(faceMaterialIndices.length() == fnMesh.numPolygons());

	// get common data from mesh
	MeshPolygonData meshPolygonData;
	bool successfullyInitialized = meshPolygonData.Initialize(fnMesh);
	if (!successfullyInitialized)
	{
		std::string nodeName = fnMesh.name().asChar();
		std::string message = nodeName + " wasn't created: Mesh has no vertices";
		MGlobal::displayWarning(message.c_str());
		return resultShapes;
	}

	// use special case TranslateMesh that is optimized for 1 shader
	if (elementCount == 1)
	{
		SingleShaderMeshTranslator::TranslateMesh(context, fnMesh, resultShapes, meshPolygonData);
#ifdef OPTIMIZATION_CLOCK
		std::chrono::steady_clock::time_point fin = std::chrono::steady_clock::now();
		std::chrono::milliseconds elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(fin - start);
#endif
	}
	else
	{
		MultipleShaderMeshTranslator::TranslateMesh(context, fnMesh, resultShapes, meshPolygonData, faceMaterialIndices);
	}

	// Export shape names
	for (size_t i = 0; i < resultShapes.size(); i++)
	{
		resultShapes[i].SetName((std::string(node.name().asChar()) + "_" + std::to_string(i)).c_str());
	}

	// Now remove any temporary mesh we created.
	if (!tessellated.isNull())
	{
		RemoveTesselatedTemporaryMesh(node, tessellated);
	}

#ifdef OPTIMIZATION_CLOCK
	std::chrono::steady_clock::time_point fin = std::chrono::steady_clock::now();
	std::chrono::milliseconds elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(fin - start);
	FireRenderContext::inTranslateMesh += elapsed.count();
#endif

	return resultShapes;
}

MObject FireMaya::MeshTranslator::Smoothed2ndUV(const MObject& object, MStatus& status)
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

MObject FireMaya::MeshTranslator::GenerateSmoothMesh(const MObject& object, const MObject& parent, MStatus& status)
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
		MObject clonedSmMeshParent = fnclonedSmoothedMesh.parent(0);
		MGlobal::deleteNode(clonedSmMeshParent);
	}

	return smoothedMesh;
}

MObject FireMaya::MeshTranslator::TessellateNurbsSurface(const MObject& object, const MObject& parent, MStatus& status)
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
	params.setUIsoparmType((MTesselationParams::IsoparmType) it->second);

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

MObject FireMaya::MeshTranslator::GetTesselatedObjectIfNecessary(const MObject& originalObject, MStatus& mstatus)
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

void FireMaya::MeshTranslator::RemoveTesselatedTemporaryMesh(const MFnDagNode& node, MObject tessellated)
{
	MStatus mayaStatus;
	MObject parent = node.parent(0);
	FireRenderThread::RunProcOnMainThread([&]
		{
#ifdef OPTIMIZATION_CLOCK
			std::chrono::steady_clock::time_point start_del = std::chrono::steady_clock::now();
#endif
			MFnDagNode parentNode(parent, &mayaStatus);
			if (MStatus::kSuccess == mayaStatus)
			{
				mayaStatus = parentNode.removeChild(tessellated);
				if (MStatus::kSuccess != mayaStatus)
				{
					mayaStatus.perror("MFnDagNode::removeChild");
				}
			}

			// double-check if node hasn't already been removed
			if (!tessellated.isNull())
			{
				MGlobal::deleteNode(tessellated);
			}

#ifdef OPTIMIZATION_CLOCK
			std::chrono::steady_clock::time_point fin_del = std::chrono::steady_clock::now();
			std::chrono::microseconds elapsed_del = std::chrono::duration_cast<std::chrono::microseconds>(fin_del - start_del);
			FireRenderContext::deleteNodes += elapsed_del.count();
#endif
		});
}

void FireMaya::MeshTranslator::GetUVCoords(
	const MFnMesh& fnMesh,
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
		MGlobal::displayError("Object " + fnMesh.fullPathName() + " have more than 2 UV sets. Only two UV sets per object supported. Scene will be rendered with first two UV sets.");
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
