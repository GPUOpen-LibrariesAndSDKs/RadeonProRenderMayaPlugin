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

#include "frWrap.h"

#include <maya/MApiNamespace.h>

#include <maya/MTypeId.h>
#include <maya/MPxNode.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MNodeMessage.h>
#include "Context/FireRenderContextIFace.h"

class FireRenderMeshCommon;

namespace FireMaya
{
	class Scope;

	typedef std::string NodeId;

	enum FitType	// chosen to match image layer fit info
	{
		FitFill,
		FitBest,
		FitHorizontal,
		FitVertical,
		FitStretch,
	};

	class TypeId : MTypeId
	{
	public:

		/*
			Registered Maya node ids with ADN
			These IDs should only be used to refer to nodes created by Maya FireRender plugin

			Your Node ID Block is: 0x00126c40 - 0x00126cbf

			(Further ID blocks can be registered here: http://mayaid.autodesk.io/)
		*/
		enum 	// do not change or re-use these IDs!
		{
			FireRenderNodeIdBeginReserved = 0x00126c40,
			FireRenderNodeIdEndReserved = 0x00126cc0, // not inclusive

			FireRenderAddMaterial = 0x00126c40,
			FireRenderBlendMaterial,
			FireRenderFresnel,
			FireRenderGlobals,
			FireRenderIBL,
			FireRenderMaterial,
			FireRenderSkyLocator,
			FireRenderStandardMaterial,
			FireRenderViewportGlobals,
			FireRenderChecker,
			FireRenderArithmetic,
			FireRenderBlendValue,
			FireRenderGradient,
			FireRenderLookup,
			FireRenderDot,
			FireRenderTexture,
			FireRenderFresnelSchlick,
			FireRenderNoise,
			FireRenderPassthrough,
			FireRenderBump,
			FireRenderNormal,
			FireRenderRenderPass,
			FireRenderUberMaterial,
			FireRenderVolumeMaterial,
			FireRenderTransparentMaterial,
			FireRenderGround,
			FireRenderSubsurfaceMaterial,
			FireRenderIESLightLocator,
			FireRenderDisplacement,
			FireRenderEnvironmentLight,
			FireRenderShadowCatcherMaterial,
			FireRenderPBRMaterial,
			FireRenderPhysicalLightLocator,
			FireRenderAONode,
			FireRenderVolumeLocator,
			FireRenderToonMaterial,
			FireRenderVoronoi,
			FireRenderRamp,
			FireRenderDoubleSided,
			FireRenderBevel,
			FireRenderMaterialXMaterial,

			// ^ always add new ids to end of list (max 128 entries here)
			FireRenderNodeIdEndCurrent, // <- this value is allowed to change, it marks the end of current list
		};

		static bool IsValidId(int id)
		{
			return id >= FireRenderAddMaterial && id < FireRenderNodeIdEndCurrent;
		}
		static bool IsReservedId(int id)
		{
			return id >= FireRenderNodeIdBeginReserved && id < FireRenderNodeIdEndReserved;
		}

	};

	enum MayaSurfaceId
	{
		MayaNodeLambert = 0x524c414d,
		MayaNodeBlinn = 0x52424c4e,
		MayaNodePhong = 0x5250484f,
		MayaNodePhongE = 0x52504845,
		MayaNodeAnisotropic = 0x52414E49,
		MayaLayeredShader = 0x4c595253
	};

	// the base class for all nodes representing FR material nodes
	class Node : public MPxNode
	{
	public:
		virtual void postConstructor() override;
		virtual void OnFileLoaded();

		void ConvertFilenameToFileInput(MObject filenameAttr, MObject inputAttr);

		// basic implementation of this methods just gets all input attributes and mark as cleaned output
		virtual MStatus compute(const MPlug& plug, MDataBlock& block) override;

		// We need to get all attributes which affect outputs in order to recalculate all dependent nodes
		// It needs to get IPR properly updating while changing attributes on the "left" nodes in dependency graph
		void ForceEvaluateAllAttributes(bool evaluateInputs);
		static void ForceEvaluateAllAttributes(MObject node, bool evaluateInputs);
		static bool IsOutputAttribute(MObject attrObj, bool parentsOnly = false);

		virtual ~Node() override;

	protected:
		void GetOutputAttributes(std::vector<MObject>& outputVec);

	private:
		void FillOutputAttributeNames();

	private:
		MCallbackId openCallback = 0;
		MCallbackId importCallback = 0;
		static void onFileOpen(void* clientData);

		std::vector<MString> m_outputAttributeNames;
	};

	// value classes operate on scalars or colors, usually this will be used as an input to something else
	// maps and utilities should use this class
	class ValueNode : public Node
	{
	public:
		virtual frw::Value GetValue(const FireMaya::Scope& scope) const = 0;

		virtual void postConstructor()
		{
			Node::postConstructor();

			MStatus status = setExistWithoutInConnections(true);
			CHECK_MSTATUS(status);
		}
	};

	// shader classes are the physical materials themselves, eg lambert, emissive, refractive etc
	class ShaderNode : public Node
	{
	public:
		virtual frw::Shader GetShader(FireMaya::Scope& scope) = 0;

		virtual frw::Shader GetVolumeShader( FireMaya::Scope& scope )	// most materials do not have volume shaders
		{
			return nullptr;
		};

		virtual MObject GetDisplacementNode()
		{
			return MObject::kNullObj;
		}

		virtual void postConstructor()
		{
			Node::postConstructor();

			MStatus status = setExistWithoutInConnections(true);
			CHECK_MSTATUS(status);
		}
	};

	class Scope
	{
		struct Data
		{
			frw::Context context;
			frw::MaterialSystem materialSystem;
			frw::Scene scene;

			std::map<NodeId, frw::Shader> volumeShaderMap;
			std::map<NodeId, frw::Shader> shaderMap;
			std::multimap<NodeId, NodeId> lightShaderMap; // shaderId = lightShaderMap[lightNodeId]
			std::map<NodeId, frw::Value> valueMap;
			std::map<NodeId, MCallbackId> m_nodeDirtyCallbacks;
			std::map<NodeId, MCallbackId> m_AttributeChangedCallbacks;
			std::map<std::string, frw::Image> imageCache;

			FireRenderMeshCommon const* m_pCurrentlyParsedMesh; // is not supposed to keep any data outside of during mesh parsing 
			MObject m_pLastLinkedLight; // is not supposed to keep any data outside of during mesh parsing 

			Data();
			~Data();
		};
		typedef std::shared_ptr<Data> DataPtr;

		DataPtr m;

		void RegisterCallback(MObject node, std::string* pOverridenUUID = nullptr);

		IFireRenderContextInfo* m_pContextInfo; // Scope can not exist without a context, thus using raw pointer here is safe

		/** Temporary fix for default diffuse color calculation */
		mutable bool m_IsLastPassTextureMissing = false; 

		// callbacks
	public:
		void NodeDirtyCallback(MObject& node);

		FireRenderMeshCommon const* GetCurrentlyParsedMesh() const { return m->m_pCurrentlyParsedMesh; }
		void SetLastLinkedLight(const MObject& lightObj) const { m->m_pLastLinkedLight = lightObj; }
		void SetIsLastPassTextureMissing(bool value) const { m_IsLastPassTextureMissing = value; }

		frw::Value createImageFromShaderNode(MObject node, MString plugName = "outColor", int width = 256, int height = 256) const;
		frw::Value createImageFromShaderNodeUsingFileNode(MObject node, MString plugName) const;

	private:
		static void NodeDirtyCallback(MObject& node, void* clientData);

		void NodeDirtyPlugCallback(MObject& node, MPlug &plug);
		static void NodeDirtyPlugCallback(MObject& node, MPlug& plug, void* clientData);

		frw::Shader GetCachedVolumeShader( const NodeId& str ) const;
		void SetCachedVolumeShader( const NodeId& str, frw::Shader shader );

		frw::Value GetCachedValue(const NodeId& str) const;
		void SetCachedValue(const NodeId& str, frw::Value shader);

		frw::Image GetCachedImage(const MString& key) const;
		void SetCachedImage(const MString& key, frw::Image img) const;

		frw::Shader ParseVolumeShader( MObject ob );
		frw::Shader ParseShader(MObject ob);

		frw::Value ParseValue(MObject ob, const MString &outPlugName) const;

		bool FindFileNodeRecursive(MObject objectNode, int& width, int& height) const;

		frw::Value CosinePowerToRoughness(const frw::Value &power);

		frw::Shader convertMayaBlinn(const MFnDependencyNode &node);
		frw::Shader convertMayaPhong(const MFnDependencyNode &node);
		frw::Shader convertMayaPhongE(const MFnDependencyNode &node);

		frw::Image CreateImageInternal(MString colorSpace,
			unsigned int width,
			unsigned int height,
			void* srcData,
			unsigned int channels,
			unsigned int componentSize,
			unsigned int rowPitch,
			bool flipY = false) const;

		frw::Image LoadImageUsingMTexture(MString texturePath, MString colorSpace, const MString& ownerNodeName) const;

	public:
		Scope();
		~Scope();

		// by object
		frw::Shader GetShader(MObject ob, const FireRenderMeshCommon* pMesh = nullptr, bool forceUpdate = false);
		frw::Shader GetShader(MPlug ob);
		frw::Shader GetShadowCatcherShader();
		frw::Shader GetReflectionCatcherShader();

		frw::Shader GetVolumeShader( MObject ob, bool forceUpdate = false );
		frw::Shader GetVolumeShader( MPlug ob );

		frw::Image GetImage(MString path, MString colorSpace, const MString& ownerNodeName) const;

		frw::Image GetTiledImage(MString texturePath, 
			int viewWidth, int viewHeight,
			int maxTileWidth, int maxTileHeight,
			int currTileWidth, int currTileHeight,
			int countXTiles, int countYTiles,
			int xTileIdx, int yTileIdx,
			MString colorSpace,
			FitType imgFit = FitStretch /*FitVertical*/);

		frw::Image GetAdjustedImage(MString texturePath,
			int viewWidth, int viewHeight,
			FitType fit, double contrast, double brightness,
			FitType filmFit, double overScan,
			double imgSizeX, double imgSizeY,
			double apertureSizeX, double apertureSizeY,
			double offsetX, double offsetY,
			bool ignoreColorSpaceFileRules, MString colorSpace);


		frw::Value GetValue(MObject ob, const MString &outPlugName = MString()) const;
		frw::Value GetValue(const MPlug& ob) const;
		frw::Value GetValue(MObject ob, frw::Value orDefault) { return GetValue(ob) | orDefault; }
		frw::Value GetValue(const MPlug& ob, frw::Value orDefault) { return GetValue(ob) | orDefault; }

		// Returns 1x1 purple image node in case if something is connected to the plug but there is no valid map as output
		frw::Value GetValueForDiffuseColor(const MPlug& ob);

		frw::Value GetConnectedValue(const MPlug& ob) const;	// recommended for uv and normals, where constant value is undesired or invalid

		frw::Value GetValueWithCheck(const MFnDependencyNode &node, const char * plugName) const;

		frw::Context Context() const { return m->context; }
		frw::MaterialSystem MaterialSystem() const { return m->materialSystem; }
		frw::Scene Scene() const { return m->scene; }

		frw::Shader GetCachedShader(const NodeId& str) const;
		void SetCachedShader(const NodeId& str, frw::Shader shader);

		typedef std::multimap<NodeId, NodeId>::iterator MMapNodeIdIterator;
		std::pair<MMapNodeIdIterator, MMapNodeIdIterator> GetCachedShaderIds(const NodeId& lightId);
		void SetCachedShaderId(const NodeId& lightId, NodeId& shaderId);// shaderId = lightShaderMap[lightNodeId]
		void ClearCachedShaderIds(const NodeId& lightId);

		void Reset();
		void Init(rpr_context handle, bool destroyMaterialSystemOnDelete = true, bool createScene = true);
		void CreateScene(void);
		bool IsValid(void) const { return m != nullptr; }

		void SetContextInfo (IFireRenderContextInfo* pCtxInfo);
		const IFireRenderContextInfo* GetIContextInfo() const;
		IFireRenderContextInfo* GetIContextInfo();
	};

#ifdef DEBUG
	static void	Dump(const MFnDependencyNode& shader_node);
#endif
    
	MPlug	GetConnectedPlug(const MPlug& plug);
	MObject GetConnectedNode(const MPlug& plug);
	MString GetBasePath();
	MString GetIconPath();
	MString GetShaderPath();

	const MString MAYA_FILE_NODE_OUTPUT_COLOR = "oc";
	const MString MAYA_FILE_NODE_OUTPUT_COLOR_COMPONENT_RED = "ocr";
	const MString MAYA_FILE_NODE_OUTPUT_COLOR_COMPONENT_GREEN = "ocg";
	const MString MAYA_FILE_NODE_OUTPUT_COLOR_COMPONENT_BLUE = "ocb";
	const MString MAYA_FILE_NODE_OUTPUT_ALPHA = "oa";
	const MString MAYA_FILE_NODE_OUTPUT_TRANSPARENCY = "ot";
	const MString MAYA_FILE_NODE_OUTPUT_TRANSPARENCY_R = "otr";
	const MString MAYA_FILE_NODE_OUTPUT_TRANSPARENCY_G = "otg";
	const MString MAYA_FILE_NODE_OUTPUT_TRANSPARENCY_B = "otb";
}
// End of namespace FireMaya

template <class T>
void MAKE_INPUT(T& attr)
{
	CHECK_MSTATUS(attr.setKeyable(true));
	CHECK_MSTATUS(attr.setStorable(true));
	CHECK_MSTATUS(attr.setReadable(false));
	CHECK_MSTATUS(attr.setWritable(true));
}

template <class T>
void MAKE_INPUT_CONST(T& attr)
{
	CHECK_MSTATUS(attr.setKeyable(false));
	CHECK_MSTATUS(attr.setStorable(true));
	CHECK_MSTATUS(attr.setReadable(false));
	CHECK_MSTATUS(attr.setWritable(true));
	CHECK_MSTATUS(attr.setConnectable(false));
}

template <class T>
void MAKE_OUTPUT(T& attr)
{
	CHECK_MSTATUS(attr.setKeyable(false));
	CHECK_MSTATUS(attr.setStorable(false));
	CHECK_MSTATUS(attr.setReadable(true));
	CHECK_MSTATUS(attr.setWritable(false));

}

#define FRMAYA_GL_INTERNAL_TEXTURE_FORMAT GL_RGBA16F
#define FRMAYA_GL_MAX_TEXTURE_SIZE 1024
#define FRMAYA_GL_TEXTURE_SIZE_MASK ~3

void CheckGlError();
#define GL_CHECK CheckGlError();

