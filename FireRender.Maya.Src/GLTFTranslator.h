#pragma once

#include "Context/FireRenderContext.h"
#include <maya/MPxFileTranslator.h>

#include <maya/MFnAnimCurve.h>

namespace FireMaya
{
	// Probably we should move cleanScene call in the destructor of FireRenderContext
	// But it needs to be tested
	class ContextAutoCleaner
	{
	public:
		ContextAutoCleaner(FireRenderContext* context) : m_Context(context){}
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
	
	class GLTFTranslator : public MPxFileTranslator
	{
	public:
		GLTFTranslator();
		~GLTFTranslator();

		static void* creator();

		virtual MStatus	writer(const MFileObject& file,
			const MString& optionsString,
			FileAccessMode mode);
		//We only support export for GLTF
		virtual bool haveWriteMethod() const { return true; }

		virtual MString defaultExtension() const { return "gltf"; }
		virtual MString filter() const { return "*.gltf"; }

		//We can't open/import GLTF files
		virtual bool canBeOpened() const { return false; }

	private:
		struct GLTFAnimationDataHolderStruct
		{
			std::vector<float> m_timePoints;
			std::vector<float> m_values;
			MString groupName;
		};

		typedef std::vector<GLTFAnimationDataHolderStruct> GLTFAnimationDataHolderVector;
		typedef std::vector<frw::Camera> CameraVector;

		struct GLTFDataHolderStruct
		{
			GLTFAnimationDataHolderVector animationDataVector;
			CameraVector cameraVector;
			MDagPathArray* inputRenderableCameras = nullptr;
		};

		MString getGroupNameForDagPath(MDagPath dagPath, int pop = 0);
		void addGLTFAnimations(GLTFDataHolderStruct& dataHolder, FireRenderContext& context);
		void animateGLTFGroups(GLTFAnimationDataHolderVector& dataHolder);
		MString getGLTFAttributeNameById(int id);

		void setGLTFTransformationForNode(MObject transform, const char* groupName);

		void assignCameras(GLTFDataHolderStruct& dataHolder, FireRenderContext& context);
		void assignMeshes(FireRenderContext& context);
		
		// addAdditionalKeys param means that we need to add additional keys for Rotation, 
		void addTimesFromCurve(const MFnAnimCurve& curve, std::set<MTime>& outUniqueTimeKeySet, bool addAdditionalKeys = false);

		int getOutputComponentCount(int attrId);
		inline float getValueForTime(const MPlug& plug, const MFnAnimCurve& curve, const MTime& time);
		void addAnimationToGLTFRPR(GLTFAnimationDataHolderStruct& gltfDataHolderStruct, int attrId);
		void applyGLTFAnimationForTransform(const MDagPath& dagPath, GLTFAnimationDataHolderVector& gltfDataHolder);
		void reportGLTFExportError(MString strPath);

		bool isNeedToSetANameForTransform(const MDagPath& dagPath);
	};
}
