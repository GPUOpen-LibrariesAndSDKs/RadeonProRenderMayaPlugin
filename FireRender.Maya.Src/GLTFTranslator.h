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
#pragma once

#include "Context/FireRenderContext.h"
#include <maya/MPxFileTranslator.h>

#include <maya/MFnAnimCurve.h>

#include "RenderProgressBars.h"

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
		void assignMeshesAndLights(FireRenderContext& context);
		
		// addAdditionalKeys param means that we need to add additional keys for Rotation, 
		void addTimesFromCurve(const MFnAnimCurve& curve, TimeKeySet& outUniqueTimeKeySet, int attributeId);

		void addOneTimePoint(const MTime time, const MFnAnimCurve& curve, TimeKeySet& outUniqueTimeKeySet, int attributeId, int keyIndex);

		int getOutputComponentCount(int attrId);
		inline float getValueForTime(const MPlug& plug, const MFnAnimCurve& curve, const MTime& time);
		void addAnimationToGLTFRPR(GLTFAnimationDataHolderStruct& gltfDataHolderStruct, int attrId);
		void applyGLTFAnimationForTransform(const MDagPath& dagPath, GLTFAnimationDataHolderVector& gltfDataHolder);
		void reportGLTFExportError(MString strPath);

		bool isNeedToSetANameForTransform(const MDagPath& dagPath);

		void ReportProgress(int progress);
		void ReportDataChunk(size_t dataChunkIndex, size_t count);

	private:
		RenderProgressBars* m_progressBars;
	};
}
