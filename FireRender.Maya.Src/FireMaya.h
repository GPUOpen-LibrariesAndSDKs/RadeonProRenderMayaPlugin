#pragma once

#include "frWrap.h"

#if MAYA_API_VERSION >= 20180000
#include <maya/MApiNamespace.h>
#endif
#include <maya/MTypeId.h>
#include <maya/MPxNode.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MNodeMessage.h>


namespace FireMaya
{
	class Scope;

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

	enum MayaValueId
	{
		// maps
		MayaNodePlace2dTexture = 0x52504c32,
		MayaNodeFile = 0x52544654,
		MayaNodeChecker = 0x52544348,
		MayaNodeRamp = 0x52545241,
		MayaNoise = 0x52544e33,
		MayaBump2d = 0x5242554D,
		MayaNodeRemapHSV = 0x524d4853,
		MayaNodeGammaCorrect = 0x5247414d,

		// arithmetic
		MayaAddDoubleLinear = 0x4441444c,
		MayaBlendColors = 0x52424c32,
		MayaContrast = 0x52434f4e,
		MayaMultDoubleLinear = 0x444d444c,
		MayaMultiplyDivide = 0x524d4449,
		MayaPlusMinusAverage = 0x52504d41,
		MayaVectorProduct = 0x52564543
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

	private:
		MCallbackId openCallback;
		MCallbackId importCallback;
		static void onFileOpen(void* clientData);
	};

	// value classes operate on scalars or colors, usually this will be used as an input to something else
	// maps and utilities should use this class
	class ValueNode : public Node
	{
	public:
		virtual frw::Value GetValue(FireMaya::Scope& scope) = 0;
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
	};

	class Scope
	{
		typedef std::string NodeId;

		struct Data
		{
			frw::Context context;
			frw::MaterialSystem materialSystem;
			frw::Scene scene;

			std::map<NodeId, frw::Shader> volumeShaderMap;
			std::map<NodeId, frw::Shader> shaderMap;
			std::map<NodeId, frw::Value> valueMap;
			std::list<MCallbackId> callbackId;
			std::map<std::string, frw::Image> imageCache;

			Data();
			~Data();
		};
		typedef std::shared_ptr<Data> DataPtr;

		DataPtr m;

		void RegisterCallback(MObject node);

		// callbacks
	public:
		void NodeDirtyCallback(MObject& node);
	private:
		static void NodeDirtyCallback(MObject& node, void* clientData);

		void NodeDirtyPlugCallback(MObject& node, MPlug &plug);
		static void NodeDirtyPlugCallback(MObject& node, MPlug& plug, void* clientData);

		frw::Shader GetCachedShader(const NodeId& str) const;
		void SetCachedShader(const NodeId& str, frw::Shader shader);

		frw::Shader GetCachedVolumeShader( const NodeId& str ) const;
		void SetCachedVolumeShader( const NodeId& str, frw::Shader shader );

		frw::Value GetCachedValue(const NodeId& str) const;
		void SetCachedValue(const NodeId& str, frw::Value shader);

		frw::Shader ParseVolumeShader( MObject ob );
		frw::Shader ParseShader(MObject ob);
		frw::Value createImageFromShaderNode(MObject node, MString plugName = "outColor", int width = 256, int height = 256);

		frw::Value ParseValue(MObject ob, const MString &outPlugName);

		bool FindFileNodeRecursive(MObject objectNode, int& width, int& height);
		frw::Value createImageFromShaderNodeUsingFileNode(MObject node, MString plugName);

		frw::Value CosinePowerToRoughness(const frw::Value &power);

		frw::Shader convertMayaBlinn(const MFnDependencyNode &node);
		frw::Shader convertMayaPhong(const MFnDependencyNode &node);
		frw::Shader convertMayaPhongE(const MFnDependencyNode &node);

		frw::Value convertMayaNoise(const MFnDependencyNode &node);
		frw::Value convertMayaBump2d(const MFnDependencyNode &node);

		// convert arithmetic nodes
		frw::Value convertMayaAddDoubleLinear(const MFnDependencyNode &node);
		frw::Value convertMayaBlendColors(const MFnDependencyNode &node);
		frw::Value convertMayaContrast(const MFnDependencyNode &node);
		frw::Value convertMayaMultDoubleLinear(const MFnDependencyNode &node);
		frw::Value convertMayaMultiplyDivide(const MFnDependencyNode &node);
		frw::Value convertMayaPlusMinusAverage(const MFnDependencyNode &node, const MString &outPlugName);
		frw::Value convertMayaVectorProduct(const MFnDependencyNode &node);

		typedef std::vector<frw::Value> ArrayOfValues;
		void GetArrayOfValues(const MFnDependencyNode &node, const char * plugName, ArrayOfValues &arr);
		frw::Value calcElements(const ArrayOfValues &arr, bool substract = false);

	public:
		Scope();
		~Scope();

		// by object
		frw::Shader GetShader(MObject ob, bool forceUpdate = false);
		frw::Shader GetShader(MPlug ob);
		frw::Shader GetShadowCatcherShader();

		frw::Shader GetVolumeShader( MObject ob, bool forceUpdate = false );
		frw::Shader GetVolumeShader( MPlug ob );

		frw::Image  GetImage(MString path, MString colorSpace, bool flipX = false);
		frw::Image  GetAdjustedImage(MString texturePath,
			int viewWidth, int viewHeight,
			FitType fit, double contrast, double brightness,
			FitType filmFit, double overScan,
			double imgSizeX, double imgSizeY,
			double apertureSizeX, double apertureSizeY,
			double offsetX, double offsetY,
			bool ignoreColorSpaceFileRules, MString colorSpace);

		frw::Value GetValue(MObject ob, const MString &outPlugName = MString());
		frw::Value GetValue(const MPlug& ob);
		frw::Value GetValue(MObject ob, frw::Value orDefault) { return GetValue(ob) | orDefault; }
		frw::Value GetValue(const MPlug& ob, frw::Value orDefault) { return GetValue(ob) | orDefault; }
		frw::Value GetConnectedValue(const MPlug& ob);	// recommended for uv and normals, where constant value is undesired or invalid

		frw::Value GetValueWithCheck(const MFnDependencyNode &node, const char * plugName);

		frw::Context Context() const { return m->context; }
		frw::MaterialSystem MaterialSystem() const { return m->materialSystem; }
		frw::Scene Scene() const { return m->scene; }

		void Reset();
		void Init(rpr_context handle, bool destroyMaterialSystemOnDelete = true);
	};

#ifdef DEBUG
	static void	Dump(const MFnDependencyNode& shader_node);
#endif
    
	MPlug	GetConnectedPlug(const MPlug& plug);
	MObject GetConnectedNode(const MPlug& plug);
	MString GetBasePath();
	MString GetIconPath();
	MString GetShaderPath();
}

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

