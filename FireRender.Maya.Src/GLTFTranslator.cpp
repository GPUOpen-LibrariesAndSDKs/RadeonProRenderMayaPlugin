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
#include "GLTFTranslator.h"

#include "Translators/Translators.h"
#include "Context/TahoeContext.h"

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

#include <memory>
#include <array>
#include <thread>
#include <float.h>

#include "ProRenderGLTF.h"

using namespace FireMaya;

const int COMPONENT_COUNT_ROTATION = 4;
const int COMPONENT_COUNT_TRANSLATION = 3;
const int COMPONENT_COUNT_SCALE = 3;

const int INPUT_PLUG_COUNT = 3;

GLTFTranslator::GLTFTranslator() = default;
GLTFTranslator::~GLTFTranslator() = default;

void* GLTFTranslator::creator()
{
	return new GLTFTranslator();
}

MStatus	GLTFTranslator::writer(const MFileObject& file,
	const MString& optionsString,
	FileAccessMode mode)
{
	// Create new context and fill it with scene
	std::unique_ptr<TahoeContext> fireRenderContext = std::make_unique<TahoeContext>();
	ContextAutoCleaner contextAutoCleaner(fireRenderContext.get());

	// to be sure RenderProgressBars will be closed upon function exit
	std::unique_ptr<RenderProgressBars> progressBars = std::make_unique<RenderProgressBars>(false);
	m_progressBars = progressBars.get();

	m_progressBars->SetWindowsTitleText("GLTF Export");
	m_progressBars->SetPreparingSceneText(true);

	fireRenderContext->setCallbackCreationDisabled(true);
	fireRenderContext->SetGLTFExport(true);

	fireRenderContext->SetWorkProgressCallback([this](const ContextWorkProgressData& syncProgressData)
		{
			if (syncProgressData.progressType == ProgressType::ObjectSyncComplete)
			{
				m_progressBars->update(syncProgressData.GetPercentProgress());
			}
		});

	if (!fireRenderContext->buildScene(false, false, true))
	{
		return MS::kFailure;
	}

	// Some resolution should be set. It's requred to retrieve background image.
	FireRenderGlobalsData renderData;
	
	unsigned int width = 800;	// use some defaults
	unsigned int height = 600;
	GetResolutionFromCommonTab(width, height);

	fireRenderContext->setResolution(width, height, true, 0);
	fireRenderContext->ConsiderSetupDenoiser();

	MStatus status;

	MDagPathArray renderableCameras = GetSceneCameras(true);
	if (renderableCameras.length() == 0)
	{
		MGlobal::displayError("No renderable cameras! Please setup at least one to be exported in GLTF!");
		return MS::kFailure;
	}

	fireRenderContext->setCamera(renderableCameras[0]);
	
	fireRenderContext->SetState(FireRenderContext::StateRendering);

	//Populate rpr scene with actual data
	if (!fireRenderContext->Freshen(false))
		return MS::kFailure;

	frw::Scene scene = fireRenderContext->GetScene();
	frw::Context context = fireRenderContext->GetContext();
	frw::MaterialSystem materialSystem = fireRenderContext->GetMaterialSystem();

	if (!scene || !context || !materialSystem)
		return MS::kFailure;

	try
	{
		m_progressBars->SetTextAboveProgress("Preparing Animation...", true);
		GLTFDataHolderStruct dataHolder;
		dataHolder.inputRenderableCameras = &renderableCameras;
		addGLTFAnimations(dataHolder, *fireRenderContext);

		std::vector<rpr_scene> scenes;
		scenes.push_back(scene.Handle());

		m_progressBars->SetTextAboveProgress("Saving to GLTF file", true);
		int err = rprExportToGLTF(
			file.expandedFullName().asChar(),
			context.Handle(),
			materialSystem.Handle(),
			scenes.data(),
			scenes.size(),
			RPRGLTF_EXPORTFLAG_COPY_IMAGES_USING_OBJECTNAME | RPRGLTF_EXPORTFLAG_KHR_LIGHT);

		if (err != GLTF_SUCCESS)
		{
			return MS::kFailure;
		}

		m_progressBars->SetTextAboveProgress("Done.");
	}
	catch (ExportCancelledException)
	{
		MGlobal::displayWarning("GLTF export was cancelled by the user");
		return MS::kFailure;
	}

	return MS::kSuccess;
}

MString GLTFTranslator::getGroupNameForDagPath(MDagPath dagPath, int pop)
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

void FillArrayWithScaledMMatrixData(std::array<float, 16>& arr, float coeff = 0.01f)
{
	MMatrix matrix;

	matrix[0][0] = coeff;
	matrix[1][1] = coeff;
	matrix[2][2] = coeff;

	FillArrayWithMMatrixData(arr, matrix);
}

void GLTFTranslator::assignCameras(GLTFDataHolderStruct& dataHolder, FireRenderContext& context)
{
	MDagPathArray& renderableCameras = *dataHolder.inputRenderableCameras;
	bool mainCameraSet = false;

	for (unsigned int i = 0; i < renderableCameras.length(); i++)
	{
		MDagPath dagPath = renderableCameras[i];

		MMatrix camIdentityMatrix;
		rpr_camera rprCamera;

		if (mainCameraSet)
		{
			frw::Camera extraCamera = context.GetContext().CreateCamera();
			dataHolder.cameraVector.push_back(extraCamera);

			rprCamera = extraCamera.Handle();

			int cameraType = FireRenderGlobals::kCameraDefault;

			FireMaya::translateCamera(extraCamera, dagPath.node(), camIdentityMatrix, true,
				(float)context.width() / context.height(), true, cameraType);

			rprGLTF_AddExtraCamera(rprCamera);
		}
		else
		{
			rprCamera = context.camera().data().Handle();
			SetCameraLookatForMatrix(rprCamera, camIdentityMatrix);
			mainCameraSet = true;
		}

		// set gltf group name for camera
		MString camGroupName = getGroupNameForDagPath(dagPath);
		rprGLTF_AssignCameraToGroup(rprCamera, camGroupName.asChar());
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

void assignMesh(FireRenderMesh* pMesh, const MString& groupName, const MDagPath& dagPath)
{
	for (auto& element : pMesh->Elements())
	{
		rprGLTF_AssignShapeToGroup(element.shape.Handle(), groupName.asChar());

		//Reset transform for shape since we already have transformation in parent groups
		std::array<float, 16> arr;
		FillArrayWithScaledMMatrixData(arr);
		element.shape.SetTransform(arr.data());

		int lightGroup = getUVLightGroup(dagPath);

		if (lightGroup >= 0)
		{
			rprGLTF_AddExtraShapeParameter(element.shape.Handle(), "lightmapUVGroup", lightGroup);
		}
	}
}

void assignLight(FireRenderLight* pLight, const MString& groupName)
{
	// this function was not included in dylib for some reason
#ifdef _WIN32
	rprGLTF_AssignLightToGroup(pLight->data().light.Handle(), groupName.asChar());
#endif
	//Reset transform for shape since we already have transformation in parent groups
	std::array<float, 16> arr;
	FillArrayWithScaledMMatrixData(arr);
	pLight->GetFrLight().light.SetTransform(arr.data());
}

void GLTFTranslator::assignMeshesAndLights(FireRenderContext& context)
{
	FireRenderContext::FireRenderObjectMap& sceneObjects = context.GetSceneObjects();
	// set gltf group name for meshes
	for (FireRenderContext::FireRenderObjectMap::iterator it = sceneObjects.begin(); it != sceneObjects.end(); ++it)
	{
		FireRenderNode* pNode = dynamic_cast<FireRenderNode*>(it->second.get());

		if (pNode == nullptr)
		{
			continue;
		}

		MDagPath dagPath = pNode->DagPath();

		if (!isNeedToSetANameForTransform(dagPath))
		{
			continue;
		}

		MString groupName = getGroupNameForDagPath(dagPath);

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

void GLTFTranslator::addGLTFAnimations(GLTFDataHolderStruct& dataHolder, FireRenderContext& context)
{
	assignCameras(dataHolder, context);
	assignMeshesAndLights(context);
	animateGLTFGroups(dataHolder.animationDataVector);
}

bool GLTFTranslator::isNeedToSetANameForTransform(const MDagPath& dagPath)
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

void GLTFTranslator::setGLTFTransformationForNode(MObject transform, const char* groupName)
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
	float coeff = 0.01f;
	vecTranslation *= coeff;

	int index = 0;
	for (int i = 0; i < 3; i++)
	{
		arr[index++] = (float) vecTranslation[i];
	}

	for (int i = 0; i < 4; i++)
	{
		arr[index++] = (float) rotation[i];
	}

	for (int i = 0; i < 3; i++)
	{
		arr[index++] = (float) scale[i];
	}

	rprGLTF_SetTransformGroup(groupName, arr.data());
}

void GLTFTranslator::animateGLTFGroups(GLTFAnimationDataHolderVector& dataHolder)
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

		if (!isNeedToSetANameForTransform(dagPath))
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

		MString transformGroupName = getGroupNameForDagPath(dagPath);

		setGLTFTransformationForNode(transform, transformGroupName.asChar());

		MString parentOfTransformGroupName = "";

		// if non-root transform
		if (dagPath.length() > 1)
		{
			parentOfTransformGroupName = getGroupNameForDagPath(dagPath, 1);
		}

		rprGLTF_AssignParentGroupToGroup(transformGroupName.asChar(), parentOfTransformGroupName.asChar());

		MSelectionList selList;
		selList.add(transform);

		if (!MAnimUtil::isAnimated(selList))
		{
			// skip transform if it is not animated
			continue;
		}

		applyGLTFAnimationForTransform(dagPath, dataHolder);

		ReportProgress((int)(100 * (i + 1) / groupDagPathVector.size()));
	}
}

MString GLTFTranslator::getGLTFAttributeNameById(int id)
{
	switch (id)
	{
	case RPRGLTF_ANIMATION_MOVEMENTTYPE_TRANSLATION:
		return "translate";
	case RPRGLTF_ANIMATION_MOVEMENTTYPE_ROTATION:
		return "rotate";
	case RPRGLTF_ANIMATION_MOVEMENTTYPE_SCALE:
		return "scale";
	}
    
	assert(false);
	return "";
}

int GLTFTranslator::getOutputComponentCount(int attrId)
{
	switch (attrId)
	{
	case RPRGLTF_ANIMATION_MOVEMENTTYPE_TRANSLATION:
		return COMPONENT_COUNT_TRANSLATION;
	case RPRGLTF_ANIMATION_MOVEMENTTYPE_SCALE:
		return COMPONENT_COUNT_SCALE;
	case RPRGLTF_ANIMATION_MOVEMENTTYPE_ROTATION:
		return COMPONENT_COUNT_ROTATION;
	}
    
	assert(false);
	return 0;
}

void GLTFTranslator::addTimesFromCurve(const MFnAnimCurve& curve, TimeKeySet& outUniqueTimeKeySet, int attributeId)
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

		addOneTimePoint(time, curve, outUniqueTimeKeySet, attributeId, keyIndex);
	}

	// Add auto point for the start and end animation point
	addOneTimePoint(startTime, curve, outUniqueTimeKeySet, attributeId, 0);
	addOneTimePoint(endTime, curve, outUniqueTimeKeySet, attributeId, keyCount - 1);
}

void GLTFTranslator::addOneTimePoint(const MTime time, const MFnAnimCurve& curve, TimeKeySet& outUniqueTimeKeySet, int attributeId, int keyIndex)
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
	if (attributeId == RPRGLTF_ANIMATION_MOVEMENTTYPE_ROTATION)
	{
		it->AddNewAttribute(RPRGLTF_ANIMATION_MOVEMENTTYPE_TRANSLATION);
	}

	// keys autogeneration for rotation
	if ((attributeId == RPRGLTF_ANIMATION_MOVEMENTTYPE_ROTATION) && (keyIndex > 0))
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

inline float GLTFTranslator::getValueForTime(const MPlug& plug, const MFnAnimCurve& curve, const MTime& time)
{
	if (!curve.object().isNull())
	{
		return (float) curve.evaluate(time);
	}
	else
	{
	return plug.asFloat();
	}
}

void GLTFTranslator::addAnimationToGLTFRPR(GLTFAnimationDataHolderStruct& gltfDataHolderStruct, int attrId)
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

void GLTFTranslator::applyGLTFAnimationForTransform(const MDagPath& dagPath, GLTFAnimationDataHolderVector& gltfDataHolder)
{
	// do not change order
	const int attrCount = 3;
	int attrIds[attrCount] = { RPRGLTF_ANIMATION_MOVEMENTTYPE_TRANSLATION,
								RPRGLTF_ANIMATION_MOVEMENTTYPE_ROTATION,
								RPRGLTF_ANIMATION_MOVEMENTTYPE_SCALE };

	MString groupName = getGroupNameForDagPath(dagPath);

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
		MString attributeName = getGLTFAttributeNameById(attributeId);

		MPlugArray plugArray;

		for (int i = 0; i < inputPlugCount; ++i)
		{
			MString plugName = attributeName + componentNames[i];
			MPlug plug = depNodeTransform.findPlug(plugName, false, &status);
			if (status != MStatus::kSuccess || plug.isNull())
			{
				MGlobal::displayError("GLTF export error: Necessary plug not found: " + plugName);
				continue;
			}

			plugArray.append(plug);
			MObjectArray curveObj;

			if (MAnimUtil::findAnimation(plug, curveObj, &status))
			{
				tempCurve.setObject(curveObj[0]);
				addTimesFromCurve(tempCurve, uniqueTimeKeys, attributeId);
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
		gltfDataHolder.emplace(gltfDataHolder.end());
		GLTFAnimationDataHolderStruct& gltfDataHolderStruct = gltfDataHolder.back();

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

			gltfDataHolderStruct.m_timePoints.push_back((float) it->GetTime().as(MTime::Unit::kSeconds));

			switch (attributeId)
			{
			case RPRGLTF_ANIMATION_MOVEMENTTYPE_TRANSLATION:
			{
				MVector vec1 = transformMatrix.getTranslation(MSpace::kTransform);
				//cm to m
				float coeff = 0.01f;

				gltfDataHolderStruct.m_values.push_back( (float) vec1.x * coeff );
				gltfDataHolderStruct.m_values.push_back( (float) vec1.y * coeff );
				gltfDataHolderStruct.m_values.push_back( (float) vec1.z * coeff );

				break;
			}
			case RPRGLTF_ANIMATION_MOVEMENTTYPE_ROTATION:
			{
				MQuaternion rotation = transformMatrix.rotation();
				gltfDataHolderStruct.m_values.push_back((float)rotation.x);
				gltfDataHolderStruct.m_values.push_back((float)rotation.y);
				gltfDataHolderStruct.m_values.push_back((float)rotation.z);
				gltfDataHolderStruct.m_values.push_back((float)rotation.w);
				break;
			}
			case RPRGLTF_ANIMATION_MOVEMENTTYPE_SCALE:
			{
				double scale[3];
				transformMatrix.getScale(scale, MSpace::kTransform);

				gltfDataHolderStruct.m_values.push_back((float)scale[0]);
				gltfDataHolderStruct.m_values.push_back((float)scale[1]);
				gltfDataHolderStruct.m_values.push_back((float)scale[2]);
				break;
			}
			}

			if (m_progressBars->isCancelled())
			{
				throw ExportCancelledException();
			}

			if (dataChunkIndex % 100 == 0)
			{
				ReportDataChunk(dataChunkIndex, attrCount * uniqueTimeKeys.size());
			}
		}

		gltfDataHolderStruct.groupName = groupName;

		if (gltfDataHolderStruct.m_timePoints.size() > 0)
		{
			addAnimationToGLTFRPR(gltfDataHolderStruct, attributeId);
		}
	}
}

void GLTFTranslator::ReportDataChunk(size_t dataChunkIndex, size_t count)
{
	std::ostringstream stream;

	stream << "Preparing Animation...current object: " << dataChunkIndex << " / " << count;
	
	m_progressBars->SetTextAboveProgress(stream.str(), true);
}

void GLTFTranslator::ReportProgress(int progress)
{
	m_progressBars->update(progress);
}

void GLTFTranslator::reportGLTFExportError(MString strPath)
{
	MGlobal::displayError("GLTF export error: cannot get animation for transform: " + strPath);
}

