#pragma once

// Switched from inc_dynamic to inc version of RadeonProRender.h so 
// RprSupport and RadeonProRender_GL needs to be included manually
#include <RadeonProRender.h>
#include <RadeonProRender_GL.h>
#include <RprSupport.h>

#include <list>
#include <map>
#include <vector>
#include <cassert>
#ifndef OSMac_
#include <memory>
#endif
#include <set>
#include <string>
#include <maya/MString.h>

#include <math.h>

#include "Logger.h"
#include "FireRenderError.h"

#include <maya/MGlobal.h>

//#define FRW_LOGGING 1

#if FRW_LOGGING

#define FRW_PRINT_DEBUG DebugPrint

#else

#define FRW_PRINT_DEBUG(...)

#endif

#ifdef MAX_PLUGIN
#define FRW_USE_MAX_TYPES	// allow use of max types in helpers
#endif

#undef min
#undef max

/*

	Helper layer for working with fire render types

	Smart pointers track usage and by default destroy objects when pointers are destroyed

	The smart pointers are protected to avoid unwanted dereferencing. This does complicate the code
	here (with the requirement for internal Data/Extra structs) but means that client code can use
	these objects as completely encapsulated reference types.

*/

#include <algorithm>

namespace frw
{
	class ImageNode;
	class Image;
	// forward declarations

	class Node;
	class Value;
	class Context;
	class MaterialSystem;
	class Shader;
	class FrameBuffer;
	class PostEffect;

	enum Operator
	{
		OperatorAdd = RPR_MATERIAL_NODE_OP_ADD,
		OperatorSubtract = RPR_MATERIAL_NODE_OP_SUB,
		OperatorMultiply = RPR_MATERIAL_NODE_OP_MUL,
		OperatorDivide = RPR_MATERIAL_NODE_OP_DIV,
		OperatorSin = RPR_MATERIAL_NODE_OP_SIN,
		OperatorCos = RPR_MATERIAL_NODE_OP_COS,
		OperatorTan = RPR_MATERIAL_NODE_OP_TAN,
		OperatorSelectX = RPR_MATERIAL_NODE_OP_SELECT_X,
		OperatorSelectY = RPR_MATERIAL_NODE_OP_SELECT_Y,
		OperatorSelectZ = RPR_MATERIAL_NODE_OP_SELECT_Z,
		OperatorCombine = RPR_MATERIAL_NODE_OP_COMBINE,
		OperatorDot = RPR_MATERIAL_NODE_OP_DOT3,
		OperatorCross = RPR_MATERIAL_NODE_OP_CROSS3,
		OperatorLength = RPR_MATERIAL_NODE_OP_LENGTH3,
		OperatorNormalize = RPR_MATERIAL_NODE_OP_NORMALIZE3,
		OperatorPow = RPR_MATERIAL_NODE_OP_POW,
		OperatorArcCos = RPR_MATERIAL_NODE_OP_ACOS,
		OperatorArcSin = RPR_MATERIAL_NODE_OP_ASIN,
		OperatorArcTan = RPR_MATERIAL_NODE_OP_ATAN,	// binary?
		OperatorComponentAverage = RPR_MATERIAL_NODE_OP_AVERAGE_XYZ, //?
		OperatorAverage = RPR_MATERIAL_NODE_OP_AVERAGE, //?
		OperatorMin = RPR_MATERIAL_NODE_OP_MIN,
		OperatorMax = RPR_MATERIAL_NODE_OP_MAX,
		OperatorFloor = RPR_MATERIAL_NODE_OP_FLOOR,
		OperatorMod = RPR_MATERIAL_NODE_OP_MOD,
		OperatorAbs = RPR_MATERIAL_NODE_OP_ABS,
	};

	enum LookupType
	{
		LookupTypeUV0 = RPR_MATERIAL_NODE_LOOKUP_UV,
		LookupTypeNormal = RPR_MATERIAL_NODE_LOOKUP_N,
		LookupTypePosition = RPR_MATERIAL_NODE_LOOKUP_P,
		LookupTypeIncident = RPR_MATERIAL_NODE_LOOKUP_INVEC,
		LookupTypeOutVector = RPR_MATERIAL_NODE_LOOKUP_OUTVEC,
		LookupTypeUV1 = RPR_MATERIAL_NODE_LOOKUP_UV1,
	};

	enum ValueType
	{
		ValueTypeArithmetic = RPR_MATERIAL_NODE_ARITHMETIC,
		ValueTypeFresnel = RPR_MATERIAL_NODE_FRESNEL,
		ValueTypeNormalMap = RPR_MATERIAL_NODE_NORMAL_MAP,
		ValueTypeBumpMap = RPR_MATERIAL_NODE_BUMP_MAP,
		ValueTypeImageMap = RPR_MATERIAL_NODE_IMAGE_TEXTURE,
		ValueTypeNoiseMap = RPR_MATERIAL_NODE_NOISE2D_TEXTURE,
		ValueTypeGradientMap = RPR_MATERIAL_NODE_GRADIENT_TEXTURE,
		ValueTypeCheckerMap = RPR_MATERIAL_NODE_CHECKER_TEXTURE, //did not get color input
		ValueTypeDotMap = RPR_MATERIAL_NODE_DOT_TEXTURE, //did not get color input
		ValueTypeConstant = RPR_MATERIAL_NODE_CONSTANT_TEXTURE,
		ValueTypeLookup = RPR_MATERIAL_NODE_INPUT_LOOKUP,
		ValueTypeBlend = RPR_MATERIAL_NODE_BLEND_VALUE,
		ValueTypeFresnelSchlick = RPR_MATERIAL_NODE_FRESNEL_SCHLICK,
		ValueTypePassthrough = RPR_MATERIAL_NODE_PASSTHROUGH,
        ValueTypeAOMap = RPR_MATERIAL_NODE_AO_MAP,
	};

	enum ShaderType
	{
		ShaderTypeInvalid = 0xffffffff,
		ShaderTypeRprx    = 0xfffffffe,

		ShaderTypeDiffuse = RPR_MATERIAL_NODE_DIFFUSE,
		ShaderTypeMicrofacet = RPR_MATERIAL_NODE_MICROFACET,
		ShaderTypeReflection = RPR_MATERIAL_NODE_REFLECTION,
		ShaderTypeRefraction = RPR_MATERIAL_NODE_REFRACTION,
		ShaderTypeMicrofacetRefraction = RPR_MATERIAL_NODE_MICROFACET_REFRACTION,
		ShaderTypeTransparent = RPR_MATERIAL_NODE_TRANSPARENT,
		ShaderTypeEmissive = RPR_MATERIAL_NODE_EMISSIVE,
		ShaderTypeWard = RPR_MATERIAL_NODE_WARD,
		ShaderTypeBlend = RPR_MATERIAL_NODE_BLEND,
		ShaderTypeStandard = RPR_MATERIAL_NODE_STANDARD,
		ShaderTypeOrenNayer = RPR_MATERIAL_NODE_ORENNAYAR,
		ShaderTypeDiffuseRefraction = RPR_MATERIAL_NODE_DIFFUSE_REFRACTION,
		ShaderTypeAdd = RPR_MATERIAL_NODE_ADD,
		ShaderTypeVolume = RPR_MATERIAL_NODE_VOLUME,
	};

	enum ContextParameterType
	{
		ParameterTypeFloat = RPR_PARAMETER_TYPE_FLOAT,
		ParameterTypeFloat2 = RPR_PARAMETER_TYPE_FLOAT2,
		ParameterTypeFloat3 = RPR_PARAMETER_TYPE_FLOAT3,
		ParameterTypeFloat4 = RPR_PARAMETER_TYPE_FLOAT4,
		ParameterTypeImage = RPR_PARAMETER_TYPE_IMAGE,
		ParameterTypeString = RPR_PARAMETER_TYPE_STRING,
		ParameterTypeShader = RPR_PARAMETER_TYPE_SHADER,
		ParameterTypeUint = RPR_PARAMETER_TYPE_UINT,
	};

	enum ContextParameterId
	{
		ContextParameterCreationFlags = RPR_CONTEXT_CREATION_FLAGS,
		ContextParameterCachePath = RPR_CONTEXT_CACHE_PATH,
		ContextParameterRenderStatus = RPR_CONTEXT_RENDER_STATUS,
		ContextParameterRenderStatistics = RPR_CONTEXT_RENDER_STATISTICS,
		ContextParameterDeviceCount = RPR_CONTEXT_DEVICE_COUNT,
		ContextParameterParameterCount = RPR_CONTEXT_PARAMETER_COUNT,
		ContextParameterActivePlugin = RPR_CONTEXT_ACTIVE_PLUGIN,
		ContextParameterScene = RPR_CONTEXT_SCENE,
#if (RPR_API_VERSION < 0x010031000)
		ContextParameterAACellSize = RPR_CONTEXT_AA_CELL_SIZE,
		ContextParameterAASamples = RPR_CONTEXT_AA_SAMPLES,
#endif
		ContextParameterFilterType = RPR_CONTEXT_IMAGE_FILTER_TYPE,
		ContextParameterFilterBoxRadius = RPR_CONTEXT_IMAGE_FILTER_BOX_RADIUS,
		ContextParameterFilterGaussianRadius = RPR_CONTEXT_IMAGE_FILTER_GAUSSIAN_RADIUS,
		ContextParameterFilterTriangleRadius = RPR_CONTEXT_IMAGE_FILTER_TRIANGLE_RADIUS,
		ContextParameterFilterMitchellRadius = RPR_CONTEXT_IMAGE_FILTER_MITCHELL_RADIUS,
		ContextParameterFilterLanczosRadius = RPR_CONTEXT_IMAGE_FILTER_LANCZOS_RADIUS,
		ContextParameterFilterBlackmanHarrisRadius = RPR_CONTEXT_IMAGE_FILTER_BLACKMANHARRIS_RADIUS,
		ContextParameterToneMappingType = RPR_CONTEXT_TONE_MAPPING_TYPE,
		ContextParameterToneMappingLinearScale = RPR_CONTEXT_TONE_MAPPING_LINEAR_SCALE,
		ContextParameterToneMappingPhotolinearSensitivity = RPR_CONTEXT_TONE_MAPPING_PHOTO_LINEAR_SENSITIVITY,
		ContextParameterToneMappingPhotolinearExposure = RPR_CONTEXT_TONE_MAPPING_PHOTO_LINEAR_EXPOSURE,
		ContextParameterToneMappingPhotolinearFStop = RPR_CONTEXT_TONE_MAPPING_PHOTO_LINEAR_FSTOP,
		ContextParameterToneMappingReinhard2Prescale = RPR_CONTEXT_TONE_MAPPING_REINHARD02_PRE_SCALE,
		ContextParameterToneMappingReinhard2Postscale = RPR_CONTEXT_TONE_MAPPING_REINHARD02_POST_SCALE,
		ContextParameterToneMappingReinhard2Burn = RPR_CONTEXT_TONE_MAPPING_REINHARD02_BURN,
		ContextParameterMaxRecursion = RPR_CONTEXT_MAX_RECURSION,
		ContextParameterRayCastEpsilon = RPR_CONTEXT_RAY_CAST_EPISLON,
		ContextParameterRadienceClamp = RPR_CONTEXT_RADIANCE_CLAMP,
		ContextParameterXFlip = RPR_CONTEXT_X_FLIP,
		ContextParameterYFlip = RPR_CONTEXT_Y_FLIP,
		ContextParameterTextureGamma = RPR_CONTEXT_TEXTURE_GAMMA,
		ContextParameterPdfThreshold = RPR_CONTEXT_PDF_THRESHOLD,
		ContextParameterRenderMode = RPR_CONTEXT_RENDER_MODE,
		ContextParameterRoughnessCap = RPR_CONTEXT_ROUGHNESS_CAP,
		ContextParameterDisplayGamma = RPR_CONTEXT_DISPLAY_GAMMA,
		ContextParameterMaterialStackSize = RPR_CONTEXT_MATERIAL_STACK_SIZE,
		ContextParameterClippingPlane = RPR_CONTEXT_CLIPPING_PLANE,
		ContextParameterGPU0Name = RPR_CONTEXT_GPU0_NAME,
		ContextParameterGPU1Name = RPR_CONTEXT_GPU1_NAME,
		ContextParameterGPU2Name = RPR_CONTEXT_GPU2_NAME,
		ContextParameterGPU3Name = RPR_CONTEXT_GPU3_NAME,
		ContextParameterGPU4Name = RPR_CONTEXT_GPU4_NAME,
		ContextParameterGPU5Name = RPR_CONTEXT_GPU5_NAME,
		ContextParameterGPU6Name = RPR_CONTEXT_GPU6_NAME,
		ContextParameterGPU7Name = RPR_CONTEXT_GPU7_NAME,
		ContextParameterCPUName = RPR_CONTEXT_CPU_NAME,
	};

	enum ContextParameterInfo
	{
		ParameterInfoId = RPR_PARAMETER_NAME,
		ParameterInfoName = RPR_PARAMETER_NAME_STRING,
		ParameterInfoType = RPR_PARAMETER_TYPE,
		ParameterInfoDescription = RPR_PARAMETER_DESCRIPTION,
		ParameterInfoValue = RPR_PARAMETER_VALUE,
	};

	enum NodeInfo
	{
		NodeInfoType = RPR_MATERIAL_NODE_TYPE,
		NodeInfoInputCount = RPR_MATERIAL_NODE_INPUT_COUNT,
		NodeInfoSystem = RPR_MATERIAL_NODE_SYSTEM,
	};

	enum NodeInputType
	{
		NodeInputTypeFloat4 = RPR_MATERIAL_NODE_INPUT_TYPE_FLOAT4,
		NodeInputTypeUint = RPR_MATERIAL_NODE_INPUT_TYPE_UINT,
		NodeInputTypeNode = RPR_MATERIAL_NODE_INPUT_TYPE_NODE,
		NodeInputTypeImage = RPR_MATERIAL_NODE_INPUT_TYPE_IMAGE,
	};

	enum NodeInputId
	{
		NodeInputColor = RPR_MATERIAL_INPUT_COLOR,
		NodeInputColor0 = RPR_MATERIAL_INPUT_COLOR0,
		NodeInputColor1 = RPR_MATERIAL_INPUT_COLOR1,
		NodeInputNormal = RPR_MATERIAL_INPUT_NORMAL,
		NodeInputUV = RPR_MATERIAL_INPUT_UV,
		NodeInputData = RPR_MATERIAL_INPUT_DATA,
		NodeInputRoughness = RPR_MATERIAL_INPUT_ROUGHNESS,
		NodeInputIOR = RPR_MATERIAL_INPUT_IOR,
		NodeInputRoughnessX = RPR_MATERIAL_INPUT_ROUGHNESS_X,
		NodeInputRoughnessY = RPR_MATERIAL_INPUT_ROUGHNESS_Y,
		NodeInputRotation = RPR_MATERIAL_INPUT_ROTATION,
		NodeInputWeight = RPR_MATERIAL_INPUT_WEIGHT,
		NodeInputOperator = RPR_MATERIAL_INPUT_OP,
		NodeInputIncidentVector = RPR_MATERIAL_INPUT_INVEC,
		NodeInputUVScale = RPR_MATERIAL_INPUT_UV_SCALE,
		NodeInputValue = RPR_MATERIAL_INPUT_VALUE,
		NodeInputReflectance = RPR_MATERIAL_INPUT_REFLECTANCE,
		NodeInputScale = RPR_MATERIAL_INPUT_SCALE,
		NodeInputScattering = RPR_MATERIAL_INPUT_SCATTERING,
		NodeInputAbsorbtion = RPR_MATERIAL_INPUT_ABSORBTION,
		NodeInputEmission = RPR_MATERIAL_INPUT_EMISSION,
		NodeInputG = RPR_MATERIAL_INPUT_G,
		NodeInputMultiScatter = RPR_MATERIAL_INPUT_MULTISCATTER,

		NodeInputStandardDiffuseColor = RPR_MATERIAL_STANDARD_INPUT_DIFFUSE_COLOR,
		NodeInputStandardDiffuseNormal = RPR_MATERIAL_STANDARD_INPUT_DIFFUSE_NORMAL,
		NodeInputStandardGlossyColor = RPR_MATERIAL_STANDARD_INPUT_GLOSSY_COLOR,
		NodeInputStandardGlossyNormal = RPR_MATERIAL_STANDARD_INPUT_GLOSSY_NORMAL,
		NodeInputStandardClearcoatColor = RPR_MATERIAL_STANDARD_INPUT_CLEARCOAT_COLOR,
		NodeInputStandardClearcoatNormal = RPR_MATERIAL_STANDARD_INPUT_CLEARCOAT_NORMAL,
		NodeInputStandardRefractionColor = RPR_MATERIAL_STANDARD_INPUT_REFRACTION_COLOR,
		NodeInputStandardRefractionNormal = RPR_MATERIAL_STANDARD_INPUT_REFRACTION_NORMAL,
		NodeInputStandardRefractionIOR = RPR_MATERIAL_STANDARD_INPUT_REFRACTION_IOR,
		NodeInputStandardDiffuseToRefractionWeight = RPR_MATERIAL_STANDARD_INPUT_DIFFUSE_TO_REFRACTION_WEIGHT,
		NodeInputStandardGlossyToDiffuseWeight = RPR_MATERIAL_STANDARD_INPUT_GLOSSY_TO_DIFFUSE_WEIGHT,
		NodeInputStandardClearcoatToGlossyWeight = RPR_MATERIAL_STANDARD_INPUT_CLEARCOAT_TO_GLOSSY_WEIGHT,
		NodeInputStandardTransparency = RPR_MATERIAL_STANDARD_INPUT_TRANSPARENCY,
		NodeInputStandardTransparencyColor = RPR_MATERIAL_STANDARD_INPUT_TRANSPARENCY_COLOR,
		NodeInputStandardRefractionRoughness = RPR_MATERIAL_STANDARD_INPUT_REFRACTION_ROUGHNESS,
	};
	enum NodeInputInfo
	{
		NodeInputInfoId = RPR_MATERIAL_NODE_INPUT_NAME,
		NodeInputInfoName = RPR_MATERIAL_NODE_INPUT_NAME_STRING,
		NodeInputInfoDescription = RPR_MATERIAL_NODE_INPUT_DESCRIPTION,
		NodeInputInfoValue = RPR_MATERIAL_NODE_INPUT_VALUE,
		NodeInputInfoType = RPR_MATERIAL_NODE_INPUT_TYPE,
	};

	enum ToneMappingType
	{
		ToneMappingTypeNone = RPR_TONEMAPPING_OPERATOR_NONE,
		ToneMappingTypeLinear = RPR_TONEMAPPING_OPERATOR_LINEAR,
		ToneMappingTypePhotolinear = RPR_TONEMAPPING_OPERATOR_PHOTOLINEAR,
		ToneMappingTypeAutolinear = RPR_TONEMAPPING_OPERATOR_AUTOLINEAR,
		ToneMappingTypeMaxwhite = RPR_TONEMAPPING_OPERATOR_MAXWHITE,
		ToneMappingTypeReinhard02 = RPR_TONEMAPPING_OPERATOR_REINHARD02,
	};

	enum EnvironmentOverride
	{
		EnvironmentOverrideReflection = RPR_SCENE_ENVIRONMENT_OVERRIDE_REFLECTION,
		EnvironmentOverrideRefraction = RPR_SCENE_ENVIRONMENT_OVERRIDE_REFRACTION,
		EnvironmentOverrideTransparency = RPR_SCENE_ENVIRONMENT_OVERRIDE_TRANSPARENCY,
		EnvironmentOverrideBackground = RPR_SCENE_ENVIRONMENT_OVERRIDE_BACKGROUND,
	};

	enum PostEffectType
	{
		PostEffectTypeToneMap = RPR_POST_EFFECT_TONE_MAP,
		PostEffectTypeWhiteBalance = RPR_POST_EFFECT_WHITE_BALANCE,
		PostEffectTypeSimpleTonemap = RPR_POST_EFFECT_SIMPLE_TONEMAP,
		PostEffectTypeNormalization = RPR_POST_EFFECT_NORMALIZATION,
		PostEffectTypeGammaCorrection = RPR_POST_EFFECT_GAMMA_CORRECTION,
	};

	enum CameraMode
	{
		CameraModePerspective = RPR_CAMERA_MODE_PERSPECTIVE,
		CameraModeOrthographic = RPR_CAMERA_MODE_ORTHOGRAPHIC,
		CameraModePanorama = RPR_CAMERA_MODE_LATITUDE_LONGITUDE_360,
		CameraModePanoramaStereo = RPR_CAMERA_MODE_LATITUDE_LONGITUDE_STEREO,
		CameraModeCubemap = RPR_CAMERA_MODE_CUBEMAP,
		CameraModeCubemapStereo = RPR_CAMERA_MODE_CUBEMAP_STEREO,
	};

	enum ErrorCode
	{
		ErrorSuccess = RPR_SUCCESS,
		ErrorComputeApiNotSupported = RPR_ERROR_COMPUTE_API_NOT_SUPPORTED,
		ErrorOutOfSystemMemory = RPR_ERROR_OUT_OF_SYSTEM_MEMORY,
		ErrorOutOfVideoMemory = RPR_ERROR_OUT_OF_VIDEO_MEMORY,
		ErrorInvalidLightPathExpression = RPR_ERROR_INVALID_LIGHTPATH_EXPR,
		ErrorInvalidImage =	RPR_ERROR_INVALID_IMAGE,
		ErrorInvalidAAMethod = RPR_ERROR_INVALID_AA_METHOD,
		ErrorUnsupportedImageFormat = RPR_ERROR_UNSUPPORTED_IMAGE_FORMAT,
		ErrorInvalidGlTexture = RPR_ERROR_INVALID_GL_TEXTURE,
		ErrorInvalidClImage = RPR_ERROR_INVALID_CL_IMAGE,
		ErrorInvalidObject = RPR_ERROR_INVALID_OBJECT,
		ErrorInvalidParameter = RPR_ERROR_INVALID_PARAMETER,
		ErrorInvalidTag = RPR_ERROR_INVALID_TAG,
		ErrorInvalidLight = RPR_ERROR_INVALID_LIGHT,
		ErrorInvalidContext = RPR_ERROR_INVALID_CONTEXT,
		ErrorUnimplemented = RPR_ERROR_UNIMPLEMENTED,
		ErrorInvalidApiVersion = RPR_ERROR_INVALID_API_VERSION,
		ErrorInternalError = RPR_ERROR_INTERNAL_ERROR,
		ErrorIoError = RPR_ERROR_IO_ERROR,
		ErrorUnsupportedShaderParameterType = RPR_ERROR_UNSUPPORTED_SHADER_PARAMETER_TYPE,
		ErrorMaterialStackOverflow = RPR_ERROR_MATERIAL_STACK_OVERFLOW
	};

	enum Constant
	{
		MaximumSubdividedFacecount = 100000,
	};

	class Matrix
	{
		float m[16] = {
			1,0,0,0,
			0,1,0,0,
			0,0,1,0,
			0,0,0,1
		};

	public:
		Matrix(const float* f, bool transpose = false)
		{
			for (int i = 0; i < 4; i++)
				for (int j = 0; j < 4; j++)
					m[i * 4 + j] = transpose ? f[j * 4 + i] : f[i * 4 + j];
		}

	};

	// a self deleting shared object
	// base class for objects that need to manage their own lifespan
	class Object
	{
		// should never be stored as address, always treat as value
		void operator&() const = delete;

	protected:
		class Data;
		typedef std::shared_ptr<Data> DataPtr;

		// the internal wrapper of the handle, + references and context if applicable
		// note that this is protected so as to avoid the hideous problems
		// caused by accidentally duplicating shared pointers when given access
		// to their contents.
		class Data
		{
			friend Object;
		private:
			void*					handle = nullptr;
			bool					destroyOnDelete = true;	// handle will be deleted?
#if FRW_LOGGING
			const char* typeNameMirror = "UnknownType";
#endif

			bool operator==(void* h) const = delete;

		protected:
			void Init(void* h, const Context& context, bool destroyOnDelete);

		public:
			DataPtr					context;
			std::set<DataPtr>		references;	// list of references to objects used by this object
			size_t					userData = 0;	// simple l-value for user reference

			Data() {}
			Data(void* h, const Context& context, bool destroyOnDelete = true)
			{
				Init(h, context, destroyOnDelete);
			}
			virtual ~Data();

			void Attach(void* h, bool destroyOnDelete = true);
			void* Handle() const { return handle; }

			virtual bool IsValid() const { return handle != nullptr; }
			virtual const char* GetTypeName() const { return "Object"; }
		};

		DataPtr m;	// never null

		Object(DataPtr p) : m(p)
		{}

		Object(void* h, const Context& context, bool destroyOnDelete = true, Data* data = nullptr)
		{
			if (!data)
				data = new Data(h, context, destroyOnDelete);
			else
				data->Init(h, context, destroyOnDelete);
			m.reset(data);
		}

		void AddReference(const Object * pValue)
		{
			if (pValue)
				AddReference(*pValue);
		}

		void AddReference(const Object & value)
		{
			if (value)	// shouldn't be necessary but lets keep things tidy
				m->references.insert(value.m);
		}

		void RemoveReference(const Object & value)
		{
			m->references.erase(value.m);
		}

		void RemoveReference(void* h)
		{
			for (auto it = m->references.begin(); it != m->references.end(); )
			{
				if ((*it)->Handle() == h)
					it = m->references.erase(it);
				else
					++it;
			}
		}


		template <class T>
		T FindRef(void* h) const
		{
			T ret;
			if (h)
			{
				for (auto ref : m->references)
				{
					if (ref->Handle() == h)
					{
						ret.m = ref;
						break;
					}
				}
			}
			return ret;
		}

		void RemoveAllReferences()
		{
			m->references.clear();
		}

		long ReferenceCount() const { return (long)m->references.size(); }
		long UseCount() const { return m.use_count(); }


	public:
		Object(Data* data = nullptr)
		{
			if (!data) data = new Data();
			m.reset(data);
		}

		void Reset()
		{
			m = std::make_shared<Data>();
		}

		bool operator==(const Object& rhs) const { return m.get() == rhs.m.get(); }
		bool operator<(const Object& rhs) const { return (size_t)m.get() < (size_t)rhs.m.get(); };

		void* Handle() const { return m->Handle(); }

		Context GetContext() const;

		void SetUserData(size_t v) { m->userData = v; }
		size_t GetUserData() const { return m->userData; }

		const char* GetTypeName() const { return m->GetTypeName(); }

		// for easier scope and creation flow
		explicit operator bool() const { return m->IsValid(); }
		bool IsValid() const { return m->IsValid(); }

		// TODO: object's data has type-info, so can use faster code avoiding use of dynamic_cast
		template <class T>
		T As()
		{
			Data* data = m.get();
			if (!data || dynamic_cast<typename T::Data*>(data))
				return reinterpret_cast<T&>(*this);
			return T();	// return a null otherwise
		}
	};

/*
	Helper macros for defining Object child classes.

	Each class will have its 'Data' class derived from Parent::Data. It is pointed by Object::m, however
	'm' points to Object::Data. To automatically cast to ClassName::Data, use generated data() function.
	Use DECLARE_OBJECT to define a class with custom data, or DECLARE_OBJECT_NO_DATA to define a class
	with fully derived data set. Insert DECLARE_OBJECT_DATA into Data definition to add necessary data
	members. 'm' is assigned in Object's constructor, so you should pass allocated Data instance to
	parent's constructor. If your class is supposed to be used as parent for another class, its constructor
	should have 'Data* d = nullptr' parameter: child class will pass its Data structure there, but when
	'this' class created itself, 'd' will be null - and in this case we should allocate data.

	Typical constructor for class with children:

	inline Node::Node(const MaterialSystem& ms, int type, bool destroyOnDelete, Node::Data* _data)
		: Object(ms.CreateNode(type)
		, ms.GetContext()
		, destroyOnDelete
		, _data ? _data : new Node::Data()) // here we're either creating Node::Data, or passing child's data class to Object's constructor
	{
		data().materialSystem = ms; // we're accessing local Data class here
	}

	It is possible to create constructor which will accept RPR object's handle and context (and 'Data* d')
	as input, however some classes has different set of parameters.
*/

// TODO: if Object::As will be refactored, it is worth removing 'friend Object' from DECLARE_OBJECT macro
#define DECLARE_OBJECT(ClassName, Parent)	\
		friend Object;						\
	protected:								\
		typedef ClassName ThisClass;		\
		typedef Parent Super;				\
		class Data;							\
		Data& data() const { return *static_cast<Data*>(m.get()); } \
	public:									\
		ClassName(Data* data = nullptr)		\
		: Parent(data ? data : new Data())	\
		{}									\
	protected:								\
		static const char* GetTypeName()	\
		{									\
			static_assert(sizeof(ThisClass) == sizeof(std::shared_ptr<frw::Object::Data>), "Error in class " #ClassName "! It should not contain any data fields"); \
			return #ClassName;				\
		}

#define DECLARE_OBJECT_NO_DATA(ClassName, Parent) \
	DECLARE_OBJECT(ClassName, Parent)		\
		class Data : public Parent::Data	\
		{									\
			DECLARE_OBJECT_DATA				\
		};

#define DECLARE_OBJECT_DATA					\
	public:									\
		virtual const char* GetTypeName() const	{ return ThisClass::GetTypeName(); } \
	private:


	class Node : public Object
	{
		DECLARE_OBJECT(Node, Object);
		class Data : public Object::Data
		{
			DECLARE_OBJECT_DATA
		public:
			Object materialSystem;
			int type;
		};

	protected:
		// This constructor is used by Shader, when RPRX material created
		Node(const Context& context, Data* data)
		: Object(nullptr, context, true, data)
		{}

	public:
		Node(const MaterialSystem& ms, int type, bool destroyOnDelete = true, Data* data = nullptr);	// not typesafe

		bool SetValue(const char* key, const Value& v);
		bool SetValueInt(const char* key, int);

		MaterialSystem GetMaterialSystem() const;

		/// do not use if you can avoid it (unsafe)
		void _SetInputNode(const char* key, const Shader& shader);

		int GetType() const
		{
			return data().type;
		}

		int GetParameterCount() const
		{
			size_t n = 0;
			auto res = rprMaterialNodeGetInfo(Handle(), NodeInfoInputCount, sizeof(n), &n, nullptr);
			checkStatus(res);
			return (int)n;
		}

		class ParameterInfo
		{
		public:
			NodeInputId id;
			NodeInputType type;
			std::string name;
		};

		ParameterInfo GetParameterInfo(int i) const
		{
			ParameterInfo info = {};
			size_t size = 0;
			rpr_int res = RPR_SUCCESS;

			res = rprMaterialNodeGetInputInfo(Handle(), i, NodeInputInfoId, sizeof(info.id), &info.id, nullptr);
			checkStatus(res);

			// get type
			res = rprMaterialNodeGetInputInfo(Handle(), i, NodeInputInfoType, sizeof(info.type), &info.type, nullptr);
			checkStatus(res);

			// Get name
			res = rprMaterialNodeGetInputInfo(Handle(), i, NodeInputInfoName, 0, nullptr, &size);
			checkStatus(res);

			info.name.resize(size);
			res = rprMaterialNodeGetInputInfo(Handle(), i, NodeInputInfoName, size, const_cast<char*>(info.name.data()), nullptr);
			checkStatus(res);

			return info;
		}
	};

	class ValueNode : public Node
	{
		DECLARE_OBJECT_NO_DATA(ValueNode, Node);
	public:
//		ValueNode(void* h, const MaterialSystem &ms) : Node(h, ms) {}
		ValueNode(const MaterialSystem& ms, ValueType type) : Node(ms, type, true, new Data()) {}
	};


	class Value
	{
		ValueNode node;
#pragma warning(push)
#pragma warning(disable:4201)

		union
		{
			struct
			{
				rpr_float x, y, z, w;
			};
		};
#pragma warning(pop)

		enum
		{
			FLOAT,
			NODE,
		} type;

	public:
		// nullable
		Value(std::nullptr_t = nullptr) : type(NODE)
		{ }

		// single scalar value gets copied to all components
		Value(double s) : type(FLOAT)
		{
			x = y = z = w = (float)s;
		}

		// otherwise it's just zeros
		Value(double _x, double _y, double _z = 0, double _w = 0) : type(FLOAT)
		{
			x = rpr_float(_x);
			y = rpr_float(_y);
			z = rpr_float(_z);
			w = rpr_float(_w);
		}

#ifdef FRW_USE_MAX_TYPES
		Value(const Color& v) : type(FLOAT)
		{
			x = v.r;
			y = v.g;
			z = v.b;
			w = 0.0;
		}
		Value(const Point3& v) : type(FLOAT)
		{
			x = v.x;
			y = v.y;
			z = v.z;
			w = 0.0;
		}
#endif

		Value(ValueNode v) : node(v), type(NODE) {}

		// this basically means "has value that can be assigned as input"
		bool IsNull() const { return type == NODE && !node; }
		bool IsFloat() const { return type == FLOAT; }
		bool IsNode() const { return type == NODE && node; }
		Node GetNode() const { return node; }

		// returns true unless null
		explicit operator bool() const { return type != NODE || node; }

		// either a map or a float > 0
		bool NonZero() const
		{
			return type == FLOAT ? (x*x + y*y + z*z + w*w) > 0 : (type == NODE && node);
		}

		bool NonZeroXYZ() const
		{
			return type == FLOAT ? (x*x + y*y + z*z) > 0 : (type == NODE && node);
		}

		bool operator==(const Value& rhs) const
		{
			return type == rhs.type
				&& (type == NODE
					? (node == rhs.node)
					: (x == rhs.x && y == rhs.y && z == rhs.z && w == rhs.w));
		}

		bool operator!=(const Value& rhs) const { return !(*this == rhs); }

		// quick best value pick
		Value operator|(const Value &rhs) const
		{
			return *this ? *this : rhs;
		}

		Value Inverse() const;
		Value SelectX() const;
		Value SelectY() const;
		Value SelectZ() const;

		// casting
		explicit operator ValueNode() const { return ValueNode(type == NODE ? node : ValueNode()); }

		// access to X,Y and Z (or default value if we are a node)
		float GetX(double def = 0) const { return type == FLOAT ? x : float(def); }
		float GetY(double def = 0) const { return type == FLOAT ? y : float(def); }
		float GetZ(double def = 0) const { return type == FLOAT ? z : float(def); }

		// for combo-box value
		int GetInt(int def = 0) const { return type == FLOAT ? (int)x : def; }

		// for check-box value
		bool GetBool(bool def = false) const { return type == FLOAT ? (int)x != 0 : def; }

		int GetNodeType() const
		{
			if (type == NODE)
			{
				return node.GetType();
			}
			else
			{
				return -1;
			}
		}

		friend class Node;
		friend class Shader;
		friend class MaterialSystem;

		MaterialSystem GetMaterialSystem() const;

		static Value unknown;
	};

	class Shape : public Object
	{
		friend class Shader;

		DECLARE_OBJECT(Shape, Object);
		class Data : public Object::Data
		{
			DECLARE_OBJECT_DATA
		public:
			Object shader;
			Object volumeShader;
			Object displacementShader;
			virtual ~Data();

			bool isAreaLight = false;

			// Used to determine if we can setup displacement or not
			bool isUVCoordinatesSet = false;
		};

	public:
		Shape(rpr_shape h, const Context &context) : Object(h, context, true, new Data()) {}

		void SetShader(Shader shader);
		Shader GetShader() const;

		void SetVolumeShader( const Shader& shader );
		Shader GetVolumeShader() const;

		void SetVisibility(bool visible)
		{
			auto res = rprShapeSetVisibility(Handle(), visible);
			checkStatus(res);
		}
		void SetPrimaryVisibility(bool visible)
		{
#if RPR_API_VERSION != 0x010000110
#if RPR_API_VERSION >= 0x010032000
			auto res = rprShapeSetVisibilityFlag(Handle(), RPR_SHAPE_VISIBILITY_PRIMARY_ONLY_FLAG, visible);
#else
			auto res = rprShapeSetVisibilityEx(Handle(), "visible.primary", visible);
#endif
			checkStatus(res);
#endif
		}
		void SetReflectionVisibility(bool visible)
		{
#if RPR_API_VERSION >= 0x010032000
			auto res = rprShapeSetVisibilityFlag(Handle(), RPR_SHAPE_VISIBILITY_REFLECTION, visible);
			checkStatus(res);
			res = rprShapeSetVisibilityFlag(Handle(), RPR_SHAPE_VISIBILITY_GLOSSY_REFLECTION, visible);
			checkStatus(res);
#else
			auto res = rprShapeSetVisibilityEx(Handle(), "visible.reflection", visible);
			checkStatus(res);
			res = rprShapeSetVisibilityEx(Handle(), "visible.reflection.glossy", visible);
			checkStatus(res);
#endif
		}

		void setRefractionVisibility(bool visible)
		{
#if RPR_API_VERSION >= 0x010032000
			auto res = rprShapeSetVisibilityFlag(Handle(), RPR_SHAPE_VISIBILITY_REFRACTION, visible);
			checkStatus(res);
			res = rprShapeSetVisibilityFlag(Handle(), RPR_SHAPE_VISIBILITY_GLOSSY_REFRACTION, visible);
			checkStatus(res);
#else
			auto res = rprShapeSetVisibilityEx(Handle(), "visible.refraction", visible);
			checkStatus(res);
			res = rprShapeSetVisibilityEx(Handle(), "visible.refraction.glossy", visible);
			checkStatus(res);
#endif
		}

		void SetLightShapeVisibilityEx(bool visible)
		{
#if RPR_API_VERSION >= 0x010032000
			auto res = rprShapeSetVisibilityFlag(Handle(), RPR_SHAPE_VISIBILITY_LIGHT, visible);
#else
			auto res = rprShapeSetVisibilityEx(Handle(), "visible.light", visible);
#endif
			checkStatus(res);
		}

		Shape CreateInstance(Context context) const;
		void SetTransform(const float* tm, bool transpose = false)
		{
			auto res = rprShapeSetTransform(Handle(), transpose, tm);
			checkStatus(res);
		}

		void SetLinearMotion(float x, float y, float z)
		{
			auto res = rprShapeSetLinearMotion(Handle(), x, y, z);
			checkStatus(res);
		}

		void SetAngularMotion(float x, float y, float z, float w)
		{
			auto res = rprShapeSetAngularMotion(Handle(), x, y, z, w);
			checkStatus(res);
		}

#ifdef FRW_USE_MAX_TYPES
		void SetTransform(const Matrix3& tm)
		{
			float m44[16] = {};
			float* p = m44;
			for (int x = 0; x < 4; ++x)
			{
				for (int y = 0; y < 3; ++y)
					*p++ = tm.GetRow(x)[y];

				*p++ = (x == 3) ? 1.f : 0.f;
			}
			SetTransform(m44);
		}
#endif
		void SetShadowFlag(bool castsShadows)
		{
#if RPR_API_VERSION >= 0x010032000
			auto res = rprShapeSetVisibilityFlag(Handle(), RPR_SHAPE_VISIBILITY_SHADOW, castsShadows);
#else
			auto res = rprShapeSetVisibilityEx(Handle(), "visible.shadow", castsShadows);
#endif
			checkStatus(res);
		}

		void SetShadowCatcherFlag(bool shadowCatcher)
		{
#if RPR_API_VERSION >= 0x010000109
			auto res = rprShapeSetShadowCatcher(Handle(), shadowCatcher);
			checkStatus(res);
#endif
		}

		void SetDisplacement(Value image, float minscale = 0, float maxscale = 1);
		Node GetDisplacementMap();
		void RemoveDisplacement();
		void SetSubdivisionFactor(int sub);
		void SetSubdivisionCreaseWeight(float weight);
		void SetSubdivisionBoundaryInterop(rpr_subdiv_boundary_interfop_type type);

		bool IsAreaLight() { return data().isAreaLight; }
		void SetAreaLightFlag(bool isAreaLight) { data().isAreaLight = isAreaLight; }

		bool IsUVCoordinatesSet() const 
		{
			return data().isUVCoordinatesSet;
		}
		void SetUVCoordinatesSetFlag(bool flag)
		{
			data().isUVCoordinatesSet = flag;
		}

		bool IsInstance() const
		{
			rpr_shape_type type = RPR_SHAPE_TYPE_INSTANCE;
			auto res = rprShapeGetInfo(Handle(), RPR_SHAPE_TYPE, sizeof(type), &type, nullptr);
			checkStatus(res);
			return type == RPR_SHAPE_TYPE_INSTANCE;
		}

		int GetFaceCount() const
		{
			if (IsInstance())
				return 1;	// TODO: need to be able to dereference instances!

			size_t n = 0;
			auto res = rprMeshGetInfo(Handle(), RPR_MESH_POLYGON_COUNT, sizeof(n), &n, nullptr);
			checkStatus(res);
			return (int)n;
		}
	};

	class Light : public Object
	{
		DECLARE_OBJECT_NO_DATA(Light, Object);
	public:
		Light(rpr_light h, const Context &context, Data* data = nullptr) : Object(h, context, true, data ? data : new Data()) {}
		void SetTransform(const float* tm, bool transpose = false)
		{
			auto res = rprLightSetTransform(Handle(), transpose, tm);
			checkStatus(res);
		}
#ifdef FRW_USE_MAX_TYPES
		void SetTransform(const Matrix3& tm)
		{
			float m44[16] = {};
			float* p = m44;
			for (int x = 0; x < 4; ++x)
			{
				for (int y = 0; y < 3; ++y)
					*p++ = tm.GetRow(x)[y];

				*p++ = (x == 3) ? 1.f : 0.f;
			}
			SetTransform(m44);
		}
#endif
	};

	class Image : public Object
	{
		DECLARE_OBJECT_NO_DATA(Image, Object);
	public:
		explicit Image(rpr_image h, const Context& context) : Object(h, context, true, new Data()) {}

		Image(Context context, float r, float g, float b);
		Image(Context context, const rpr_image_format& format, const rpr_image_desc& image_desc, const void* data);
		Image(Context context, const char * filename);
		void SetGamma(float gamma)
		{
#if (RPR_API_VERSION >= 0x010029100) 
			rpr_int res = rprImageSetGamma(Handle(), gamma);
			checkStatus(res);
#endif
		}
	};

	class PointLight : public Light
	{
		DECLARE_OBJECT_NO_DATA(PointLight, Light);
	public:
		PointLight(rpr_light h, const Context &context) : Light(h, context, new Data()) {}
		void SetRadiantPower(float r, float g, float b)
		{
			auto res = rprPointLightSetRadiantPower3f(Handle(), r, g, b);
			checkStatus(res);
		}
	};

	class SpotLight : public Light
	{
		DECLARE_OBJECT_NO_DATA(SpotLight, Light);
	public:
		SpotLight(rpr_light h, const Context &context) : Light(h, context, new Data()) {}
		void SetRadiantPower(float r, float g, float b)
		{
			auto res = rprSpotLightSetRadiantPower3f(Handle(), r, g, b);
			checkStatus(res);
		}
		void SetConeShape(float innerAngle, float outerAngle)
		{
			auto res = rprSpotLightSetConeShape(Handle(), innerAngle, outerAngle);
			checkStatus(res);
		}
	};

	class DirectionalLight : public Light
	{
		DECLARE_OBJECT_NO_DATA(DirectionalLight, Light);
	public:
		DirectionalLight(rpr_light h, const Context &context) : Light(h, context, new Data()) {}

		void SetRadiantPower(float r, float g, float b)
		{
			auto res = rprDirectionalLightSetRadiantPower3f(Handle(), r, g, b);
			checkStatus(res);
		}
	};

	class IESLight : public Light
	{
		DECLARE_OBJECT_NO_DATA(IESLight, Light);
	public:
		IESLight(rpr_light h, const Context &context) : Light(h, context, new Data()) {}

		int SetIESFile(const char* filePath, int xImageResolution, int yImageResolution)
		{
			auto res = rprIESLightSetImageFromFile(Handle(), filePath, xImageResolution, yImageResolution);
			assert(RPR_SUCCESS == res);
			return res;
		}

		int SetIESData(const char* iesData, int xImageResolution, int yImageResolution)
		{
			auto res = rprIESLightSetImageFromIESdata(Handle(), iesData, xImageResolution, yImageResolution);
			assert(RPR_SUCCESS == res);
			return res;
		}

		void SetRadiantPower(float r, float g, float b)
		{
			auto res = rprIESLightSetRadiantPower3f(Handle(), r, g, b);
			assert(RPR_SUCCESS == res);
		}
	};

	class EnvironmentLight : public Light
	{
		DECLARE_OBJECT(EnvironmentLight, Light);
		class Data : public Light::Data
		{
			DECLARE_OBJECT_DATA
		public:
			Image image;
			bool isAmbientLight = false;
		};
	public:
		EnvironmentLight(rpr_light h, const Context &context) : Light(h, context, new Data()) {}
		void SetLightIntensityScale(float k)
		{
			auto res = rprEnvironmentLightSetIntensityScale(Handle(), k);
			checkStatus(res);
		}
		void SetImage(Image img);
		void AttachPortal(Shape shape);
		void DetachPortal(Shape shape);
		void SetAmbientLight(bool value)
		{
			data().isAmbientLight = value;
		}
		bool IsAmbientLight() const
		{
			return data().isAmbientLight;
		}
	};

	class Camera : public Object
	{
		DECLARE_OBJECT(Camera, Object);
		class Data : public Object::Data
		{
			DECLARE_OBJECT_DATA;
		public:
			int m_mode = 0;
		};

	public:
		Camera(rpr_camera h, const Context &context) : Object(h, context, true, new Data()) {}

		void SetMode(CameraMode mode)
		{
			if (data().m_mode == mode)
				return;

			auto res = rprCameraSetMode(Handle(), mode);
			checkStatus(res, "Error setting camera mode");

			data().m_mode = mode;
		}
	};

	class Scene : public Object
	{
		DECLARE_OBJECT(Scene, Object);
		class Data : public Object::Data
		{
			DECLARE_OBJECT_DATA;
		public:
			Camera camera;
			Image backgroundImage;
			std::map<EnvironmentOverride, EnvironmentLight> overrides;
			int mAreaLightCount = 0;
			int mShapeCount = 0;
			int mLightCount = 0;
		};

	public:
		Scene(rpr_scene h, const Context &context) : Object(h, context, true, new Data()) {}

		void Attach(Shape v)
		{
			AddReference(v);
			auto res = rprSceneAttachShape(Handle(), v.Handle());
			checkStatus(res);

			if (v.IsAreaLight()) data().mAreaLightCount++;
			data().mShapeCount++;
		}
		void Attach(Light v)
		{
			AddReference(v);
			auto res = rprSceneAttachLight(Handle(), v.Handle());
			checkStatus(res);

			data().mLightCount++;
		}
		void Detach(Shape v)
		{
			RemoveReference(v);
			auto res = rprSceneDetachShape(Handle(), v.Handle());
			checkStatus(res);

			if (v.IsAreaLight()) data().mAreaLightCount--;
			data().mShapeCount--;
		}
		void Detach(Light v)
		{
			RemoveReference(v);
			auto res = rprSceneDetachLight(Handle(), v.Handle());
			checkStatus(res);

			data().mLightCount--;
		}

		void SetCamera(Camera v)
		{
			auto res = rprSceneSetCamera(Handle(), v.Handle());
			checkStatus(res);
			data().camera = v;
		}
		Camera GetCamera()
		{
			return data().camera;
		}

		void Clear()
		{
			SetCamera(Camera());

			auto res = rprSceneClear(Handle());
			checkStatus(res);
			RemoveAllReferences();
			// clear local data
			Data& d = data();
			d.camera = Camera();
			d.backgroundImage = Image();
			d.overrides.clear();

			data().mAreaLightCount = data().mLightCount = data().mShapeCount = 0;
		}

		int AreaLightCount() const
		{
			return data().mAreaLightCount;
		}

		int ShapeObjectCount() const
		{
			return data().mShapeCount - data().mAreaLightCount;
		}

		int LightObjectCount() const
		{
			return data().mLightCount + data().mAreaLightCount;
		}

		int ShapeCount() const
		{
			return data().mShapeCount;
		}

		void DetachShapes()
		{
			auto count = ShapeCount();
			std::vector<rpr_shape> items(count);
			auto res = rprSceneGetInfo(Handle(), RPR_SCENE_SHAPE_LIST, count * sizeof(rpr_shape), items.data(), nullptr);
			checkStatus(res);

			for (auto& it : items)
			{
				res = rprSceneDetachShape(Handle(), it);
				checkStatus(res);
				RemoveReference(it);
			}

			data().mShapeCount = 0;
		}

		std::list<Shape> GetShapes() const
		{
			auto count = ShapeCount();
			std::vector<rpr_shape> items(count);
			auto res = rprSceneGetInfo(Handle(), RPR_SCENE_SHAPE_LIST, count * sizeof(rpr_shape), items.data(), nullptr);
			checkStatus(res);
			std::list<Shape> ret;
			for (auto& it : items)
			{
				if (auto shape = FindRef<Shape>(it))
					ret.push_back(shape);
			}
			return ret;
		}

		int LightCount() const
		{
			return data().mLightCount;
		}

		std::list<Light> GetLights() const
		{
			auto count = LightCount();
			std::vector<rpr_light> items(count);
			auto res = rprSceneGetInfo(Handle(), RPR_SCENE_LIGHT_LIST, count * sizeof(rpr_shape), items.data(), nullptr);
			checkStatus(res);
			std::list<Light> ret;
			for (auto& it : items)
			{
				if (auto light = FindRef<Light>(it))
					ret.push_back(light);
			}
			return ret;
		}

		void DetachLights()
		{
			auto count = LightCount();
			std::vector<rpr_light> items(count);
			auto res = rprSceneGetInfo(Handle(), RPR_SCENE_LIGHT_LIST, count * sizeof(rpr_shape), items.data(), nullptr);
			checkStatus(res);
			for (auto& it : items)
			{
				res = rprSceneDetachLight(Handle(), it);
				checkStatus(res);
				RemoveReference(it);
			}
			data().mLightCount = 0;
		}

		void SetEnvironmentOverride(EnvironmentOverride e, EnvironmentLight light)
		{
			auto res = rprSceneSetEnvironmentOverride(Handle(), e, light.Handle());
			checkStatus(res);
			data().overrides[e] = light;
		}

		void ClearEnvironmentOverrides()
		{
			for (const auto& it : data().overrides)
			{
				auto res = rprSceneSetEnvironmentOverride(Handle(), it.first, nullptr);
				checkStatus(res);
			}
			data().overrides.clear();
		}

		void SetBackgroundImage(Image v)
		{
			auto res = rprSceneSetBackgroundImage(Handle(), v.Handle());
			checkStatus(res);
			data().backgroundImage = v;
		}
		Image GetBackgroundImage()
		{
			return data().backgroundImage;
		}

		void SetActive();
	};

	class Context : public Object
	{
		DECLARE_OBJECT(Context, Object);
		class Data : public Object::Data
		{
			DECLARE_OBJECT_DATA;
		public:
			Data()
			: scene(nullptr)
			{}
			rpr_scene scene;		// scene handle; not frw::Scene to avoid circular references
		};

	public:
		explicit Context(DataPtr p)
		{
			m = p;
		}

		explicit Context(rpr_context h, bool destroyOnDelete = true) : Object(h, *this, destroyOnDelete, new Data())
		{
		}

		void SetParameter(const char * sz, rpr_uint v)
		{
			auto res = rprContextSetParameter1u(Handle(), sz, v);
			checkStatus(res);
		}
		void SetParameter(const char * sz, int v)
		{
			auto res = rprContextSetParameter1u(Handle(), sz, v);
			checkStatus(res);
		}
		void SetParameter(const char * sz, double v)
		{
			auto res = rprContextSetParameter1f(Handle(), sz, (rpr_float)v);
			checkStatus(res);
		}
		void SetParameter(const char * sz, double x, double y, double z)
		{
			auto res = rprContextSetParameter3f(Handle(), sz, (rpr_float)x, (rpr_float)y, (rpr_float)z);
			checkStatus(res);
		}
		void SetParameter(const char * sz, double x, double y, double z, double w)
		{
			auto res = rprContextSetParameter4f(Handle(), sz, (rpr_float)x, (rpr_float)y, (rpr_float)z, (rpr_float)w);
			checkStatus(res);
		}

		Scene CreateScene()
		{
			FRW_PRINT_DEBUG("CreateScene()");
			rpr_scene v;
			auto status = rprContextCreateScene(Handle(), &v);
			checkStatusThrow(status, "Unable to create scene");

			data().scene = v;
			return Scene(v, *this);
		}

		rpr_scene GetScene() const
		{
			return data().scene;
		}

		Camera CreateCamera()
		{
			FRW_PRINT_DEBUG("CreateCamera()");
			rpr_camera v;
			auto status = rprContextCreateCamera(Handle(), &v);
			checkStatusThrow(status, "Unable to create camera");

			return Camera(v, *this);
		}

		Shape CreateMesh(const rpr_float* vertices, size_t num_vertices, rpr_int vertex_stride,
			const rpr_float* normals, size_t num_normals, rpr_int normal_stride,
			const rpr_float* texcoords, size_t num_texcoords, rpr_int texcoord_stride,
			const rpr_int*   vertex_indices, rpr_int vidx_stride,
			const rpr_int*   normal_indices, rpr_int nidx_stride,
			const rpr_int*   texcoord_indices, rpr_int tidx_stride,
			const rpr_int*   num_face_vertices, size_t num_faces);

		Shape CreateMeshEx(const rpr_float* vertices, size_t num_vertices, rpr_int vertex_stride,
			const rpr_float* normals, size_t num_normals, rpr_int normal_stride,
			const rpr_int* perVertexFlag, size_t num_perVertexFlags, rpr_int perVertexFlag_stride,
			rpr_int numberOfTexCoordLayers, const rpr_float** texcoords, const size_t* num_texcoords, const rpr_int* texcoord_stride,
			const rpr_int* vertex_indices, rpr_int vidx_stride,
			const rpr_int* normal_indices, rpr_int nidx_stride, const rpr_int** texcoord_indices,
			const rpr_int* tidx_stride, const rpr_int * num_face_vertices, size_t num_faces);

		PointLight CreatePointLight()
		{
			FRW_PRINT_DEBUG("CreatePointLight()");
			rpr_light h;
			auto status = rprContextCreatePointLight(Handle(), &h);
			checkStatusThrow(status, "Unable to create point light");

			return PointLight(h, *this);
		}

		SpotLight CreateSpotLight()
		{
			FRW_PRINT_DEBUG("CreateSpotLight()");
			rpr_light h;
			auto status = rprContextCreateSpotLight(Handle(), &h);
			checkStatusThrow(status, "Unable to create spot light");

			return SpotLight(h, *this);
		}

		EnvironmentLight CreateEnvironmentLight()
		{
			FRW_PRINT_DEBUG("CreateEnvironmentLight()");
			rpr_light h;
			auto status = rprContextCreateEnvironmentLight(Handle(), &h);
			checkStatusThrow(status, "Unable to create environment light");

			return EnvironmentLight(h, *this);
		}

		DirectionalLight CreateDirectionalLight()
		{
			FRW_PRINT_DEBUG("CreateDirectionalLight()");
			rpr_light h;
			auto status = rprContextCreateDirectionalLight(Handle(), &h);
			checkStatusThrow(status, "Unable to create directional light");

			return DirectionalLight(h, *this);
		}

		IESLight CreateIESLight()
		{
			FRW_PRINT_DEBUG("CreateIESLight()");
			rpr_light h;
			auto status = rprContextCreateIESLight(Handle(), &h);
			checkStatusThrow(status, "Unable to create IES light");

			return IESLight(h, *this);
		}

		// context state
		void SetScene(Scene scene)
		{
			assert(!scene || scene.GetContext() == *this);

			auto status = rprContextSetScene(Handle(), scene.Handle());
			checkStatusThrow(status, "Unable to set scene");
		}

		void Attach(PostEffect post_effect);

		void Detach(PostEffect post_effect);

		/// set the target buffer for render calls
		void SetAOV(FrameBuffer frameBuffer, rpr_aov aov = RPR_AOV_COLOR);

		void Render()
		{
			auto status = rprContextRender(Handle());

#ifndef SHOW_EXTENDED_ERROR_MSG
			checkStatusThrow(status, "Unable to render");
#else
			if (status != RPR_SUCCESS)
			{
				size_t length;
				auto status2 = rprContextGetInfo(Handle(), RPR_CONTEXT_LAST_ERROR_MESSAGE, sizeof(size_t), nullptr, &length);
				std::vector<rpr_char> path(length);
				auto status3 = rprContextGetInfo(Handle(), RPR_CONTEXT_LAST_ERROR_MESSAGE, path.size(), &path[0], nullptr);

				std::string error_msg(path.begin(), path.end());
				LogPrint(error_msg.c_str());

				checkStatusThrow(status, error_msg.c_str());
			}
#endif
		}

		void RenderTile(int rxmin, int rxmax, int rymin, int rymax)
		{
			auto status = rprContextRenderTile(Handle(), rxmin, rxmax, rymin, rymax);
			checkStatusThrow(status, "Unable to render tile");
		}

		size_t GetMemoryUsage() const
		{
			rpr_render_statistics statistics = {};
			auto res = rprContextGetInfo(Handle(), RPR_CONTEXT_RENDER_STATISTICS, sizeof(statistics), &statistics, nullptr);
			checkStatus(res);
			return statistics.gpumem_usage;
		}
		int GetMaterialStackSize() const
		{
			size_t info = 0;
			auto res = rprContextGetInfo(Handle(), RPR_CONTEXT_MATERIAL_STACK_SIZE, sizeof(info), &info, nullptr);
			checkStatus(res);
			return (int)info;
		}

		int GetParameterCount() const
		{
			size_t info = 0;
			auto res = rprContextGetInfo(Handle(), RPR_CONTEXT_PARAMETER_COUNT, sizeof(info), &info, nullptr);
			checkStatus(res);
			return (int)info;
		}

		class ParameterInfo
		{
		public:
			ContextParameterId	id;
			std::string name;
			std::string description;
			ContextParameterType type;
		};

		ParameterInfo GetParameterInfo(int i) const
		{
			ParameterInfo info = {};
			size_t size = 0;

			// get id
			auto res = rprContextGetParameterInfo(Handle(), i, ParameterInfoId, sizeof(info.id), &info.id, nullptr);
			checkStatus(res);

			// get name
			res = rprContextGetParameterInfo(Handle(), i, ParameterInfoName, 0, nullptr, &size);
			checkStatus(res);
			info.name.resize(size);
			res = rprContextGetParameterInfo(Handle(), i, ParameterInfoName, size, const_cast<char*>(info.name.data()), nullptr);
			checkStatus(res);

			// get description
			res = rprContextGetParameterInfo(Handle(), i, ParameterInfoDescription, 0, nullptr, &size);
			checkStatus(res);
			info.description.resize(size);
			res = rprContextGetParameterInfo(Handle(), i, ParameterInfoDescription, size, const_cast<char*>(info.description.data()), nullptr);
			checkStatus(res);


			// get type
			res = rprContextGetParameterInfo(Handle(), i, ParameterInfoType, sizeof(info.type), &info.type, nullptr);
			checkStatus(res);

			return info;
		}

		static void TraceOutput(const char * tracingfolder)
		{
			rprContextSetParameter1u(nullptr, "tracing", 0);
			if (tracingfolder)
			{
				auto res = rprContextSetParameterString(nullptr, "tracingfolder", tracingfolder);
				if (RPR_SUCCESS == res)
					rprContextSetParameter1u(nullptr, "tracing", 1);
			}
		}
		void DumpParameterInfo();
	};



	class ImageNode : public ValueNode
	{
	public:
		explicit ImageNode(const MaterialSystem& h) : ValueNode(h, ValueTypeImageMap) {}
		rpr_int SetMap(Image v)
		{
			AddReference(v);
			return rprMaterialNodeSetInputImageData(Handle(), "data", v.Handle());
		}
	};

	class LookupNode : public ValueNode
	{
	public:
		LookupNode(const MaterialSystem& h, LookupType v) : ValueNode(h, ValueTypeLookup)
		{
			SetValueInt("value", v);
		}
	};

	class ArithmeticNode : public ValueNode
	{
	public:
		ArithmeticNode(const MaterialSystem& h, Operator op) : ValueNode(h, ValueTypeArithmetic)
		{
			SetValueInt("op", op);
		}
		ArithmeticNode(const MaterialSystem& h, Operator op, const Value& a) : ValueNode(h, ValueTypeArithmetic)
		{
			SetValueInt("op", op);
			SetValue("color0", a);
		}
		ArithmeticNode(const MaterialSystem& h, Operator op, const Value& a, const Value& b) : ValueNode(h, ValueTypeArithmetic)
		{
			SetValueInt("op", op);
			SetValue("color0", a);
			SetValue("color1", b);
		}
		// arithmetic node doesn't support 3 operators
		void SetArgument(int i, const Value& v)
		{
			switch (i)
			{
			case 0: SetValue("color0", v); break;
			case 1: SetValue("color1", v); break;
			}
		}
	};

	class NormalMapNode : public ValueNode
	{
	public:
		explicit NormalMapNode(const MaterialSystem& h) : ValueNode(h, ValueTypeNormalMap) {}

		void SetMap(const Value& v)
		{
			if (v.IsNode())
			{
				Node n = v.GetNode();
				AddReference(n);
				auto res = rprMaterialNodeSetInputN(Handle(), "color", n.Handle());
				checkStatus(res);
			}
		}
	};

	class BumpMapNode : public ValueNode
	{
	public:
		explicit BumpMapNode(const MaterialSystem& h) : ValueNode(h, ValueTypeBumpMap) {}

		void SetMap(const Value& v)
		{
			if (v.IsNode())
			{
				Node n = v.GetNode();
				AddReference(n);
				auto res = rprMaterialNodeSetInputN(Handle(), "color", n.Handle());
				checkStatus(res);
			}
		}
	};

    class AOMapNode : public ValueNode
    {
    public:
        explicit AOMapNode(const MaterialSystem& h, const Value& radius, const Value& side) : ValueNode(h, ValueTypeAOMap)
        {
            SetValue("radius", radius);
            SetValue("side", side);
        }
    };

	class MaterialSystem : public Object
	{
		static const bool allowShortcuts = true;
		DECLARE_OBJECT(MaterialSystem, Object);

		class Data : public Object::Data
		{
			DECLARE_OBJECT_DATA;
		public:
			virtual ~Data()
			{
				if (rprxContext)
				{
					rprxDeleteContext(rprxContext);
					FRW_PRINT_DEBUG("\tDeleted RPRX context 0x%016llX", rprxContext);
				}
			}
			rprx_context rprxContext = nullptr;
		};

	protected:
		friend class Node;
		rpr_material_node CreateNode(rpr_material_node_type type) const
		{
			FRW_PRINT_DEBUG("CreateNode(%d) in MaterialSystem: 0x%016llX", type, Handle());
			rpr_material_node node = nullptr;
			auto res = rprMaterialSystemCreateNode(Handle(), type, &node);
			checkStatus(res);
			return node;
		}

	public:
		explicit MaterialSystem(DataPtr p)
		{
			m = p;
		}

		explicit MaterialSystem(const Context& c, rpr_material_system h = nullptr, bool destroyOnDelete = true) : Object(h, c, destroyOnDelete, new Data())
		{
			if (!h)
			{
				FRW_PRINT_DEBUG("CreateMaterialSystem()");
				rpr_material_system hMS = nullptr;
				rpr_int res = rprContextCreateMaterialSystem(c.Handle(), 0, &hMS);
				checkStatus(res);
				m->Attach(hMS, destroyOnDelete);
				res = rprxCreateContext(Handle(), 0, &data().rprxContext);
				checkStatus(res);
				FRW_PRINT_DEBUG("\tCreated RPRX context 0x%016llX", data().rprxContext);
			}
		}

		rprx_context GetRprxContext() const { return data().rprxContext; }

		Value ValueBlend(const Value& a, const Value& b, const Value& t) const
		{
			// shortcuts
			if (t.IsFloat())
			{
				if (t.x <= 0)
					return a;
				if (t.x >= 1)
					return b;
			}

			if (allowShortcuts && a.IsFloat() && b.IsFloat() && t.IsFloat())
			{
				auto k = std::min(std::max(t.x, 0.0f), 1.0f);

				return Value(
					a.x * (1 - k) + b.x * k,
					a.y * (1 - k) + b.y * k,
					a.z * (1 - k) + b.z * k,
					a.w * (1 - k) + b.w * k
					);
			}

			ValueNode node(*this, ValueTypeBlend);
			node.SetValue("color0", a);
			node.SetValue("color1", b);
			node.SetValue("weight", t);
			return node;
		}

		// unclamped, multichannel version of ValueBlend
		Value ValueMix(const Value& a, const Value& b, const Value& t) const
		{
			return ValueAdd(
				ValueMul(a, ValueSub(1, t)),
				ValueMul(b, t)
				);
		}

		Value ValueAdd(const Value& a, const Value& b) const
		{
			if (a.IsNull())
				return b;

			if (b.IsNull())
				return a;

			if (!a.NonZero())
				return b;

			if (!b.NonZero())
				return a;

			if (allowShortcuts && a.IsFloat() && b.IsFloat())
				return Value(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);

			return ArithmeticNode(*this, OperatorAdd, a, b);
		}
		Value ValueAdd(const Value& a, const Value& b, const Value& c) const
		{
			return ValueAdd(ValueAdd(a, b), c);
		}
		Value ValueAdd(const Value& a, const Value& b, const Value& c, const Value& d) const
		{
			return ValueAdd(ValueAdd(a, b), ValueAdd(c,d));
		}

		Value ValueSub(Value a, const Value& b) const
		{
			if (a.IsNull())
				a = 0.;

			if (!b.NonZero())
				return a;

			if (allowShortcuts && a.IsFloat() && b.IsFloat())
				return Value(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w);

			return ArithmeticNode(*this, OperatorSubtract, a, b);
		}

		Value ValueMul(const Value& a, const Value& b) const
		{
			if (!a || !b)
				return Value();

			if (!a.NonZero() || !b.NonZero())
				return 0.;

			if (b == 1)
				return a;

			if (a == 1)
				return b;

			if (allowShortcuts && a.IsFloat() && b.IsFloat())
				return Value(a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w);

			return ArithmeticNode(*this, OperatorMultiply, a, b);
		}

		static float safeDiv(float a, float b)
		{ return b == 0 ? 0 : a / b; }

		Value ValueDiv(const Value& a, const Value& b) const
		{
			if (!a || !b)
				return Value();

			if (!a.NonZero())
				return 0.;

			if (b == 1)
				return a;

			if (allowShortcuts && a.IsFloat() && b.IsFloat())
				return Value(
					safeDiv(a.x, b.x),
					safeDiv(a.y, b.y),
					safeDiv(a.z, b.z),
					safeDiv(a.w, b.w)
				);

			return ArithmeticNode(*this, OperatorDivide, a, b);
		}

		static float safeMod(float a, float b)
		{
			return b == 0 ? 0 : fmod(a, b);
		}

		Value ValueMod(const Value& a, const Value& b) const
		{
			if (!a || !b)
				return Value();

			if (!a.NonZero() || !b.NonZero())
				return 0.;

			if (allowShortcuts && a.IsFloat() && b.IsFloat())
				return Value(
					safeMod(a.x, b.x),
					safeMod(a.y, b.y),
					safeMod(a.z, b.z),
					safeMod(a.w, b.w)
				);

			return ArithmeticNode(*this, OperatorMod, a, b);
		}

		Value ValueFloor(const Value& a) const
		{
			if (!a)
				return a;

			if (allowShortcuts && a.IsFloat())
				return Value(
					floor(a.x),
					floor(a.y),
					floor(a.z),
					floor(a.w)
				);

			return ArithmeticNode(*this, OperatorFloor, a);
		}

		Value ValueComponentAverage(const Value& a) const
		{
			if (!a)
				return a;

			if (allowShortcuts && a.IsFloat())
				return Value((a.x + a.y + a.z) / 3);

			return ArithmeticNode(*this, OperatorComponentAverage, a);
		}

		Value ValueAverage(const Value& a, const Value& b) const
		{
			if (!a || !b)
				return Value();

			if (allowShortcuts && a.IsFloat() && b.IsFloat())
				return Value(
					(a.x + b.x) * 0.5,
					(a.y + b.y) * 0.5,
					(a.z + b.z) * 0.5,
					(a.w + b.w) * 0.5
				);

			return ArithmeticNode(*this, OperatorAverage, a, b);
		}

		Value ValueNegate(const Value& a) const
		{
			return ValueSub(0., a);
		}

		Value ValueDot(Value a, Value b) const
		{
			if (!a.NonZeroXYZ() || !b.NonZeroXYZ())
				return 0.;

			if (allowShortcuts && a.IsFloat() && b.IsFloat())
				return Value(a.x * b.x + a.y * b.y + a.z * b.z);

			return ArithmeticNode(*this, OperatorDot, a, b);
		}

		Value ValueCombine(const Value& a, const Value& b) const
		{
			if (allowShortcuts && a.IsFloat() && b.IsFloat())
				return Value(a.x, b.x);

			return ArithmeticNode(*this, OperatorCombine, a, b);
		}
		Value ValueCombine(const Value& a, const Value& b, const Value& c) const
		{
			if (allowShortcuts && a.IsFloat() && b.IsFloat() && c.IsFloat())
				return Value(a.x, b.x, c.x);

			// ArithmeticNode(*this, OperatorCombine, a, b, c);
			// there is no 3-way combine yet
			return ValueAdd(
				ValueMul(a, Value(1, 0, 0)),
				ValueMul(ValueSelectY(b), Value(0, 1, 0)),
				ValueMul(ValueSelectZ(c), Value(0, 0, 1))
				);
		}

		// special helper
		Value ValueRotateXY(const Value& a, const Value& b) const;


		Value ValuePow(const Value& a, const Value& b) const
		{
			if (a == 0. || !a)
				return a;

			if (b == 1)
				return a;

			if (b == 0.)
				return 1;

			if (allowShortcuts && a.IsFloat() && b.IsFloat())
				return Value(pow(a.x,b.x), pow(a.y,b.y), pow(a.z,b.z), pow(a.w,b.w));

			return ArithmeticNode(*this, OperatorPow, a, b);
		}


		// unary
		Value ValueMagnitude(const Value& a) const
		{
			if (!a)
				return 0.;

			if (allowShortcuts && a.IsFloat())
				return sqrt(a.x*a.x + a.y*a.y + a.z*a.z);

			return ArithmeticNode(*this, OperatorLength, a);
		}

		Value ValueAbs(const Value& a) const
		{
			if (!a)
				return 0.;

			if (allowShortcuts && a.IsFloat())
				return Value(fabs(a.x), fabs(a.y), fabs(a.z), fabs(a.w));

			return ArithmeticNode(*this, OperatorAbs, a);
		}
		Value ValueNormalize(const Value& a) const
		{
			if (!a)
				return 0.;

			if (allowShortcuts && a.IsFloat())
			{
				auto m = sqrt(a.x*a.x + a.y*a.y + a.z*a.z);
				if (m > 0)
					m = 1.0f / m;
				return Value(a.x * m, a.y * m, a.z * m, a.w * m);
			}

			return ArithmeticNode(*this, OperatorNormalize, a);
		}

		Value ValueSin(const Value& a) const
		{
			if (allowShortcuts && a.IsFloat())
				return Value(sin(a.x), sin(a.y), sin(a.z), sin(a.w));

			return ArithmeticNode(*this, OperatorSin, a);
		}

		Value ValueCos(const Value& a) const
		{
			if (allowShortcuts && a.IsFloat())
				return Value(cos(a.x), cos(a.y), cos(a.z), cos(a.w));

			return ArithmeticNode(*this, OperatorCos, a);
		}

		Value ValueTan(const Value& a) const
		{
			if (allowShortcuts && a.IsFloat())
				return Value(tan(a.x), tan(a.y), tan(a.z), tan(a.w));

			return ArithmeticNode(*this, OperatorTan, a);
		}

		Value ValueArcSin(const Value& a) const
		{
			if (allowShortcuts && a.IsFloat())
				return Value(asin(a.x), asin(a.y), asin(a.z), asin(a.w));

			return ArithmeticNode(*this, OperatorArcSin, a);
		}


		Value ValueArcCos(const Value& a) const
		{
			if (allowShortcuts && a.IsFloat())
				return Value(acos(a.x), acos(a.y), acos(a.z), acos(a.w));

			return ArithmeticNode(*this, OperatorArcCos, a);
		}


		Value ValueArcTan(const Value& a, const Value& b) const
		{
			if (allowShortcuts && a.IsFloat() && b.IsFloat())
				return Value(atan2(a.x, b.x), atan2(a.y, b.y), atan2(a.z, b.z), atan2(a.w, b.w));
			return ArithmeticNode(*this, OperatorArcTan, a, b);
		}

		Value ValueSelectX(const Value& a) const
		{
			if (allowShortcuts && a.IsFloat())
				return Value(a.x);

			return ArithmeticNode(*this, OperatorSelectX, a);
		}

		Value ValueSelectY(const Value& a) const
		{
			if (allowShortcuts && a.IsFloat())
				return Value(a.y);

			return ArithmeticNode(*this, OperatorSelectY, a);
		}

		Value ValueSelectZ(const Value& a) const
		{
			if (allowShortcuts && a.IsFloat())
				return Value(a.z);

			return ArithmeticNode(*this, OperatorSelectZ, a);
		}

		// special lookup values
		Value ValueLookupN() const
		{
			return LookupNode(*this, LookupTypeNormal);
		}

		Value ValueLookupUV(int idx) const
		{
			switch (idx)
			{
			case 0: return LookupNode(*this, LookupTypeUV0);
			case 1: return LookupNode(*this, LookupTypeUV1);
			}

			return LookupNode(*this, LookupTypeUV0);
		}

		Value ValueLookupP() const
		{
			return LookupNode(*this, LookupTypePosition);
		}

		Value ValueLookupINVEC() const
		{
			return LookupNode(*this, LookupTypeIncident);
		}

		Value ValueLookupOUTVEC() const
		{
			return LookupNode(*this, LookupTypeOutVector);
		}

		Value ValueFresnel(const Value& ior, Value n = Value(), Value v = Value()) const
		{
			ValueNode node(*this, ValueTypeFresnel);

			node.SetValue("ior", ior);
			// TODO the following fail if set to expected lookup values
			node.SetValue("invec", v);
			node.SetValue("normal", n);
			return node;
		}

		Value ValueFresnelSchlick(const Value& reflectance, Value n = Value(), Value v = Value()) const
		{
			ValueNode node(*this, ValueTypeFresnelSchlick);

			node.SetValue("reflectance", reflectance);
			// TODO the following fail if set to expected lookup values
			node.SetValue("invec", v);
			node.SetValue("normal", n);
			return node;
		}

		// this performs a 3x3 matrix transform on a 3 component vector (EXPENSIVE!?!)
		Value ValueTransform(const Value& v, const Value& mx, const Value& my, const Value& mz)
		{
			auto x = mx.NonZero() ? ValueMul(ValueSelectX(v), mx) : 0.;
			auto y = my.NonZero() ? ValueMul(ValueSelectY(v), my) : 0.;
			auto z = mz.NonZero() ? ValueMul(ValueSelectZ(v), mz) : 0.;

			return ValueAdd(x, y, z);
		}

		Value ValueTransform(const Value& v, const Value& mx, const Value& my, const Value& mz, const Value& w)
		{
			auto x = mx.NonZero() ? ValueMul(ValueSelectX(v), mx) : 0.;
			auto y = my.NonZero() ? ValueMul(ValueSelectY(v), my) : 0.;
			auto z = mz.NonZero() ? ValueMul(ValueSelectZ(v), mz) : 0.;

			return ValueAdd(x, y, z, w);
		}

		Value ValueCross(const Value& a, const Value& b) const
		{
			if (!a.NonZero() || !b.NonZero())
				return 0.;

			return ArithmeticNode(*this, OperatorCross, a, b);
		}



#ifdef FRW_USE_MAX_TYPES
		Value ValueTransform(Value v, const Matrix3& tm)
		{
			if (tm.IsIdentity())
				return v;

				// shortcut scale
			if (tm[0][0] == tm[1][1] && tm[0][0] == tm[2][2] && tm[0][0] != 0)
				return ValueAdd(ValueMul(v, tm[0][0]), tm.GetRow(3));
			else
				return ValueTransform(v, tm.GetRow(0), tm.GetRow(1), tm.GetRow(2), tm.GetRow(3));
		}

#endif
		Value ValueMin(const Value& a, const Value& b)
		{
			if (allowShortcuts && a.IsFloat() && b.IsFloat())
				return Value(std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z), std::min(a.w, b.w));

			return ArithmeticNode(*this, OperatorMin, a, b);
		}

		Value ValueMax(const Value& a, const Value& b)
		{
			if (allowShortcuts && a.IsFloat() && b.IsFloat())
				return Value(std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z), std::max(a.w, b.w));

			return ArithmeticNode(*this, OperatorMax, a, b);
		}

		// clamp components between 0 and 1
		Value ValueClamp(const Value& v)
		{
			return ValueMin(ValueMax(v, 0.), 1.);
		}

		Shader ShaderBlend(const Shader& a, const Shader& b, const Value& t);
		Shader ShaderAdd(const Shader& a, const Shader& b);
	};

	class PostEffect : public Object
	{
		DECLARE_OBJECT_NO_DATA(PostEffect, Object);
	public:
		PostEffect(rpr_post_effect h, const Context& context, bool destroyOndelete = true) : Object(h, context, destroyOndelete, new Data()) {}

		PostEffect(const Context& context, PostEffectType type, bool destroyOndelete = true) : Object(nullptr, context, destroyOndelete, new Data())
		{
			rpr_post_effect h = nullptr;
			auto res = rprContextCreatePostEffect(context.Handle(), type, &h);
			checkStatus(res);
			m->Attach(h);
		}

		void SetParameter(const char * name, int value)
		{
			auto res = rprPostEffectSetParameter1u(Handle(), name, value);
			checkStatus(res);
		}

		void SetParameter(const char * name, float value)
		{
			auto res = rprPostEffectSetParameter1f(Handle(), name, value);
			checkStatus(res);
		}
	};

	class FrameBuffer : public Object
	{
		DECLARE_OBJECT_NO_DATA(FrameBuffer, Object);
	public:
		FrameBuffer(rpr_framebuffer h, const Context& context) : Object(h, context, true, new Data()) {}

		FrameBuffer(const Context& context, int width, int height, rpr_framebuffer_format format = { 4, RPR_COMPONENT_TYPE_FLOAT32 })
			: Object(nullptr, context, true, new Data())
		{
			FRW_PRINT_DEBUG("CreateFrameBuffer()");
			rpr_framebuffer h = nullptr;
			rpr_framebuffer_desc desc = { rpr_uint(width), rpr_uint(height) };
			auto res = rprContextCreateFrameBuffer(context.Handle(), format, &desc, &h);
			checkStatusThrow(res);
			m->Attach(h);
		}

		FrameBuffer(const Context& context, rpr_GLuint* glTextureId);

		void Resolve(FrameBuffer dest)
		{
#if (RPR_API_VERSION < 0x010031000)
			auto status = rprContextResolveFrameBuffer(GetContext().Handle(), Handle(), dest.Handle());
#else
			auto status = rprContextResolveFrameBuffer(GetContext().Handle(), Handle(), dest.Handle(), FALSE);
#endif
			checkStatusThrow(status, "Unable to resolve frame buffer");
		}

		void Clear()
		{
			auto status = rprFrameBufferClear(Handle());
			checkStatusThrow(status, "Unable to clear frame buffer");
		}
	};

	// this cannot be used for standard inputs!
	class Shader : public Node
	{
		DECLARE_OBJECT(Shader, Node);
		class Data : public Node::Data
		{
			DECLARE_OBJECT_DATA

			struct ShadowCatcherParams
			{
				float mShadowR = 0.0f;
				float mShadowG = 0.0f;
				float mShadowB = 0.0f;
				float mShadowA = 0.0f;
				float mShadowWeight = 1.0f;
				bool mBgIsEnv = false;
			};

		public:
			virtual ~Data()
			{
				if (material)
				{
					for(auto kvp : inputs)
						rprxMaterialDetachMaterial(context, kvp.second, kvp.first.c_str(), material);
					inputs.clear();

					FRW_PRINT_DEBUG("\tDeleting RPRX material 0x%016llX (from context 0x%016llX)", material, context);
					auto res = rprxMaterialDelete(context, material);
					checkStatus(res);
					FRW_PRINT_DEBUG("\tDeleted RPRX material 0x%016llX", material);
				}
			}

			virtual bool IsValid() const
			{
				if (Handle() != nullptr) return true;
				if (material != nullptr) return true;
				return false;
			}

			bool bDirty = true;
			int numAttachedShapes = 0;
			ShaderType shaderType = ShaderTypeInvalid;
			rprx_context context = nullptr;
			rprx_material material = nullptr; // RPRX material
			std::map<std::string, rpr_material_node> inputs;
			bool isShadowCatcher = false;
			ShadowCatcherParams mShadowCatcherParams;
		};

	public:
		void SetShadowCatcher(bool isShadowCatcher) { data().isShadowCatcher = isShadowCatcher; }
		bool IsShadowCatcher() const { return data().isShadowCatcher; }
		void SetShadowColor(float r, float g, float b, float a)
		{
			data().mShadowCatcherParams.mShadowR = r;
			data().mShadowCatcherParams.mShadowG = g;
			data().mShadowCatcherParams.mShadowB = b;
			data().mShadowCatcherParams.mShadowA = a;
		}
		void GetShadowColor(float *r, float *g, float *b, float *a) const
		{
			*r = data().mShadowCatcherParams.mShadowR;
			*g = data().mShadowCatcherParams.mShadowG;
			*b = data().mShadowCatcherParams.mShadowB;
			*a = data().mShadowCatcherParams.mShadowA;
		}

		void SetShadowWeight(float w) { data().mShadowCatcherParams.mShadowWeight = w; }
		float GetShadowWeight() const { return data().mShadowCatcherParams.mShadowWeight; }

		void SetBackgroundIsEnvironment(bool bgIsEnv) { data().mShadowCatcherParams.mBgIsEnv = bgIsEnv; }
		bool BgIsEnv() const { return data().mShadowCatcherParams.mBgIsEnv; }

		Shader(DataPtr p)
		{
			m = p;
		}
		explicit Shader(const MaterialSystem& ms, ShaderType type, bool destroyOnDelete = true)
		: Node(ms, type, destroyOnDelete, new Data())
		{
			data().shaderType = type;
		}
		explicit Shader(const MaterialSystem& ms, const Context& context, rprx_material_type type)
		: Node(context, new Data())
		{
			Data& d = data();
			rprx_context rprxContext = ms.GetRprxContext();
			d.context = rprxContext;
			auto status = rprxCreateMaterial(rprxContext, type, &d.material);
			checkStatusThrow(status, "Unable to create rprx material");
			d.shaderType = ShaderTypeRprx;
			d.bDirty = false;
			FRW_PRINT_DEBUG("\tCreated RPRX material 0x%016llX of type: 0x%X", d.material, type);
		}

		ShaderType GetShaderType() const
		{
			if (!IsValid())
				return ShaderTypeInvalid;
			return data().shaderType;
		}

		bool IsRprxMaterial() const
		{
			return data().material != nullptr;
		}

		void SetDirty()
		{
			data().bDirty = true;
		}

		bool IsDirty() const
		{
			return data().bDirty;
		}

		void xMaterialCommit() const
		{
			Data&d = data();

			if (d.material)
			{
				try
				{
					rpr_int res = rprxMaterialCommit(d.context, d.material);
					checkStatus(res);
				}
				catch (...)
				{
					DebugPrint("Failed to rprx commit: Context=0x%016llX x_material=0x%016llX", d.context, d.material);

					throw;
				}
			}
		}

		void AttachToShape(Shape::Data& shape)
		{
			Data& d = data();
			d.numAttachedShapes++;
			rpr_int res;
			if (d.material)
			{
				FRW_PRINT_DEBUG("\tShape.AttachMaterial: d: 0x%016llX - numAttachedShapes: %d shape=0x%016llX x_material=0x%016llX", &d, d.numAttachedShapes, shape.Handle(), d.material);
				res = rprxShapeAttachMaterial(d.context, shape.Handle(), d.material);
				checkStatus(res);

				if (d.isShadowCatcher)
				{
					res = rprShapeSetShadowCatcher(shape.Handle(), true);
					checkStatus(res);
				}
			}
			else
			{
				FRW_PRINT_DEBUG("\tShape.AttachMaterial: d: 0x%016llX - numAttachedShapes: %d shape=0x%016llX material=0x%016llX", d, d.numAttachedShapes, shape.Handle(), d.Handle());
				res = rprShapeSetMaterial(shape.Handle(), d.Handle());
				checkStatus(res);
			}
		}
		void DetachFromShape(Shape::Data& shape)
		{
			Data& d = data();
			d.numAttachedShapes--;
			FRW_PRINT_DEBUG("\tShape.DetachMaterial: d: 0x%016llX - numAttachedShapes: %d shape=0x%016llX, material=0x%016llX", &d, d.numAttachedShapes, shape.Handle(), d.material);
			if (d.material)
			{
				rpr_int res = rprxShapeDetachMaterial(d.context, shape.Handle(), d.material);
				checkStatus(res);

				if (d.isShadowCatcher)
				{
					res = rprShapeSetShadowCatcher(shape.Handle(), false);
					checkStatus(res);
				}
			}
//			else -- RPR 1.262 crashes when changing any RPRX parameter (when creating new material and replacing old one); mirroring call to rpr API fixes that
			{
				rpr_int res = rprShapeSetMaterial(shape.Handle(), nullptr);
				checkStatus(res);
			}
		}

		void Commit()
		{
			Data& d = data();
			rpr_int res;

			if (!d.context || !d.material)
				return;

			res = rprxMaterialCommit(d.context, d.material);
			checkStatus(res);
		}

		void AttachToMaterialInput(rpr_material_node node, const char* inputName) const
		{
			auto& d = data();
			rpr_int res;
			FRW_PRINT_DEBUG("\tShape.AttachToMaterialInput: node=0x%016llX, material=0x%016llX on %s", node, d.material, inputName);
			if (d.material)
			{
				// attach rprx shader output to some material's input
				auto it = d.inputs.find(inputName);
				if (it != d.inputs.end())
					DetachFromMaterialInput(it->second, it->first.c_str());

				res = rprxMaterialAttachMaterial(d.context, node, inputName, d.material);
				checkStatus(res);
				d.inputs.emplace(inputName, node);
			}
			else
			{
				res = rprMaterialNodeSetInputN(node, inputName, d.Handle());
			}
			checkStatus(res);
		}

		void DetachFromMaterialInput(rpr_material_node node, const char* inputName) const
		{
			auto& d = data();
			rpr_int res = RPR_ERROR_INVALID_PARAMETER;
			FRW_PRINT_DEBUG("\tShape.DetachFromMaterialInput: node=0x%016llX, material=0x%016llX on %s", node, d.material, inputName);
			if (d.material)
			{
				// detach rprx shader output to some material's input
				res = rprxMaterialDetachMaterial(d.context, node, inputName, d.material);
				d.inputs.erase(inputName);
			}
			checkStatus(res);
		}

		void xSetParameterN(rprx_parameter parameter, rpr_material_node node)
		{
			const Data& d = data();
			rpr_int res = rprxMaterialSetParameterN(d.context, d.material, parameter, node);
			checkStatus(res);
		}
		void xSetParameterU(rprx_parameter parameter, rpr_uint value)
		{
			const Data& d = data();
			rpr_int res = rprxMaterialSetParameterU(d.context, d.material, parameter, value);
			checkStatus(res);
		}
		void xSetParameterF(rprx_parameter parameter, rpr_float x, rpr_float y, rpr_float z, rpr_float w)
		{
			const Data& d = data();
			rpr_int res = rprxMaterialSetParameterF(d.context, d.material, parameter, x, y, z, w);
			checkStatus(res);
		}

		bool xSetValue(rprx_parameter parameter, const Value& v)
		{
			switch (v.type)
			{
				case Value::FLOAT:
					xSetParameterF(parameter, v.x, v.y, v.z, v.w);
					return true;
				case Value::NODE:
				{
					if (!v.node)	// in theory we should now allow this, as setting a NULL input is legal (as of FRSDK 1.87)
						return false;
					AddReference(v.node);
					xSetParameterN(parameter, v.node.Handle());	// should be ok to set null here now
					return true;
				}
			}
			assert(!"bad type");
			return false;
		}
	};

	class DiffuseShader : public Shader
	{
	public:
		explicit DiffuseShader(const MaterialSystem& h) : Shader(h, ShaderTypeDiffuse) {}

		void SetColor(Value value) { SetValue("color", value); }
		void SetNormal(Value value) { SetValue("normal", value); }
		void SetRoughness(Value value) { SetValue("roughness", value); }
	};

	class EmissiveShader : public Shader
	{
	public:
		explicit EmissiveShader(MaterialSystem& h) : Shader(h, ShaderTypeEmissive) {}

		void SetColor(Value value) { SetValue("color", value); }
	};

	class TransparentShader : public Shader
	{
	public:
		explicit TransparentShader(const MaterialSystem& h) : Shader(h, ShaderTypeTransparent) {}

		void SetColor(Value value) { SetValue("color", value); }
	};

	// inline definitions
	static int allocatedObjects = 0;

	inline void Object::Data::Init(void* h, const Context& c, bool destroy)
	{
		handle = h;
		context = c.m;
		destroyOnDelete = destroy;
		if (h)
		{
			allocatedObjects++;
#if FRW_LOGGING
			typeNameMirror = GetTypeName();
#endif
			FRW_PRINT_DEBUG("\tFR+ %s 0x%016llX%s (%d total)", GetTypeName(), h, destroyOnDelete ? "" : "*", allocatedObjects);
		}
	}

	inline Object::Data::~Data()
	{
		if (handle)
		{
			// Clear references before destroying the object itself. For example, Scene has referenced Shapes,
			// and these shapes better to be destroyed before the scene itself.
			references.clear();

			if (destroyOnDelete)
				rprObjectDelete(handle);

			allocatedObjects--;

			// Can't use virtual GetTypeName() in destructor, so using "mirrored" type name
			FRW_PRINT_DEBUG("\tFR- %s 0x%016llX%s (%d total)", typeNameMirror, handle, destroyOnDelete ? "" : "*", allocatedObjects);
		}
	}

	inline void Object::Data::Attach(void* h, bool destroy)
	{
		if (handle)
		{
			if (destroyOnDelete)
				rprObjectDelete(handle);

			allocatedObjects--;

			FRW_PRINT_DEBUG("\tFR- %s 0x%016llX%s (%d total)", GetTypeName(), handle, destroyOnDelete ? "" : "*", allocatedObjects);
		}

		if (h)
		{
			destroyOnDelete = destroy;
			handle = h;
			allocatedObjects++;

#if FRW_LOGGING
			typeNameMirror = GetTypeName();
#endif
			FRW_PRINT_DEBUG("\tFR+ %s 0x%016llX%s (%d total)", GetTypeName(), h, destroyOnDelete ? "" : "*", allocatedObjects);
		}
	}

	inline Context Object::GetContext() const
	{
		return Context(m->context);
	}

	inline Node::Node(const MaterialSystem& ms, int type, bool destroyOnDelete, Node::Data* _data)
		: Object(ms.CreateNode(type), ms.GetContext(), destroyOnDelete, _data ? _data : new Node::Data())
	{
		data().materialSystem = ms;
		data().type = type;
	}

	inline void Node::_SetInputNode(const char* key, const Shader& shader)
	{
		if (shader)
		{
			AddReference(shader);
			shader.AttachToMaterialInput(Handle(), key);
		}
	}

	inline bool Node::SetValue(const char* key, const Value& v)
	{
		switch (v.type)
		{
			case Value::FLOAT:
				return RPR_SUCCESS == rprMaterialNodeSetInputF(Handle(), key, v.x, v.y, v.z, v.w);
			case Value::NODE:
			{
				if (!v.node)	// in theory we should now allow this, as setting a NULL input is legal (as of FRSDK 1.87)
					return false;
				AddReference(v.node);
				return RPR_SUCCESS == rprMaterialNodeSetInputN(Handle(), key, v.node.Handle());	// should be ok to set null here now
			}
		}
		assert(!"bad type");
		return false;
	}

	inline bool Node::SetValueInt(const char* key, int v)
	{
		return RPR_SUCCESS == rprMaterialNodeSetInputU(Handle(), key, v);
	}

	inline MaterialSystem Node::GetMaterialSystem() const
	{
		return data().materialSystem.As<MaterialSystem>();
	}


	inline Shader MaterialSystem::ShaderBlend(const Shader& a, const Shader& b, const Value& t)
	{
		// in theory a null could be considered a transparent material, but would create unexpected node
		if (!a)
		{
			b.xMaterialCommit();
			return b;
		}

		if (!b)
		{
			a.xMaterialCommit();
			return a;
		}

		Shader node(*this, ShaderTypeBlend);
		node._SetInputNode("color0", a);
		node._SetInputNode("color1", b);

		node.SetValue("weight", t);

		node.xMaterialCommit();

		a.xMaterialCommit();
		b.xMaterialCommit();

		return node;
	}

	inline Shader MaterialSystem::ShaderAdd(const Shader& a, const Shader& b)
	{
		if (!a)
			return b;
		if (!b)
			return a;

		Shader node(*this, ShaderTypeAdd);
		node._SetInputNode("color0", a);
		node._SetInputNode("color1", b);
		return node;
	}

	inline Value Value::SelectX() const
	{
		auto ms = GetMaterialSystem();
		return ms.ValueSelectX(*this);
	}
	inline Value Value::SelectY() const
	{
		auto ms = GetMaterialSystem();
		return ms.ValueSelectY(*this);
	}
	inline Value Value::SelectZ() const
	{
		auto ms = GetMaterialSystem();
		return ms.ValueSelectZ(*this);
	}

	inline MaterialSystem Value::GetMaterialSystem() const
	{
		if (IsNode())
			return node.GetMaterialSystem();
		return MaterialSystem();
	}

	inline Shape::Data::~Data()
	{
		Shader sh = shader.As<Shader>();
		if (sh.IsValid())
		{
			sh.DetachFromShape(*this);
		}
	}

	inline void Shape::SetShader(Shader shader)
	{
		if (Shader old = GetShader())
		{
			if (old == shader)	// no change?
				return;

			RemoveReference(old);
			old.DetachFromShape(data());
		}

		AddReference(shader);
		data().shader = shader;
		shader.AttachToShape(data());
	}

	inline Shader Shape::GetShader() const
	{
		return data().shader.As<Shader>();
	}

	inline void Shape::SetVolumeShader(const frw::Shader& shader)
	{
		if( auto old = GetVolumeShader() )
		{
			if( old == shader )	// no change?
				return;
			RemoveReference( old );
		}

		AddReference(shader);
		data().volumeShader = shader;

		if (!shader.IsRprxMaterial())
		{
			auto res = rprShapeSetVolumeMaterial(Handle(), shader.Handle());
			checkStatus(res);
		}
	}

	inline Shader Shape::GetVolumeShader() const
	{
		return data().volumeShader.As<Shader>();
	}

	inline Shape Shape::CreateInstance(Context context) const
	{
		FRW_PRINT_DEBUG("CreateInstance()");
		rpr_shape h = nullptr;
		auto res = rprContextCreateInstance(context.Handle(), Handle(), &h);
		checkStatus(res);
		Shape shape(h, context);
		shape.AddReference(*this);
		return shape;
	}

	inline void Shape::SetDisplacement(Value v, float minscale, float maxscale)
	{
		RemoveDisplacement();
		if (v.IsNode())
		{
			Node n = v.GetNode();
			data().displacementShader = n;
			auto res = rprShapeSetDisplacementMaterial(Handle(), n.Handle());
			checkStatus(res);

			if (minscale > maxscale)
			{
				// Create the command to show the error dialog
				MString command = "confirmDialog -title \"Radeon ProRender Error\" -button \"OK\" -message \" Invalid parameters passed to displacement material! \"";

				// Show the error dialog
				MGlobal::executeCommandOnIdle(command);

				// render with no displacement
				res = rprShapeSetDisplacementScale(Handle(), maxscale, maxscale);
			}
			else
			{
				res = rprShapeSetDisplacementScale(Handle(), minscale, maxscale);
			}
			checkStatus(res);
		}
	}

	inline Node Shape::GetDisplacementMap()
	{
		return data().displacementShader.As<Node>();
	}

	inline void Shape::RemoveDisplacement()
	{
		if (data().displacementShader)
		{
			rpr_int res = rprShapeSetDisplacementMaterial(Handle(), nullptr);
			checkStatus(res);
			res = rprShapeSetSubdivisionFactor(Handle(), 0);
			checkStatus(res);
			data().displacementShader = nullptr;
		}
	}

	inline void Shape::SetSubdivisionFactor(int sub)
	{

		size_t faceCount = GetFaceCount();

		while (sub > 1 && (faceCount * sub * sub) > MaximumSubdividedFacecount)
			sub--;

		auto res = rprShapeSetSubdivisionFactor(Handle(), sub);
		checkStatus(res);
	}

	inline void Shape::SetSubdivisionCreaseWeight(float weight)
	{
		auto res = rprShapeSetSubdivisionCreaseWeight(Handle(), weight);
		checkStatus(res);
	}

	inline void Shape::SetSubdivisionBoundaryInterop(rpr_subdiv_boundary_interfop_type type)
	{
		auto res = rprShapeSetSubdivisionBoundaryInterop(Handle(), type);
		checkStatus(res);
	}

	inline Image::Image(Context context, float r, float g, float b)
		: Object(nullptr, context, true, new Data())
	{
		FRW_PRINT_DEBUG("CreateImage()");

		rpr_image_format format = { 3,RPR_COMPONENT_TYPE_FLOAT32 };
		rpr_image_desc image_desc = { 1,1,1,12,12 };
		float data[] = { r, g, b };

		rpr_image h = nullptr;
		auto res = rprContextCreateImage(context.Handle(), format, &image_desc, data, &h);
		if (checkStatus(res, "Unable to create image"))
			m->Attach(h);
	}

	inline Image::Image(Context context, const rpr_image_format& format, const rpr_image_desc& image_desc, const void* data)
		: Object(nullptr, context, true, new Data())
	{
		FRW_PRINT_DEBUG("CreateImage()");
		rpr_image h = nullptr;
		auto res = rprContextCreateImage(context.Handle(), format, &image_desc, data, &h);
		if (checkStatus(res, "Unable to create image"))
			m->Attach(h);
	}

	inline Image::Image(Context context, const char* filename)
		: Object(nullptr, context, true, new Data())
	{
		rpr_image h = nullptr;
		auto res = rprContextCreateImageFromFile(context.Handle(), filename, &h);
		if (ErrorUnsupportedImageFormat != res) {
			//^ we don't want to give Error messages to the user about unsupported image formats, since we support them through the plugin.
			if (checkStatus(res, "Unable to load: " + MString(filename)))
				m->Attach(h);
		}
	}

	inline void EnvironmentLight::SetImage(Image img)
	{
		auto res = rprEnvironmentLightSetImage(Handle(), img.Handle());
		checkStatus(res);
		data().image = img;
	}

	inline void EnvironmentLight::AttachPortal(Shape shape)
	{
		auto res = rprEnvironmentLightAttachPortal(GetContext().GetScene(), Handle(), shape.Handle());
		checkStatus(res);
		AddReference(shape);
	}

	inline void EnvironmentLight::DetachPortal(Shape shape)
	{
		auto res = rprEnvironmentLightDetachPortal(GetContext().GetScene(), Handle(), shape.Handle());
		checkStatus(res);
		AddReference(shape);
	}

	inline void Scene::SetActive()
	{
		if (auto context = GetContext())
			context.SetScene(*this);
	}

	inline void Context::Attach(PostEffect post_effect)
	{
		FRW_PRINT_DEBUG("AttachPostEffect(%p)\n", post_effect.Handle());
		auto res = rprContextAttachPostEffect(Handle(), post_effect.Handle());
		checkStatus(res);
	}

	inline void Context::Detach(PostEffect post_effect)
	{
		FRW_PRINT_DEBUG("DetachPostEffect(%p)\n", post_effect.Handle());
		auto res = rprContextDetachPostEffect(Handle(), post_effect.Handle());
		checkStatus(res);
	}

	inline Shape Context::CreateMesh(const rpr_float* vertices, size_t num_vertices, rpr_int vertex_stride,
		const rpr_float* normals, size_t num_normals, rpr_int normal_stride,
		const rpr_float* texcoords, size_t num_texcoords, rpr_int texcoord_stride,
		const rpr_int*   vertex_indices, rpr_int vidx_stride,
		const rpr_int*   normal_indices, rpr_int nidx_stride,
		const rpr_int*   texcoord_indices, rpr_int tidx_stride,
		const rpr_int*   num_face_vertices, size_t num_faces)
	{
		FRW_PRINT_DEBUG("CreateMesh() - %d faces\n", num_faces);
		rpr_shape shape = nullptr;

		auto status = rprContextCreateMesh(Handle(),
			vertices, num_vertices, vertex_stride,
			normals, num_normals, normal_stride,
			texcoords, num_texcoords, texcoord_stride,
			vertex_indices, vidx_stride,
			normal_indices, nidx_stride,
			texcoord_indices, tidx_stride,
			num_face_vertices, num_faces,
			&shape);

		checkStatusThrow(status, "Unable to create mesh");

		Shape shapeObj(shape, *this);

		shapeObj.SetUVCoordinatesSetFlag(texcoords != nullptr && num_texcoords > 0);

		return shapeObj;
	}

	inline Shape Context::CreateMeshEx(const rpr_float* vertices, size_t num_vertices, rpr_int vertex_stride,
		const rpr_float* normals, size_t num_normals, rpr_int normal_stride,
		const rpr_int* perVertexFlag, size_t num_perVertexFlags, rpr_int perVertexFlag_stride,
		rpr_int numberOfTexCoordLayers, const rpr_float** texcoords, const size_t* num_texcoords, const rpr_int* texcoord_stride,
		const rpr_int* vertex_indices, rpr_int vidx_stride,
		const rpr_int* normal_indices, rpr_int nidx_stride, const rpr_int** texcoord_indices,
		const rpr_int* tidx_stride, const rpr_int * num_face_vertices, size_t num_faces)
	{
		FRW_PRINT_DEBUG("CreateMesh() - %d faces\n", num_faces);
		rpr_shape shape = nullptr;

		auto status = rprContextCreateMeshEx(Handle(),
			vertices, num_vertices, vertex_stride,
			normals, num_normals, normal_stride,
			perVertexFlag, num_perVertexFlags, perVertexFlag_stride,
			numberOfTexCoordLayers, texcoords, num_texcoords, texcoord_stride,
			vertex_indices, vidx_stride,
			normal_indices, nidx_stride, texcoord_indices,
			tidx_stride, num_face_vertices, num_faces,
			&shape);

		checkStatusThrow(status, "Unable to create mesh");

		Shape shapeObj (shape, *this);
		shapeObj.SetUVCoordinatesSetFlag(numberOfTexCoordLayers > 0);

		return shapeObj;
	}

	inline void Context::SetAOV(FrameBuffer frameBuffer, rpr_aov aov)
	{
		auto res = rprContextSetAOV(Handle(), aov, frameBuffer.Handle());
		checkStatus(res);
	}

	// convenience operators
	inline Value operator+(const Value& a, const Value& b)
	{
		auto ms = a.GetMaterialSystem();
		if (!ms)
			ms = b.GetMaterialSystem();
		return ms.ValueAdd(a, b);
	}
	inline Value operator*(const Value& a, const Value& b)
	{
		auto ms = a.GetMaterialSystem();
		if (!ms)
			ms = b.GetMaterialSystem();
		return ms.ValueMul(a, b);
	}
	inline Value operator/(const Value& a, const Value& b)
	{
		auto ms = a.GetMaterialSystem();
		if (!ms)
			ms = b.GetMaterialSystem();
		return ms.ValueDiv(a, b);
	}
	inline Value operator-(const Value& a, const Value& b)
	{
		auto ms = a.GetMaterialSystem();
		if (!ms)
			ms = b.GetMaterialSystem();
		return ms.ValueSub(a, b);
	}
	inline Value operator-(const Value& a)
	{
		auto ms = a.GetMaterialSystem();
		return ms.ValueNegate(a);
	}
	inline Value operator^(const Value& a, const Value& b)
	{
		auto ms = a.GetMaterialSystem();
		if (!ms)
			ms = b.GetMaterialSystem();
		return ms.ValuePow(a, b);
	}
	inline Value operator&(const Value& a, const Value& b)
	{
		auto ms = a.GetMaterialSystem();
		if (!ms)
			ms = b.GetMaterialSystem();
		return ms.ValueDot(a, b);
	}

	inline Value MaterialSystem::ValueRotateXY(const Value& a, const Value& b) const
	{
		if (allowShortcuts && a.IsFloat() && b.IsFloat())
			return Value(a.x * cos(b.x) - a.y * sin(b.x), a.x * sin(b.x) + a.y * cos(b.x), 0);

		auto sinb = ValueSin(b);
		auto cosb = ValueCos(b);

		auto x = ValueSelectX(a);
		auto y = ValueSelectY(a);

		return ValueCombine(x * cosb - y * sinb, x * sinb + y * cosb);
	}

}
