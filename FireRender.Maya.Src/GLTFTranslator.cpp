#include "GLTFTranslator.h"

#include "Translators/Translators.h"

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


#include <array>

#include <thread>

#ifdef OPAQUE
#undef OPAQUE
#include "ProRenderGLTF.h"
#endif

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
#if defined(OSMac_)
    return MStatus::kFailure;
#else
	// Create new context and fill it with scene
	std::unique_ptr<FireRenderContext> fireRenderContext = std::make_unique<FireRenderContext>();
	ContextAutoCleaner contextAutoCleaner(fireRenderContext.get());

	fireRenderContext->setCallbackCreationDisabled(true);
	if (!fireRenderContext->buildScene(false, false, false, true))
		return MS::kFailure;

	// Some resolution should be set. It's requred to retrieve background image.
	// We set 800x600 here but we can set any other resolution.
	fireRenderContext->setResolution(800, 600, false, 0);

	MStatus status;

	MDagPathArray renderableCameras = getRenderableCameras();
	if (renderableCameras.length() == 0)
	{
		MGlobal::displayError("No renderable cameras! Please setup at least one to be exported in GLTF!");
		return MS::kFailure;
	}

	fireRenderContext->setCamera(renderableCameras[0]);
	
	fireRenderContext->state = FireRenderContext::StateRendering;

	//Populate rpr scene with actual data
	if (!fireRenderContext->Freshen(false, [this]() { return false; }))
		return MS::kFailure;

	frw::Scene scene = fireRenderContext->GetScene();
	frw::Context context = fireRenderContext->GetContext();
	frw::MaterialSystem materialSystem = fireRenderContext->GetMaterialSystem();

	if (!scene || !context || !materialSystem)
		return MS::kFailure;

	GLTFDataHolderStruct dataHolder;
	dataHolder.inputRenderableCameras = &renderableCameras;
	addGLTFAnimations(dataHolder, *fireRenderContext);

	std::vector<rpr_scene> scenes;
	scenes.push_back(scene.Handle());

#if (RPR_API_VERSION >= 0x010032500)
	int err = rprExportToGLTF(file.expandedFullName().asChar(), context.Handle(), materialSystem.Handle(), materialSystem.GetRprxContext(), scenes.data(), scenes.size(), 0);
#else
	int err = rprExportToGLTF(file.expandedFullName().asChar(), context.Handle(), materialSystem.Handle(), materialSystem.GetRprxContext(), scenes.data(), scenes.size());
#endif

	if (err != GLTF_SUCCESS)
	{
		return MS::kFailure;
	}

	return MS::kSuccess;
#endif
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
#if !defined(OSMac_)
	MDagPathArray& renderableCameras = *dataHolder.inputRenderableCameras;
	bool mainCameraSet = false;

	for (int i = 0; i < renderableCameras.length(); i++)
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

		// set rpr name
		rprObjectSetName(rprCamera, dagPath.partialPathName().asChar());
	}
#endif
}

void GLTFTranslator::assignMeshes(FireRenderContext& context)
{
#if !defined(OSMac_)
	FireRenderContext::FireRenderObjectMap& sceneObjects = context.GetSceneObjects();
	// set gltf group name for meshes
	for (FireRenderContext::FireRenderObjectMap::iterator it = sceneObjects.begin(); it != sceneObjects.end(); ++it)
	{
		FireRenderMesh* pMesh = dynamic_cast<FireRenderMesh*>(it->second.get());

		if (pMesh == nullptr)
		{
			continue;
		}

		MDagPath dagPath = pMesh->DagPath();

		if (!isNeedToSetANameForTransform(dagPath))
		{
			continue;
		}

		MString groupName = getGroupNameForDagPath(dagPath);

		for (auto& element : pMesh->Elements())
		{
			rprGLTF_AssignShapeToGroup(element.shape.Handle(), groupName.asChar());

			//Reset transform for shape since we already have transformation in parent groups
			std::array<float, 16> arr;
			FillArrayWithScaledMMatrixData(arr);
			element.shape.SetTransform(arr.data());
		}
	}
#endif
}

void GLTFTranslator::addGLTFAnimations(GLTFDataHolderStruct& dataHolder, FireRenderContext& context)
{
	assignCameras(dataHolder, context);
	assignMeshes(context);
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
#if !defined(OSMac_)
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
#endif
}

void GLTFTranslator::animateGLTFGroups(GLTFAnimationDataHolderVector& dataHolder)
{
#if !defined(OSMac_)
	MStatus status;

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

		MObject transform = dagPath.transform();

		if (!isNeedToSetANameForTransform(dagPath))
		{
			continue;
		}

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
	}
#endif
}

MString GLTFTranslator::getGLTFAttributeNameById(int id)
{
#if !defined(OSMac_)
	switch (id)
	{
	case RPRGLTF_ANIMATION_MOVEMENTTYPE_TRANSLATION:
		return "translate";
	case RPRGLTF_ANIMATION_MOVEMENTTYPE_ROTATION:
		return "rotate";
	case RPRGLTF_ANIMATION_MOVEMENTTYPE_SCALE:
		return "scale";
	}
#endif
    
	assert(false);
	return "";
}

int GLTFTranslator::getOutputComponentCount(int attrId)
{
#if !defined(OSMac_)
	switch (attrId)
	{
	case RPRGLTF_ANIMATION_MOVEMENTTYPE_TRANSLATION:
		return COMPONENT_COUNT_TRANSLATION;
	case RPRGLTF_ANIMATION_MOVEMENTTYPE_SCALE:
		return COMPONENT_COUNT_SCALE;
	case RPRGLTF_ANIMATION_MOVEMENTTYPE_ROTATION:
		return COMPONENT_COUNT_ROTATION;
	}
#endif
    
	assert(false);
	return 0;
}

void GLTFTranslator::addTimesFromCurve(const MFnAnimCurve& curve, std::set<MTime>& outUniqueTimeKeySet, bool addAdditionalKeys)
{
	int keyCount = curve.numKeys();
	for (int keyIndex = 0; keyIndex < keyCount; ++keyIndex)
	{
		MTime time = curve.time(keyIndex);

		outUniqueTimeKeySet.insert(time);

		// keys autogeneration for rotation
		if (addAdditionalKeys && keyIndex > 0)
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
				outUniqueTimeKeySet.insert(additionalTimePoint);

				currentValue += step;
			}
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
#if !defined(OSMac_)
	size_t keyCount = gltfDataHolderStruct.m_timePoints.size();

	rprgltf_animation gltfAnimData;
	gltfAnimData.structSize = sizeof(gltfAnimData);

	// RPR GLTF takes char* that's why const cast is needed
	gltfAnimData.groupName = const_cast<char*>(gltfDataHolderStruct.groupName.asChar());
	gltfAnimData.movementType = attrId;
	gltfAnimData.interpolationType = 0;
	gltfAnimData.nbTimeKeys = (unsigned int) keyCount;
	gltfAnimData.nbTransformValues = (unsigned int) keyCount;
	gltfAnimData.timeKeys = gltfDataHolderStruct.m_timePoints.data();
	gltfAnimData.transformValues = gltfDataHolderStruct.m_values.data();

	int res = rprGLTF_AddAnimation(&gltfAnimData);
	if (res != RPR_SUCCESS)
	{
		MGlobal::displayError("rprGLTF_AddAnimation returned error: ");
	}
#endif
}

void GLTFTranslator::applyGLTFAnimationForTransform(const MDagPath& dagPath, GLTFAnimationDataHolderVector& gltfDataHolder)
{
#if !defined(OSMac_)
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
	std::set<MTime> uniqueTimeKeys;

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
				addTimesFromCurve(tempCurve, uniqueTimeKeys, attributeId == RPRGLTF_ANIMATION_MOVEMENTTYPE_ROTATION);
			}
		}
	}

	MPlug matrixPlug = depNodeTransform.findPlug("matrix", &status);

	size_t keyCount = uniqueTimeKeys.size();

	for (int attributeId : attrIds)
	{
		gltfDataHolder.emplace(gltfDataHolder.end());
		GLTFAnimationDataHolderStruct& gltfDataHolderStruct = gltfDataHolder.back();

		for (std::set<MTime>::iterator it = uniqueTimeKeys.begin(); it != uniqueTimeKeys.end(); it++)
		{
			MTime time = *it;
			MDGContext dgContext(time);

			MMatrix newMatrix;
			MObject val;

			matrixPlug.getValue(val, dgContext);
			newMatrix = MFnMatrixData(val).matrix();

			MTransformationMatrix transformMatrix(newMatrix);

			gltfDataHolderStruct.m_timePoints.push_back((float) it->as(MTime::Unit::kSeconds));

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
		}

		gltfDataHolderStruct.groupName = groupName;
		addAnimationToGLTFRPR(gltfDataHolderStruct, attributeId);
	}
#endif
}

void GLTFTranslator::reportGLTFExportError(MString strPath)
{
	MGlobal::displayError("GLTF export error: cannot get animation for transform: " + strPath);
}

