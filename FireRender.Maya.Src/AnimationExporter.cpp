#include "AnimationExporter.h"
#include "RprLoadStore.h"

#include <maya/MItDag.h>
#include <maya/MGlobal.h>
#include <maya/MDagPath.h>

#include <maya/MAnimUtil.h>
#include <maya/MTime.h>
#include <maya/MQuaternion.h>
#include <maya/MFnTransform.h>
#include <maya/MSelectionList.h>

#include <maya/MFnMatrixData.h>
#include <maya/MPlugArray.h>
#include <maya/MAnimControl.h>

const int COMPONENT_COUNT_ROTATION = 4;
const int COMPONENT_COUNT_TRANSLATION = 3;
const int COMPONENT_COUNT_SCALE = 3;

const int INPUT_PLUG_COUNT = 3;

frw::RPRSContext g_exportContext;

AnimationExporter::AnimationExporter(bool gltfExport) :
	m_IsGLTFExport(gltfExport),
	m_progressBars(nullptr)
{
	if (m_IsGLTFExport)
	{
		m_runtimeMoveTypeTranslation = RPRGLTF_ANIMATION_MOVEMENTTYPE_TRANSLATION;
		m_runtimeMoveTypeRotation = RPRGLTF_ANIMATION_MOVEMENTTYPE_ROTATION;
		m_runtimeMoveTypeScale = RPRGLTF_ANIMATION_MOVEMENTTYPE_SCALE;

		m_pFunc_AddExtraCamera = rprGLTF_AddExtraCamera;
		m_pFunc_AssignCameraToGroup = rprGLTF_AssignCameraToGroup;
		m_pFunc_AddExtraShapeParameter = rprGLTF_AddExtraShapeParameter;
		m_pFunc_AssignLightToGroup = rprGLTF_AssignLightToGroup;
		m_pFunc_AssignShapeToGroup = rprGLTF_AssignShapeToGroup;
		m_pFunc_SetTransformGroup = rprGLTF_SetTransformGroup;
		m_pFunc_AssignParentGroupToGroup = rprGLTF_AssignParentGroupToGroup;

		m_pFunc_AddAnimationTrackToRPR = &AnimationExporter::AddAnimationToGLTFRPR;
	}
	else
	{
		m_runtimeMoveTypeTranslation = RPRS_ANIMATION_MOVEMENTTYPE_TRANSLATION;
		m_runtimeMoveTypeRotation = RPRS_ANIMATION_MOVEMENTTYPE_ROTATION;
		m_runtimeMoveTypeScale = RPRS_ANIMATION_MOVEMENTTYPE_SCALE;

		m_pFunc_AddExtraCamera = [](rpr_camera extraCam) { return rprsAddExtraCameraEx(g_exportContext.Handle(), extraCam); };
		m_pFunc_AddExtraShapeParameter = [](rpr_shape shape, const rpr_char* parameterName, rpr_int value) { return rprsAddExtraShapeParameterEx(g_exportContext.Handle(), shape, parameterName, value); };
		m_pFunc_AssignLightToGroup = [](rpr_light light, const rpr_char* groupName) { return rprsAssignLightToGroupEx(g_exportContext.Handle(), light, groupName); };

		m_pFunc_AssignCameraToGroup = [](rpr_camera camera, const rpr_char* groupName) { return rprsAssignCameraToGroupEx(g_exportContext.Handle(), camera, groupName); };
		m_pFunc_AssignShapeToGroup = [](rpr_shape shape, const rpr_char* groupName) { return rprsAssignShapeToGroupEx(g_exportContext.Handle(), shape, groupName); };
		m_pFunc_SetTransformGroup = [](const rpr_char* groupChild, const float* matrixComponents) { return rprsSetTransformGroupEx(g_exportContext.Handle(), groupChild, matrixComponents); };
		m_pFunc_AssignParentGroupToGroup = [](const rpr_char* groupChild, const rpr_char* groupParent) { return rprsAssignParentGroupToGroupEx(g_exportContext.Handle(), groupChild, groupParent); };

		m_pFunc_AddAnimationTrackToRPR = &AnimationExporter::AddAnimationToRPRS;
	}
}

void AnimationExporter::Export(FireRenderContext& context, MDagPathArray* renderableCamera, frw::RPRSContext exportContext)
{
	g_exportContext = exportContext;

	m_dataHolder.animationDataVector.clear();
	m_dataHolder.cameraVector.clear();
	m_dataHolder.inputRenderableCameras = renderableCamera;

	AddAnimations(m_dataHolder, context);

	g_exportContext.Reset();
}

MString AnimationExporter::GetGroupNameForDagPath(MDagPath dagPath, int pop)
{
	if (pop > 0)
	{
		dagPath.pop();
	}

	if (dagPath.node().hasFn(MFn::kTransform))
	{
		return dagPath.fullPathName();
	}
	else
	{
		// if dagPath points to shape, we need to do pop 1 to get path poiting to transform
		dagPath.pop();
		return dagPath.fullPathName();
	}
}

void FillArrayWithMMatrixData(std::array<float, 16>& arr, const MMatrix& matrix)
{
	for (int i = 0; i < 4; i++)
		for (int j = 0; j < 4; j++)
		{
			arr[i * 4 + j] = (float)matrix(i, j);
		}
}

void FillArrayWithScaledMMatrixData(std::array<float, 16>& arr, float coeff = GetSceneUnitsConversionCoefficient())
{
	MMatrix matrix;

	matrix[0][0] = coeff;
	matrix[1][1] = coeff;
	matrix[2][2] = coeff;

	FillArrayWithMMatrixData(arr, matrix);
}

void AnimationExporter::AssignCameras(DataHolderStruct& dataHolder, FireRenderContext& context)
{
	MDagPathArray& renderableCameras = *dataHolder.inputRenderableCameras;

	MDagPath mainCameraPath = context.camera().DagPath();

	for (unsigned int i = 0; i < renderableCameras.length(); i++)
	{
		MDagPath dagPath = renderableCameras[i];

		MMatrix camIdentityMatrix;
		rpr_camera rprCamera;

		MString camGroupName = GetGroupNameForDagPath(dagPath);

		if (mainCameraPath.fullPathName() != dagPath.fullPathName())
		{
			frw::Camera extraCamera = context.GetContext().CreateCamera();
			dataHolder.cameraVector.push_back(extraCamera);

			rprCamera = extraCamera.Handle();

			int cameraType = FireRenderGlobals::kCameraDefault;

			FireMaya::translateCamera(extraCamera, dagPath.node(), camIdentityMatrix, true,
				(float)context.width() / context.height(), true, cameraType);

			if (m_pFunc_AddExtraCamera != nullptr)
			{
				m_pFunc_AddExtraCamera(rprCamera);
			}
		}
		else
		{
			rprCamera = context.camera().data().Handle();
			SetCameraLookatForMatrix(rprCamera, camIdentityMatrix);
		}

		// set gltf/rprs group name for camera
		m_pFunc_AssignCameraToGroup(rprCamera, camGroupName.asChar());
	}
}

int getUVLightGroup(const MDagPath& inDagPath)
{
	MDagPath dagPath = inDagPath;

	while (dagPath.transform() != MObject())
	{
		MFnTransform transform(dagPath.transform());

		MPlug plug = transform.findPlug("lightmapUVGroup");

		if (!plug.isNull())
		{
			return plug.asInt();
		}

		dagPath.pop();
	}

	return -1;
}

void AnimationExporter::assignMesh(FireRenderMesh* pMesh, const MString& groupName, const MDagPath& dagPath)
{
	auto& elements = pMesh->Elements();
	for (auto& element : elements)
	{
		int res = m_pFunc_AssignShapeToGroup(element.shape.Handle(), groupName.asChar());
		assert(res == RPR_SUCCESS);

		//Reset transform for shape since we already have transformation in parent groups
		std::array<float, 16> arr;
		FillArrayWithScaledMMatrixData(arr);
		element.shape.SetTransform(arr.data());

		int lightGroup = getUVLightGroup(dagPath);

		if (lightGroup >= 0)
		{
			if (m_pFunc_AddExtraShapeParameter != nullptr)
			{
				m_pFunc_AddExtraShapeParameter(element.shape.Handle(), "lightmapUVGroup", lightGroup);
			}
		}
	}
}

void AnimationExporter::assignLight(FireRenderLight* pLight, const MString& groupName)
{
	// this function was not included in dylib for some reason
#ifdef _WIN32
	m_pFunc_AssignLightToGroup(pLight->data().light.Handle(), groupName.asChar());
#endif
	//Reset transform for shape since we already have transformation in parent groups
	std::array<float, 16> arr;
	FillArrayWithScaledMMatrixData(arr);
	pLight->GetFrLight().light.SetTransform(arr.data());
}

void AnimationExporter::AssignMeshesAndLights(FireRenderContext& context)
{
	FireRenderContext::FireRenderObjectMap& sceneObjects = context.GetSceneObjects();
	// set gltf/rprs group name for meshes
	for (FireRenderContext::FireRenderObjectMap::iterator it = sceneObjects.begin(); it != sceneObjects.end(); ++it)
	{
		FireRenderNode* pNode = dynamic_cast<FireRenderNode*>(it->second.get());

		if (pNode == nullptr)
		{
			continue;
		}

		MDagPath dagPath = pNode->DagPath();

		if (!IsNeedToSetANameForTransform(dagPath))
		{
			continue;
		}

		MString groupName = GetGroupNameForDagPath(dagPath);

		FireRenderMesh* pMesh = dynamic_cast<FireRenderMesh*>(pNode);
		FireRenderLight* pLight = dynamic_cast<FireRenderLight*>(pNode);

		if (pMesh != nullptr)
		{
			assignMesh(pMesh, groupName, dagPath);
		}
		else if (pLight != nullptr && !pLight->data().isAreaLight)
		{
			assignLight(pLight, groupName);
		}
	}
}

void AnimationExporter::AddAnimations(DataHolderStruct& dataHolder, FireRenderContext& context)
{
	AssignCameras(dataHolder, context);
	AssignMeshesAndLights(context);
	AnimateGroups(dataHolder.animationDataVector);
}

bool AnimationExporter::IsNeedToSetANameForTransform(const MDagPath& dagPath)
{
	MObject transform = dagPath.transform();

	MFnDagNode transformDagNode(transform);

	if (transformDagNode.childCount() == 0)
	{
		// skip transform if it has no children
		return false;
	}

	return true;
}

void AnimationExporter::SetTransformationForNode(MObject transform, const char* groupName)
{
	MFnDependencyNode fnTransform(transform);

	MPlug matrixPlug = fnTransform.findPlug("matrix");

	MMatrix newMatrix;
	MObject val;

	matrixPlug.getValue(val);
	newMatrix = MFnMatrixData(val).matrix();
	MTransformationMatrix transformMatrix(newMatrix);

	std::array<float, 10> arr;

	MVector vecTranslation = transformMatrix.getTranslation(MSpace::kTransform);

	MQuaternion rotation = transformMatrix.rotation();

	double scale[3];
	transformMatrix.getScale(scale, MSpace::kTransform);

	//cm to m
	vecTranslation *= GetSceneUnitsConversionCoefficient();

	int index = 0;
	for (int i = 0; i < 3; i++)
	{
		arr[index++] = (float)vecTranslation[i];
	}

	for (int i = 0; i < 4; i++)
	{
		arr[index++] = (float)rotation[i];
	}

	for (int i = 0; i < 3; i++)
	{
		arr[index++] = (float)scale[i];
	}

	m_pFunc_SetTransformGroup(groupName, arr.data());
}

void AnimationExporter::AnimateGroups(AnimationDataHolderVector& dataHolder)
{
	MStatus status;

	std::vector<MDagPath> groupDagPathVector;

	MItDag itDag(MItDag::kDepthFirst, MFn::kDagNode, &status);
	if (MStatus::kSuccess != status)
		MGlobal::displayError("MItDag::MItDag iteration not possible?!");

	for (; !itDag.isDone(); itDag.next())
	{
		MDagPath dagPath;
		itDag.getPath(dagPath);

		if (!dagPath.node().hasFn(MFn::kTransform))
		{
			continue;
		}

		if (!IsNeedToSetANameForTransform(dagPath))
		{
			continue;
		}

		groupDagPathVector.push_back(dagPath);
	}

	MDagPath dagPath;
	for (size_t i = 0; i < groupDagPathVector.size(); ++i)
	{
		dagPath = groupDagPathVector[i];

		MObject transform = dagPath.transform();

		MString transformGroupName = GetGroupNameForDagPath(dagPath);

		SetTransformationForNode(transform, transformGroupName.asChar());

		MString parentOfTransformGroupName = "";

		// if non-root transform
		if (dagPath.length() > 1)
		{
			parentOfTransformGroupName = GetGroupNameForDagPath(dagPath, 1);
		}

		m_pFunc_AssignParentGroupToGroup(transformGroupName.asChar(), parentOfTransformGroupName.asChar());

		MSelectionList selList;
		selList.add(transform);

		if (!MAnimUtil::isAnimated(selList))
		{
			// skip transform if it is not animated
			continue;
		}

		ApplyAnimationForTransform(dagPath, dataHolder);

		ReportProgress((int)(100 * (i + 1) / groupDagPathVector.size()));
	}
}

MString AnimationExporter::GetAttributeNameById(int id)
{
	if (id == m_runtimeMoveTypeTranslation)
	{
		return "translate";
	}
	else if (id == m_runtimeMoveTypeRotation)
	{
		return "rotate";
	}
	else if (id == m_runtimeMoveTypeScale)
	{
		return "scale";
	}

	assert(false);
	return "";
}

int AnimationExporter::GetOutputComponentCount(int attrId)
{
	if (attrId == m_runtimeMoveTypeTranslation)
	{
		return COMPONENT_COUNT_TRANSLATION;
	}
	else if (attrId == m_runtimeMoveTypeRotation)
	{
		return COMPONENT_COUNT_SCALE;
	}
	else if (attrId == m_runtimeMoveTypeScale)
	{
		return COMPONENT_COUNT_ROTATION;
	}

	assert(false);
	return 0;
}

void AnimationExporter::AddTimesFromCurve(const MFnAnimCurve& curve, TimeKeySet& outUniqueTimeKeySet, int attributeId)
{
	int keyCount = curve.numKeys();

	MTime startTime = MAnimControl::animationStartTime();
	MTime endTime = MAnimControl::animationEndTime();

	for (int keyIndex = 0; keyIndex < keyCount; ++keyIndex)
	{
		MTime time = curve.time(keyIndex);

		if ((time < startTime) || (time > endTime))
		{
			continue;
		}

		AddOneTimePoint(time, curve, outUniqueTimeKeySet, attributeId, keyIndex);
	}

	// Add auto point for the start and end animation point
	AddOneTimePoint(startTime, curve, outUniqueTimeKeySet, attributeId, 0);
	AddOneTimePoint(endTime, curve, outUniqueTimeKeySet, attributeId, keyCount - 1);
}

void AnimationExporter::AddOneTimePoint(const MTime time, const MFnAnimCurve& curve, TimeKeySet& outUniqueTimeKeySet, int attributeId, int keyIndex)
{
	TimeKeyStruct timeKey(time, attributeId);

	TimeKeySet::iterator it = outUniqueTimeKeySet.find(timeKey);
	if (it != outUniqueTimeKeySet.end())
	{
		it->AddNewAttribute(attributeId);
	}
	else
	{
		it = outUniqueTimeKeySet.insert(timeKey).first;
	}

	// if we process rotation attribute we should as translation as well because in some complex rotations translation might be changed as well
	if (attributeId == m_runtimeMoveTypeRotation)
	{
		it->AddNewAttribute(m_runtimeMoveTypeTranslation);
	}

	// keys autogeneration for rotation
	if ((attributeId == m_runtimeMoveTypeRotation) && (keyIndex > 0))
	{
		double maxValue = curve.value(keyIndex);
		double minValue = curve.value(keyIndex - 1);

		double step = M_PI / 2;
		double currentValue = minValue + step;
		MTime prevTime = curve.time(keyIndex - 1);
		MTime maxTime = time;
		while (currentValue < maxValue)
		{
			MTime additionalTimePoint = prevTime + (maxTime - prevTime) * (currentValue - minValue) / (maxValue - minValue);
			outUniqueTimeKeySet.insert(TimeKeyStruct(additionalTimePoint, attributeId));

			currentValue += step;
		}
	}
}

inline float AnimationExporter::GetValueForTime(const MPlug& plug, const MFnAnimCurve& curve, const MTime& time)
{
	if (!curve.object().isNull())
	{
		return (float)curve.evaluate(time);
	}
	else
	{
		return plug.asFloat();
	}
}

void AnimationExporter::AddAnimationToGLTFRPR(AnimationDataHolderStruct& gltfDataHolderStruct, int attrId)
{
	size_t keyCount = gltfDataHolderStruct.m_timePoints.size();

	rprgltf_animation gltfAnimData;
	gltfAnimData.structSize = sizeof(gltfAnimData);

	// RPR GLTF takes char* that's why const cast is needed
	gltfAnimData.groupName = const_cast<char*>(gltfDataHolderStruct.groupName.asChar());
	gltfAnimData.movementType = attrId;
	gltfAnimData.interpolationType = 0;
	gltfAnimData.nbTimeKeys = (unsigned int)keyCount;
	gltfAnimData.nbTransformValues = (unsigned int)keyCount;
	gltfAnimData.timeKeys = gltfDataHolderStruct.m_timePoints.data();
	gltfAnimData.transformValues = gltfDataHolderStruct.m_values.data();

	int res = rprGLTF_AddAnimation(&gltfAnimData);
	if (res != RPR_SUCCESS)
	{
		MGlobal::displayError("rprGLTF_AddAnimation returned error: ");
	}
}

void AnimationExporter::AddAnimationToRPRS(AnimationDataHolderStruct& dataHolderStruct, int attrId)
{
	size_t keyCount = dataHolderStruct.m_timePoints.size();

	rprs_animation rprsAnimData;
	rprsAnimData.structSize = sizeof(rprsAnimData);

	// RPRs takes char* that's why const cast is needed
	rprsAnimData.groupName = const_cast<char*>(dataHolderStruct.groupName.asChar());
	rprsAnimData.movementType = attrId;
	rprsAnimData.interpolationType = 0;
	rprsAnimData.nbTimeKeys = (unsigned int)keyCount;
	rprsAnimData.nbTransformValues = (unsigned int)keyCount;
	rprsAnimData.timeKeys = dataHolderStruct.m_timePoints.data();
	rprsAnimData.transformValues = dataHolderStruct.m_values.data();

	int res = rprsAddAnimationEx(g_exportContext.Handle(), &rprsAnimData);
	if (res != RPR_SUCCESS)
	{
		MGlobal::displayError("rprGLTF_AddAnimation returned error: ");
	}
}


void AnimationExporter::ApplyAnimationForTransform(const MDagPath& dagPath, AnimationDataHolderVector& dataHolder)
{
	// do not change order
	const int attrCount = 3;
	int attrIds[attrCount] = { m_runtimeMoveTypeTranslation,
								m_runtimeMoveTypeRotation,
								m_runtimeMoveTypeScale };

	MString groupName = GetGroupNameForDagPath(dagPath);

	MFnDependencyNode depNodeTransform(dagPath.transform());
	MFnTransform fnTransform(dagPath.transform());

	const int inputPlugCount = INPUT_PLUG_COUNT; // it is always x, y, z as inputs

	MStatus status;

	MFnAnimCurve tempCurve;

	TimeKeySet uniqueTimeKeys;

	// Gather Unique key points
	MString componentNames[inputPlugCount] = { "X", "Y", "Z" };
	for (int attributeId : attrIds)
	{
		MString attributeName = GetAttributeNameById(attributeId);

		MPlugArray plugArray;

		for (int i = 0; i < inputPlugCount; ++i)
		{
			MString plugName = attributeName + componentNames[i];
			MPlug plug = depNodeTransform.findPlug(plugName, false, &status);
			if (status != MStatus::kSuccess || plug.isNull())
			{
				MGlobal::displayError("GLTF/RPRS export error: Necessary plug not found: " + plugName);
				continue;
			}

			plugArray.append(plug);
			MObjectArray curveObj;

			if (MAnimUtil::findAnimation(plug, curveObj, &status))
			{
				tempCurve.setObject(curveObj[0]);
				AddTimesFromCurve(tempCurve, uniqueTimeKeys, attributeId);
			}
		}
	}

	MPlug matrixPlug = depNodeTransform.findPlug("matrix", &status);

	size_t keyCount = uniqueTimeKeys.size();

	// this is just for progress reporting
	size_t dataChunkIndex = 0;

	// Export necessary attributes
	for (int attributeId : attrIds)
	{
		dataHolder.emplace(dataHolder.end());
		AnimationDataHolderStruct& dataHolderStruct = dataHolder.back();

		for (TimeKeySet::iterator it = uniqueTimeKeys.begin(); it != uniqueTimeKeys.end(); it++)
		{
			dataChunkIndex++;

			if (!it->DoesAttributeIdPresent(attributeId))
			{
				continue;
			}

			MTime time = it->GetTime();
			MDGContext dgContext(time);

			MMatrix newMatrix;
			MObject val;

			matrixPlug.getValue(val, dgContext);
			newMatrix = MFnMatrixData(val).matrix();

			MTransformationMatrix transformMatrix(newMatrix);

			dataHolderStruct.m_timePoints.push_back((float)it->GetTime().as(MTime::Unit::kSeconds));

			if (attributeId == m_runtimeMoveTypeTranslation)
			{
				MVector vec1 = transformMatrix.getTranslation(MSpace::kTransform);
				//cm to m
				dataHolderStruct.m_values.push_back((float)vec1.x * GetSceneUnitsConversionCoefficient());
				dataHolderStruct.m_values.push_back((float)vec1.y * GetSceneUnitsConversionCoefficient());
				dataHolderStruct.m_values.push_back((float)vec1.z * GetSceneUnitsConversionCoefficient());
			}
			else if (attributeId == m_runtimeMoveTypeRotation)
			{
				MQuaternion rotation = transformMatrix.rotation();
				dataHolderStruct.m_values.push_back((float)rotation.x);
				dataHolderStruct.m_values.push_back((float)rotation.y);
				dataHolderStruct.m_values.push_back((float)rotation.z);
				dataHolderStruct.m_values.push_back((float)rotation.w);
			}
			else if (attributeId == m_runtimeMoveTypeScale)
			{
				double scale[3];
				transformMatrix.getScale(scale, MSpace::kTransform);

				dataHolderStruct.m_values.push_back((float)scale[0]);
				dataHolderStruct.m_values.push_back((float)scale[1]);
				dataHolderStruct.m_values.push_back((float)scale[2]);
			}

			if (m_progressBars != nullptr && m_progressBars->isCancelled())
			{
				throw ExportCancelledException();
			}

			if (dataChunkIndex % 100 == 0)
			{
				ReportDataChunk(dataChunkIndex, attrCount * uniqueTimeKeys.size());
			}
		}

		dataHolderStruct.groupName = groupName;

		if (dataHolderStruct.m_timePoints.size() > 0)
		{
			(this->*m_pFunc_AddAnimationTrackToRPR)(dataHolderStruct, attributeId);
		}
	}
}

void AnimationExporter::ReportDataChunk(size_t dataChunkIndex, size_t count)
{
	std::ostringstream stream;

	stream << "Preparing Animation...current object: " << dataChunkIndex << " / " << count;

	if (m_progressBars != nullptr)
	{
		m_progressBars->SetTextAboveProgress(stream.str(), true);
	}
}

void AnimationExporter::ReportProgress(int progress)
{
	if (m_progressBars != nullptr)
	{
		m_progressBars->update(progress);
	}
}

void AnimationExporter::ReportGLTFExportError(MString strPath)
{
	MGlobal::displayError("GLTF/RPRs export error: cannot get animation for transform: " + strPath);
}
