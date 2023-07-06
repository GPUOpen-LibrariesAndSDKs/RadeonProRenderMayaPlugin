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
#include <maya/MSelectionList.h>
#include <maya/MAnimControl.h>

#include <unordered_map>

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
	, motionSamplesCount(0)
	, haveDeformation(false)
	, fullName("")
	, m_isInitialized(false)
{
}

void ChangeCurrentTimeAndUpdateMesh(MFnMesh& fnMesh, const MTime& time, MString fullDagPath)
{
	MGlobal::viewFrame(time);

	// We need to update fnMesh function set in order to apply time change

	MDagPath dagPath;

	MSelectionList sl;
	sl.add(fullDagPath);
	if (!sl.isEmpty())
	{
		sl.getDagPath(0, dagPath);
		fnMesh.setObject(dagPath);
	}
	else
	{
		assert(false);
	}
}

bool DoesMeshHaveDeformation(const MString& fullDagPath)
{
	// back-off
	if (fullDagPath.length() == 0)
		return false;
	
	// Check if mesh has deformers or rigs inside construction history
	MString command;
	command.format("source common.mel; hasGivenMeshDeformerOrRigAttached(\"^1s\");", fullDagPath);

	MStatus status;
	MString result = MGlobal::executeCommandStringResult(command, false, false, &status);

	if (result.asInt() == 0)
	{
		return false;
	}

	return true;
}

bool FireMaya::MeshTranslator::MeshPolygonData::ProcessDeformationFrameCount(MFnMesh& fnMesh, MString fullDagPath)
{
	if (motionSamplesCount < 2 || fullDagPath.length() == 0)
	{
		return false;
	}

	// Check if mesh has deformers or rigs inside construction history

	MString name = fullDagPath;
	MString command;
	command.format("source common.mel; hasGivenMeshDeformerOrRigAttached(\"^1s\");", name);
	
	MStatus status;
	MString result = MGlobal::executeCommandStringResult(command, false, false, &status);

	if (result.asInt() == 0)
	{
		return false;
	}

	MTime initialTime = MAnimControl::currentTime();
	MTime currentTime = initialTime;

	if (initialTime == MAnimControl::maxTime())
	{
		return false;
	}

	size_t floatsVertexOneFrame = 3 * countVertices;
	size_t floatsNormalOneFrame = 3 * countNormals;

	unsigned int currentTimeIndex = 0;

	MDagPath dagPath;

	for (unsigned int currentTimeIndex = 0; currentTimeIndex < motionSamplesCount; ++currentTimeIndex)
	{
		// positioning on next point of time (starting from currentTime)
		currentTime += (float)currentTimeIndex / (motionSamplesCount - 1);

		ChangeCurrentTimeAndUpdateMesh(fnMesh, currentTime, fullDagPath);

		const float* pData = fnMesh.getRawPoints(&status);
		assert(MStatus::kSuccess == status);
		std::copy(pData, pData + floatsVertexOneFrame, arrVertices.data() + floatsVertexOneFrame * currentTimeIndex);

		pData = fnMesh.getRawNormals(&status);
		assert(MStatus::kSuccess == status);
		std::copy(pData, pData + floatsNormalOneFrame, arrNormals.data() + floatsNormalOneFrame * currentTimeIndex);
	}

	ChangeCurrentTimeAndUpdateMesh(fnMesh, initialTime, fullDagPath);

	return true;
}

void FireMaya::MeshTranslator::MeshPolygonData::clear()
{
	m_isInitialized = false;

	arrVertices.clear();
	arrNormals.clear();

	uvCoords.clear();
	sizeCoords.clear();
	puvCoords.clear();
}

bool FireMaya::MeshTranslator::MeshPolygonData::Initialize(MFnMesh& fnMesh, unsigned int deformationFrameCount, MString fullDagPath)
{
	GetUVCoords(fnMesh, uvSetNames, uvCoords, puvCoords, sizeCoords);
	unsigned int uvSetCount = uvSetNames.length();

	MStatus mstatus;

	MString meshName = fnMesh.name();

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
	if (countVertices == 0)
	{
		return false;
	}

	// pointer to array of normal coordinates in Maya
	pNormals = fnMesh.getRawNormals(&mstatus);
	assert(MStatus::kSuccess == mstatus);

	countNormals = fnMesh.numNormals(&mstatus);
	assert(MStatus::kSuccess == mstatus);

	size_t floatsVertexOneFrame = 3 * countVertices;
	size_t floatsNormalOneFrame = 3 * countNormals;

	haveDeformation = DoesMeshHaveDeformation(fullDagPath) && (deformationFrameCount > 0);

	arrVertices.resize(haveDeformation ? floatsVertexOneFrame * deformationFrameCount : floatsVertexOneFrame );
	arrNormals.resize(haveDeformation ? floatsNormalOneFrame * deformationFrameCount : floatsNormalOneFrame );

	if (haveDeformation)
	{
		motionSamplesCount = deformationFrameCount;
	}

	if (!haveDeformation)
	{
		const float* pData = fnMesh.getRawPoints(&mstatus);
		assert(MStatus::kSuccess == mstatus);
		std::copy(pData, pData + countVertices * 3, arrVertices.data());

		pData = fnMesh.getRawNormals(&mstatus);
		assert(MStatus::kSuccess == mstatus);
		std::copy(pData, pData + countNormals * 3, arrNormals.data());
	}
	else
	{
		ReadDeformationFrame(fnMesh, 0);
	}

	// get triangle count (max possible count; this number is used for reserve only)
	MIntArray triangleCounts; // basically number of triangles in polygons; size of array equal to number of polygons in mesh
	MIntArray triangleVertices; // indices of points in triangles (3 indices per triangle)
	mstatus = fnMesh.getTriangles(triangleCounts, triangleVertices);
	triangleVertexIndicesCount = triangleVertices.length();

	m_isInitialized = true;
	return true;
}

bool FireMaya::MeshTranslator::MeshPolygonData::ReadDeformationFrame(MFnMesh& fnMesh, unsigned int currentDeformationFrame)
{
	if (!haveDeformation)
	{
		return true;
	}

	size_t floatsVertexOneFrame = 3 * countVertices;
	size_t floatsNormalOneFrame = 3 * countNormals;
	MStatus status;

	const float* pData = fnMesh.getRawPoints(&status);
	assert(MStatus::kSuccess == status);
	assert(arrVertices.size() >= floatsVertexOneFrame * currentDeformationFrame);
	std::copy(pData, pData + floatsVertexOneFrame, arrVertices.data() + floatsVertexOneFrame * currentDeformationFrame);

	pData = fnMesh.getRawNormals(&status);
	assert(MStatus::kSuccess == status);
	assert(arrNormals.size() >= floatsNormalOneFrame * currentDeformationFrame);
	std::copy(pData, pData + floatsNormalOneFrame, arrNormals.data() + floatsNormalOneFrame * currentDeformationFrame);

	return true;
}

bool FireMaya::MeshTranslator::PreProcessMesh(
	MeshPolygonData& outMeshPolygonData,
	const frw::Context& context,
	const MObject& originalObject,
	unsigned int deformationFrameCount /*= 0*/,
	unsigned int currentDeformationFrame /*= 0*/,
	MString fullDagPath /*= ""*/)
{
	MAIN_THREAD_ONLY;

	MStatus mayaStatus;

	MFnDagNode node(originalObject);
	outMeshPolygonData.fullName = node.fullPathName();

	DebugPrint("PreProcessMesh: %s, deformantion frame %d", outMeshPolygonData.fullName.asUTF8(), currentDeformationFrame);

	// Don't render intermediate object
	if (node.isIntermediateObject(&mayaStatus))
	{
		return false;
	}

	bool removeSmoothedORTesselated = true;

	// Create tesselated object
	MObject tessellated = GetTesselatedObjectIfNecessary(originalObject, mayaStatus);
	if (MStatus::kSuccess != mayaStatus)
	{
		mayaStatus.perror("Tesselation error");
		return false;
	}

	MObject smoothed = GetSmoothedObjectIfNecessary(originalObject, mayaStatus);
	if (MStatus::kSuccess != mayaStatus)
	{
		mayaStatus.perror("Smoothing error");
		return false;
	}

	// Consider geting mesh from tesselated or smoothed objects
	MObject object = originalObject;

	if (!tessellated.isNull())
	{
		object = tessellated;
		if (outMeshPolygonData.tesselatedObject.isNull())
		{
			outMeshPolygonData.tesselatedObject = object;
			removeSmoothedORTesselated = false;
		}
	}

	if (!smoothed.isNull())
	{
		object = smoothed;

		if (outMeshPolygonData.smoothedObject.isNull())
		{
			outMeshPolygonData.smoothedObject = object;
			removeSmoothedORTesselated = false;
		}
	}

	// get fnMesh
	MFnMesh fnMesh(object, &mayaStatus);

	if (MStatus::kSuccess != mayaStatus)
	{
		mayaStatus.perror("MFnMesh constructor error");
		return false;
	}

	// all checks clear => proceed with extracting mesh data from Maya
	bool successfullyProcessed = false;

	// get number of materials used in this mesh
	if (currentDeformationFrame == 0)
	{
		outMeshPolygonData.materialCount = GetFaceMaterials(fnMesh, outMeshPolygonData.faceMaterialIndices);	

		// for tesselated or smoothed mesh disable deformation MB for now
		successfullyProcessed = outMeshPolygonData.Initialize(
			fnMesh,
			object != originalObject ? 0 : deformationFrameCount,
			fullDagPath
		);
	}
	else
	{
		successfullyProcessed = outMeshPolygonData.ReadDeformationFrame(
			fnMesh,
			currentDeformationFrame
		);
	}

	if (!successfullyProcessed)
	{
		std::string nodeName = fnMesh.name().asChar();
		std::string message = nodeName + " wasn't created: Mesh has no vertices";
		MGlobal::displayWarning(message.c_str());
		return false;
	}

	if (removeSmoothedORTesselated)
	{
		// Now remove any temporary mesh we created.
		if (!tessellated.isNull())
		{
			RemoveTesselatedTemporaryMesh(node, tessellated);
		}
		if (!smoothed.isNull())
		{
			RemoveSmoothedTemporaryMesh(node, smoothed);
		}
	}

	return successfullyProcessed;
}

frw::Shape FireMaya::MeshTranslator::TranslateMesh(
	MeshPolygonData& meshPolygonData,
	const frw::Context& context,
	const MObject& originalObject,
	std::vector<int>& outFaceMaterialIndices,
	unsigned int deformationFrameCount, MString fullDagPath)
{
	DebugPrint("TranslateMesh: %s", meshPolygonData.fullName.asUTF8());

	outFaceMaterialIndices.clear();

	// get fnMesh
	// - NOTE: this will be removed in future PR
	MObject object = originalObject;
	MStatus mayaStatus;
	
	if (!meshPolygonData.tesselatedObject.isNull())
	{
		object = meshPolygonData.tesselatedObject;
	}
	if (!meshPolygonData.smoothedObject.isNull())
	{
		object = meshPolygonData.smoothedObject;
	}

	MFnMesh fnMesh(object, &mayaStatus);
	if (MStatus::kSuccess != mayaStatus)
	{
		mayaStatus.perror("MFnMesh constructor");
	}

	// translate mesh
	frw::Shape outShape;
	SingleShaderMeshTranslator::TranslateMesh(
		context, fnMesh, outShape, meshPolygonData, meshPolygonData.faceMaterialIndices, outFaceMaterialIndices
	);

	// Now remove any temporary mesh we created.
	MFnDagNode node(originalObject);
	if (!meshPolygonData.tesselatedObject.isNull())
	{
		RemoveTesselatedTemporaryMesh(node, meshPolygonData.tesselatedObject);
		meshPolygonData.tesselatedObject = MObject();
	}

	if (!meshPolygonData.smoothedObject.isNull())
	{
		RemoveSmoothedTemporaryMesh(node, meshPolygonData.smoothedObject);
		meshPolygonData.smoothedObject = MObject();
	}

	return outShape;
}

frw::Shape FireMaya::MeshTranslator::TranslateMesh(
	const frw::Context& context, 
	const MObject& originalObject, 
	std::vector<int>& outFaceMaterialIndices,
	unsigned int deformationFrameCount, MString fullDagPath)
{
	MAIN_THREAD_ONLY;

#ifdef OPTIMIZATION_CLOCK
	std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
#endif

	MStatus mayaStatus;

	MFnDagNode node(originalObject);

	DebugPrint("TranslateMesh: %s", node.fullPathName().asUTF8());

	frw::Shape outShape;

	// Don't render intermediate object
	if (node.isIntermediateObject(&mayaStatus))
	{
		return outShape;
	}

	// Create tesselated object
	MObject tessellated = GetTesselatedObjectIfNecessary(originalObject, mayaStatus);
	if (MStatus::kSuccess != mayaStatus)
	{
		mayaStatus.perror("Tesselation error");
		return outShape;
	}

	MObject smoothed = GetSmoothedObjectIfNecessary(originalObject, mayaStatus);
	if (MStatus::kSuccess != mayaStatus)
	{
		mayaStatus.perror("Smoothing error");
		return outShape;
	}

	// Consider geting mesh from tesselated or smoothed objects
	MObject object = originalObject;

	if (!tessellated.isNull())
	{
		object = tessellated;
	}

	if (!smoothed.isNull())
	{
		object = smoothed;
	}

	MFnMesh fnMesh(object, &mayaStatus);

	if (MStatus::kSuccess != mayaStatus)
	{
		mayaStatus.perror("MFnMesh constructor");
		return outShape;
	}

	// get number of materials used in this mesh
	MIntArray faceMaterialIndices;
	int materialCount = GetFaceMaterials(fnMesh, faceMaterialIndices);

	// get common data from mesh
	MeshPolygonData meshPolygonData;

	/// for tesselated or smoothed mesh disable deformation MB for now
	bool successfullyInitialized = meshPolygonData.Initialize(fnMesh, object != originalObject ? 0 : deformationFrameCount, fullDagPath);
	if (!successfullyInitialized)
	{
		std::string nodeName = fnMesh.name().asChar();
		std::string message = nodeName + " wasn't created: Mesh has no vertices";
		MGlobal::displayWarning(message.c_str());
		return outShape;
	}

	outFaceMaterialIndices.clear();
	SingleShaderMeshTranslator::TranslateMesh(
		context, fnMesh, outShape, meshPolygonData, faceMaterialIndices, outFaceMaterialIndices
	);

#ifdef OPTIMIZATION_CLOCK
	std::chrono::steady_clock::time_point fin = std::chrono::steady_clock::now();
	std::chrono::milliseconds elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(fin - start);
#endif

	// Now remove any temporary mesh we created.
	if (!tessellated.isNull())
	{
		RemoveTesselatedTemporaryMesh(node, tessellated);
	}
	if (!smoothed.isNull())
	{
		RemoveSmoothedTemporaryMesh(node, smoothed);
	}

#ifdef OPTIMIZATION_CLOCK
	std::chrono::steady_clock::time_point fin = std::chrono::steady_clock::now();
	std::chrono::milliseconds elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(fin - start);
	FireRenderContext::inTranslateMesh += elapsed.count();
#endif

	return outShape;
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

MString GenerateSmoothOptions(const MFnDagNode& dagMesh)
{
	std::map<std::string, std::string> optionMap;

	MPlug useSmoothPreviewForRenderPlug = dagMesh.findPlug("useSmoothPreviewForRender");
	assert(!useSmoothPreviewForRenderPlug.isNull());

	std::string smoothLevelPlugName = "smoothLevel";
	if (useSmoothPreviewForRenderPlug.asInt() == 0)
	{
		smoothLevelPlugName = "renderSmoothLevel";
	}

	MPlug smoothLevelPlug = dagMesh.findPlug(smoothLevelPlugName.c_str());
	assert(!smoothLevelPlug.isNull());
	std::string smoothLevel = std::to_string(smoothLevelPlug.asInt());

	optionMap["dv"] = smoothLevel;

	MPlug useGlobalSmoothDrawTypePlug = dagMesh.findPlug("useGlobalSmoothDrawType");
	assert(!useGlobalSmoothDrawTypePlug.isNull());

	int smoothingType = 0;

	// read from globals
	if (useGlobalSmoothDrawTypePlug.asInt() > 0 )
	{
		MGlobal::executeCommand("optionVar -q proxySubdivisionType", smoothingType);
		optionMap["sdt"] = std::to_string(smoothingType);
	}
	// read from node
	else 
	{
		MPlug smoothDrawTypePlug = dagMesh.findPlug("smoothDrawType");
		smoothingType = smoothDrawTypePlug.asInt();
		// for some reason smoothingType is always one bigger than needed when it is not 0
		if (smoothingType != 0)
		{
			smoothingType -= 1;
		}
		assert(!smoothDrawTypePlug.isNull());
		optionMap["sdt"] = std::to_string(smoothingType);
	}	

	if (smoothingType == 0) // maya catmull-clark
	{
		MPlug keepBordersPlug = dagMesh.findPlug("keepBorder");
		assert(!keepBordersPlug.isNull());

		optionMap["kb"] = std::to_string(keepBordersPlug.asInt());

		MPlug shouldSmoothUVs = dagMesh.findPlug("smoothUVs");
		assert(!shouldSmoothUVs.isNull());
		optionMap["suv"] = std::to_string(shouldSmoothUVs.asInt());

		MPlug propogateHardnessPlug = dagMesh.findPlug("propagateEdgeHardness");
		assert(!propogateHardnessPlug.isNull());
		
		optionMap["peh"] = std::to_string(propogateHardnessPlug.asInt());
	}
	else // OpenSubdiv catmull-clark
	{
		int uvBoundarySmoothType = 0;
		if (useGlobalSmoothDrawTypePlug.asInt() > 0)
		{
			MGlobal::executeCommand("optionVar -q proxySmoothOsdFvarBoundary", uvBoundarySmoothType);
			optionMap["ofb"] = std::to_string(uvBoundarySmoothType);
		}
		else
		{
			MPlug plug_uvSmooth = dagMesh.findPlug("osdFvarBoundary");
			assert(!plug_uvSmooth.isNull());
			optionMap["ofb"] = std::to_string(plug_uvSmooth.asInt());
		}
	}

	MString result;

	for (auto it = optionMap.begin(); it != optionMap.end(); ++it)
	{
		result = result + "-" + it->first.c_str() + " " + it->second.c_str() + " ";
	}

	return result;
}

MObject FireMaya::MeshTranslator::GenerateSmoothMesh(const MObject& object, const MObject& parent, MStatus& status)
{
	status = MStatus::kSuccess;

	// check if we need to generate a smooth mesh
	DependencyNode attributes(object);
	bool smoothPreview = attributes.getBool("displaySmoothMesh");

	if (!smoothPreview)
		return MObject::kNullObj;

	// remember current selection
	MSelectionList currentSelection;
	MGlobal::getActiveSelectionList(currentSelection); 

	// copy original mesh and smooth is via mel
	MFnDagNode dagMesh(object);
	MDagPath meshPath; 
	dagMesh.getPath(meshPath);
	MString meshName = meshPath.fullPathName(&status);

	assert(status == MStatus::kSuccess);
	if (status != MStatus::kSuccess)
		return MObject::kNullObj;

	MString options = GenerateSmoothOptions(dagMesh);

	MString command = R"(
		proc string generateSmoothMesh() 
		{
			$res = `duplicate -rc ^1s`;
			polySmooth ^2s $res[0];
			select -clear;
			select -add $res[0];
			return $res[0];
		}
		generateSmoothMesh();
	)";

	command.format(command, meshName, options);
	MString result;
	status = MGlobal::executeCommand(command, result);

	if (status != MStatus::kSuccess) // failed to generate smoothMesh
	{
		return MObject::kNullObj;
	}

	// find generated mesh by returned name
	MObject smoothedMesh = MObject::kNullObj;

	MSelectionList clonedMeshSelection;
	MGlobal::getActiveSelectionList(clonedMeshSelection);

	if (status != MStatus::kSuccess) // failed to find cloned object
		return MObject::kNullObj;

	int len = clonedMeshSelection.length();
	assert(len == 1);
	status = clonedMeshSelection.getDependNode(0, smoothedMesh);

	// set selection to previous selection
	MGlobal::setActiveSelectionList(currentSelection);

	// return created mesh
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

	const std::map<int, int> modeToTesParam = { 
										{ 1, MTesselationParams::kSurface3DEquiSpaced },
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

	MObject tesselated = surface.tesselate(params, parent, &status); // failure is possible and should be handled
	return tesselated;
}

MObject FireMaya::MeshTranslator::GetSmoothedObjectIfNecessary(const MObject& originalObject, MStatus& mstatus)
{
	MFnDagNode node(originalObject);

	DebugPrint("TranslateMesh: %s", node.fullPathName().asUTF8());

	MObject parent = node.parent(0);

	// tessellate to mesh if we aren't already one
	if (!originalObject.hasFn(MFn::kMesh))
	{
		return MObject::kNullObj;
	}

	// is mesh => can smooth
	MObject smoothed = GenerateSmoothMesh(originalObject, parent, mstatus);
	if (mstatus != MStatus::kSuccess)
	{
		mstatus.perror("MFnMesh::generateSmoothMesh");
		return MObject::kNullObj;
	}

	if (smoothed != MObject::kNullObj)
	{
		// get shape
		MDagPath createdMeshPath;
		MFnDagNode smoothedObj(smoothed);
		mstatus = smoothedObj.getPath(createdMeshPath);
		assert(mstatus == MStatus::kSuccess);
		createdMeshPath.extendToShape();
		smoothed = createdMeshPath.node();
	}

	return smoothed;
}

MObject FireMaya::MeshTranslator::GetTesselatedObjectIfNecessary(const MObject& originalObject, MStatus& mstatus)
{
#ifdef OPTIMIZATION_CLOCK
	std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
#endif
	MFnDagNode node(originalObject);

	DebugPrint("TranslateMesh: %s", node.fullPathName().asUTF8());

	MObject parent = node.parent(0);

	MObject tessellated = MObject::kNullObj;
	// tessellate to mesh
	if (originalObject.hasFn(MFn::kNurbsSurface))
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

void FireMaya::MeshTranslator::RemoveSmoothedTemporaryMesh(const MFnDagNode& node, MObject smoothed)
{
	FireRenderThread::RunProcOnMainThread([&]
		{
#ifdef OPTIMIZATION_CLOCK
			std::chrono::steady_clock::time_point start_del = std::chrono::steady_clock::now();
#endif
			MFnDagNode shapeNode(smoothed);
			assert(shapeNode.parentCount() == 1);
			MObject parent = shapeNode.parent(0);
			assert(!parent.isNull());

			MGlobal::deleteNode(parent);

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
	uvSetNames.clear();
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
