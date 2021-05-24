#pragma once

#include <maya/MFnAnimCurve.h>
#include "RenderProgressBars.h"
#include "Context/FireRenderContext.h"

#include <vector>

// Probably we should move cleanScene call in the destructor of FireRenderContext
// But it needs to be tested
class ContextAutoCleaner
{
public:
	ContextAutoCleaner(FireRenderContext* context) : m_Context(context) {}
	~ContextAutoCleaner()
	{
		if (m_Context != nullptr)
		{
			m_Context->cleanScene();
		}
	}
private:
	FireRenderContext* m_Context;
};

class ExportCancelledException
{

};

class TimeKeyStruct
{
public:
	TimeKeyStruct(MTime time, int attributeId) : m_Time(time)
	{
		m_AttributeId.insert(attributeId);
	}

	bool operator == (const TimeKeyStruct& rhs) const
	{
		return m_Time == rhs.m_Time;
	}

	bool operator < (const TimeKeyStruct& rhs) const
	{
		return m_Time < rhs.m_Time;
	}


	// Make modifier as const because of use of mutable m_AttributeId member. It is needed because we need to modify it in a "std::set" container
	void AddNewAttribute(int attributeId) const
	{
		m_AttributeId.insert(attributeId);
	}

	bool DoesAttributeIdPresent(int attributeId) const
	{
		return m_AttributeId.find(attributeId) != m_AttributeId.end();
	}

	MTime GetTime() const { return m_Time; }
	const std::set<int>& GetAttributeSetRef() const { return m_AttributeId; }

private:
	MTime m_Time;

	// Use set of attribute Ids because  RPRGLTF_ANIMATION_MOVEMENTTYPE_TRANSLATION, 
	// RPRGLTF_ANIMATION_MOVEMENTTYPE_ROTATION, RPRGLTF_ANIMATION_MOVEMENTTYPE_SCALE cannot be combined in a flag mask				
	mutable std::set<int> m_AttributeId;
};

typedef std::set<TimeKeyStruct> TimeKeySet;

class AnimationExporter
{
public:
	AnimationExporter(bool gltfExport); // true - gltf, false - rprs

	bool IsGLTFExport() const { return m_IsGLTFExport; }

	void SetProgressBars(RenderProgressBars* progressBars) { m_progressBars = progressBars; }
	RenderProgressBars* GetProgressBars() { return m_progressBars; }

	void Export(FireRenderContext& context, MDagPathArray* renderableCamera);

private:
	struct AnimationDataHolderStruct
	{
		std::vector<float> m_timePoints;
		std::vector<float> m_values;
		MString groupName;
	};

	typedef std::vector<AnimationDataHolderStruct> AnimationDataHolderVector;
	typedef std::vector<frw::Camera> CameraVector;

	struct DataHolderStruct
	{
		AnimationDataHolderVector animationDataVector;
		CameraVector cameraVector;
		MDagPathArray* inputRenderableCameras = nullptr;
	};

	MString GetGroupNameForDagPath(MDagPath dagPath, int pop = 0);
	void AddAnimations(DataHolderStruct& dataHolder, FireRenderContext& context);
	void AnimateGroups(AnimationDataHolderVector& dataHolder);
	MString GetAttributeNameById(int id);

	void SetTransformationForNode(MObject transform, const char* groupName);

	void AssignCameras(DataHolderStruct& dataHolder, FireRenderContext& context);
	void AssignMeshesAndLights(FireRenderContext& context);

	// addAdditionalKeys param means that we need to add additional keys for Rotation, 
	void AddTimesFromCurve(const MFnAnimCurve& curve, TimeKeySet& outUniqueTimeKeySet, int attributeId);

	void AddOneTimePoint(const MTime time, const MFnAnimCurve& curve, TimeKeySet& outUniqueTimeKeySet, int attributeId, int keyIndex);

	int GetOutputComponentCount(int attrId);
	inline float GetValueForTime(const MPlug& plug, const MFnAnimCurve& curve, const MTime& time);

	void AddAnimationToGLTFRPR(AnimationDataHolderStruct& gltfDataHolderStruct, int attrId);
	void AddAnimationToRPRS(AnimationDataHolderStruct& gltfDataHolderStruct, int attrId);

	void ApplyAnimationForTransform(const MDagPath& dagPath, AnimationDataHolderVector& gltfDataHolder);
	void ReportGLTFExportError(MString strPath);

	bool IsNeedToSetANameForTransform(const MDagPath& dagPath);

	void ReportProgress(int progress);
	void ReportDataChunk(size_t dataChunkIndex, size_t count);

	void assignMesh(FireRenderMesh* pMesh, const MString& groupName, const MDagPath& dagPath);
	void assignLight(FireRenderLight* pLight, const MString& groupName);

private:
	int m_runtimeMoveTypeTranslation;
	int m_runtimeMoveTypeRotation;
	int m_runtimeMoveTypeScale;

	RenderProgressBars* m_progressBars;
	bool m_IsGLTFExport;

	DataHolderStruct m_dataHolder;

	int (*m_pFunc_AddExtraCamera) (rpr_camera extraCam);

	int (*m_pFunc_AssignCameraToGroup) (rpr_camera camera, const rpr_char* groupName);
	int (*m_pFunc_AssignShapeToGroup) (rpr_shape shape, const rpr_char* groupName);
	int (*m_pFunc_AddExtraShapeParameter) (rpr_shape shape, const rpr_char* parameterName, int value);
	int (*m_pFunc_AssignLightToGroup) (rpr_light light, const rpr_char* groupName);

	int (*m_pFunc_SetTransformGroup) (const rpr_char* groupChild, const float* matrixComponents);
	int (*m_pFunc_AssignParentGroupToGroup) (const rpr_char* groupChild, const rpr_char* groupParent);

	void (AnimationExporter::*m_pFunc_AddAnimationTrackToRPR) (AnimationDataHolderStruct&, int);
};