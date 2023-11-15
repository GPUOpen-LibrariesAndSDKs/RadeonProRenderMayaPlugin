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

// Switched from inc_dynamic to inc version of RadeonProRender.h so 
// RprSupport and RadeonProRender_GL needs to be included manually
#include <RadeonProRender.h>
#include <RadeonProRender_GL.h>

#include <list>
#include <map>
#include <vector>
#include <cassert>
#ifndef OSMac_
#include <memory>
#endif
#include <set>
#include <string>
#include <array>
#include <maya/MString.h>

#include <math.h>

#include "Logger.h"
#include "FireRenderError.h"

#include <maya/MGlobal.h>
#include <maya/MDistance.h>
#include <maya/MColor.h>
#include "FireRenderMath.h"
#include "ProRenderGLTF.h"
#include "RprLoadStore.h"

//#define FRW_LOGGING 1

#define RPR_AOV_MAX 0x41

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
		OperatorSelectW = RPR_MATERIAL_NODE_OP_SELECT_W,
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

	enum RampInterpolationMode
	{
		InterpolationModeNone = RPR_INTERPOLATION_MODE_NONE,
		InterpolationModeLinear = RPR_INTERPOLATION_MODE_LINEAR
	};

	const static int MATERIAL_NODE_LOOKUP_VERTEX_COLOR = 0x100;

	enum LookupType
	{
		LookupTypeUV0 = RPR_MATERIAL_NODE_LOOKUP_UV,
		LookupTypeNormal = RPR_MATERIAL_NODE_LOOKUP_N,
		LookupTypePosition = RPR_MATERIAL_NODE_LOOKUP_P,
		LookupTypeIncident = RPR_MATERIAL_NODE_LOOKUP_INVEC,
		LookupTypeOutVector = RPR_MATERIAL_NODE_LOOKUP_OUTVEC,
		LookupTypeUV1 = RPR_MATERIAL_NODE_LOOKUP_UV1,
		LookupTypePositionLocal = RPR_MATERIAL_NODE_LOOKUP_P_LOCAL,
		LookupTypeVertexColor = MATERIAL_NODE_LOOKUP_VERTEX_COLOR,
		LookupTypeVertexValue0 = RPR_MATERIAL_NODE_LOOKUP_VERTEX_VALUE0,
		LookupTypeVertexValue1 = RPR_MATERIAL_NODE_LOOKUP_VERTEX_VALUE1,
		LookupTypeVertexValue2 = RPR_MATERIAL_NODE_LOOKUP_VERTEX_VALUE2,
		LookupTypeVertexValue3 = RPR_MATERIAL_NODE_LOOKUP_VERTEX_VALUE3,
		LookupTypeShapeRandomColor = RPR_MATERIAL_NODE_LOOKUP_SHAPE_RANDOM_COLOR,
		LookupTypeObjectID = RPR_MATERIAL_NODE_LOOKUP_OBJECT_ID,
		LookupTypePrimitiveRandomColor = RPR_MATERIAL_NODE_LOOKUP_PRIMITIVE_RANDOM_COLOR
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
		ValueTypeVoronoiMap = RPR_MATERIAL_NODE_VORONOI_TEXTURE, //did not get color input
		ValueTypeConstant = RPR_MATERIAL_NODE_CONSTANT_TEXTURE,
		ValueTypeLookup = RPR_MATERIAL_NODE_INPUT_LOOKUP,
		ValueTypeBlend = RPR_MATERIAL_NODE_BLEND_VALUE,
		ValueTypeFresnelSchlick = RPR_MATERIAL_NODE_FRESNEL_SCHLICK,
		ValueTypePassthrough = RPR_MATERIAL_NODE_PASSTHROUGH, // legacy, substituted with flat color shader
        ValueTypeAOMap = RPR_MATERIAL_NODE_AO_MAP,
		ValueTypeUVProcedural = RPR_MATERIAL_NODE_UV_PROCEDURAL,
		ValueTypeUVTriplanar = RPR_MATERIAL_NODE_UV_TRIPLANAR,
		ValueTypeBufferSampler = RPR_MATERIAL_NODE_BUFFER_SAMPLER, // buffer node
		ValueTypeHSVToRGB = RPR_MATERIAL_NODE_HSV_TO_RGB,
		ValueTypeRRGToHSV = RPR_MATERIAL_NODE_RGB_TO_HSV,
		ValueTypeToonRamp = RPR_MATERIAL_NODE_TOON_RAMP,
		ValueTypeGridSampler = RPR_MATERIAL_NODE_GRID_SAMPLER,
		ValueTypePrimvarLookup = RPR_MATERIAL_NODE_PRIMVAR_LOOKUP,
		ValueTypeRamp = RPR_MATERIAL_NODE_RAMP,
		ValueTypeBlackBody = RPR_MATERIAL_NODE_BLACKBODY,
		ValueTypeBevel = RPR_MATERIAL_NODE_ROUNDED_CORNER,
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
		ShaderTypeBlend = RPR_MATERIAL_NODE_BLEND,
		ShaderTypeStandard = RPR_MATERIAL_NODE_UBERV2,
		ShaderTypeOrenNayer = RPR_MATERIAL_NODE_ORENNAYAR,
		ShaderTypeDiffuseRefraction = RPR_MATERIAL_NODE_DIFFUSE_REFRACTION,
		ShaderTypeAdd = RPR_MATERIAL_NODE_ADD,
		ShaderTypeVolume = RPR_MATERIAL_NODE_VOLUME,
		ShaderTypeFlatColor = RPR_MATERIAL_NODE_PASSTHROUGH,
		ShaderTypeToon = RPR_MATERIAL_NODE_TOON_CLOSURE,
		ShaderTypeDoubleSided = RPR_MATERIAL_NODE_TWOSIDED,
		ShaderTypeMicrofacetAnisotropicReflection = RPR_MATERIAL_NODE_MICROFACET_ANISOTROPIC_REFLECTION,
		ShaderTypeMaterialX = RPR_MATERIAL_NODE_MATX
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
		ContextParameterFilterType = RPR_CONTEXT_IMAGE_FILTER_TYPE,
		ContextParameterFilterRadius = RPR_CONTEXT_IMAGE_FILTER_RADIUS,
		ContextParameterToneMappingType = RPR_CONTEXT_TONE_MAPPING_TYPE,
		ContextParameterToneMappingLinearScale = RPR_CONTEXT_TONE_MAPPING_LINEAR_SCALE,
		ContextParameterToneMappingPhotolinearSensitivity = RPR_CONTEXT_TONE_MAPPING_PHOTO_LINEAR_SENSITIVITY,
		ContextParameterToneMappingPhotolinearExposure = RPR_CONTEXT_TONE_MAPPING_PHOTO_LINEAR_EXPOSURE,
		ContextParameterToneMappingPhotolinearFStop = RPR_CONTEXT_TONE_MAPPING_PHOTO_LINEAR_FSTOP,
		ContextParameterToneMappingReinhard2Prescale = RPR_CONTEXT_TONE_MAPPING_REINHARD02_PRE_SCALE,
		ContextParameterToneMappingReinhard2Postscale = RPR_CONTEXT_TONE_MAPPING_REINHARD02_POST_SCALE,
		ContextParameterToneMappingReinhard2Burn = RPR_CONTEXT_TONE_MAPPING_REINHARD02_BURN,
		ContextParameterMaxRecursion = RPR_CONTEXT_MAX_RECURSION,
		ContextParameterRayCastEpsilon = RPR_CONTEXT_RAY_CAST_EPSILON,
		ContextParameterRadienceClamp = RPR_CONTEXT_RADIANCE_CLAMP,
		ContextParameterXFlip = RPR_CONTEXT_X_FLIP,
		ContextParameterYFlip = RPR_CONTEXT_Y_FLIP,
		ContextParameterTextureGamma = RPR_CONTEXT_TEXTURE_GAMMA,
		ContextParameterPdfThreshold = RPR_CONTEXT_PDF_THRESHOLD,
		ContextParameterRenderMode = RPR_CONTEXT_RENDER_MODE,
		ContextParameterRoughnessCap = RPR_CONTEXT_ROUGHNESS_CAP,
		ContextParameterDisplayGamma = RPR_CONTEXT_DISPLAY_GAMMA,
		ContextParameterMaterialStackSize = RPR_CONTEXT_MATERIAL_STACK_SIZE,
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
#if RPR_VERSION_MAJOR_MINOR_REVISION < 0x00103402 
		ParameterInfoName = RPR_PARAMETER_NAME_STRING,
#endif
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

		NodeInputStandardDiffuseColor = RPR_MATERIAL_INPUT_UBER_DIFFUSE_COLOR,
		NodeInputStandardDiffuseNormal = RPR_MATERIAL_INPUT_UBER_DIFFUSE_NORMAL,
		NodeInputStandardGlossyColor = RPR_MATERIAL_INPUT_UBER_SHEEN,
		NodeInputStandardClearcoatColor = RPR_MATERIAL_INPUT_UBER_COATING_COLOR,
		NodeInputStandardClearcoatNormal = RPR_MATERIAL_INPUT_UBER_COATING_NORMAL,
		NodeInputStandardRefractionColor = RPR_MATERIAL_INPUT_UBER_REFRACTION_COLOR,
		NodeInputStandardRefractionNormal = RPR_MATERIAL_INPUT_UBER_REFRACTION_NORMAL,
		NodeInputStandardRefractionIOR = RPR_MATERIAL_INPUT_UBER_REFRACTION_IOR,
		NodeInputStandardDiffuseToRefractionWeight = RPR_MATERIAL_INPUT_UBER_REFRACTION_WEIGHT,
		NodeInputStandardGlossyToDiffuseWeight = RPR_MATERIAL_INPUT_UBER_COATING_WEIGHT,
		NodeInputStandardTransparency = RPR_MATERIAL_INPUT_UBER_TRANSPARENCY,
		NodeInputStandardRefractionRoughness = RPR_MATERIAL_INPUT_UBER_REFRACTION_ROUGHNESS,
		NodeInputFresnelSchlikApproximation = RPR_MATERIAL_INPUT_UBER_FRESNEL_SCHLICK_APPROXIMATION,

		NodeInputUVType = RPR_MATERIAL_INPUT_UV_TYPE,
		NodeInputOffsset = RPR_MATERIAL_INPUT_OFFSET,
		NodeInputZAxis = RPR_MATERIAL_INPUT_ZAXIS,
		NodeInputXAxis = RPR_MATERIAL_INPUT_XAXIS,
		NodeInputOrigin = RPR_MATERIAL_INPUT_ORIGIN,
		NodeInputThreshold = RPR_MATERIAL_INPUT_THRESHOLD
	};
	enum NodeInputInfo
	{
		NodeInputInfoId = RPR_MATERIAL_NODE_INPUT_NAME,
#if RPR_VERSION_MAJOR_MINOR_REVISION < 0x00103305 
		NodeInputInfoName = RPR_MATERIAL_NODE_INPUT_NAME_STRING,
#endif
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
		EnvironmentOverrideReflection = RPR_ENVIRONMENT_LIGHT_OVERRIDE_REFLECTION,
		EnvironmentOverrideRefraction = RPR_ENVIRONMENT_LIGHT_OVERRIDE_REFRACTION,
		EnvironmentOverrideTransparency = RPR_ENVIRONMENT_LIGHT_OVERRIDE_TRANSPARENCY,
		EnvironmentOverrideBackground = RPR_ENVIRONMENT_LIGHT_OVERRIDE_BACKGROUND,
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

	enum class UVTypeValue
	{
		Planar = RPR_MATERIAL_NODE_UVTYPE_PLANAR,
		Cylindrical = RPR_MATERIAL_NODE_UVTYPE_CYLINDICAL,
		Spherical = RPR_MATERIAL_NODE_UVTYPE_SPHERICAL,
		Project = RPR_MATERIAL_NODE_UVTYPE_PROJECT
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

			void DeleteAndClear()
			{
				rpr_int res = rprObjectDelete(handle);
				handle = nullptr;
				checkStatus(res);
			}

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

		bool SetName(const char* name)
		{
			if (!IsValid())
			{
				return false;
			}

			rpr_int status = rprObjectSetName(Handle(), name);
			return status == RPR_SUCCESS;
		}

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

	public:
		Node(const MaterialSystem& ms, int type, bool destroyOnDelete = true, Data* data = nullptr);	// not typesafe

		bool SetValue(rpr_material_node_input key, const Value& v);
		bool SetValueInt(rpr_material_node_input key, int);
		bool SetValueBuffer(rpr_material_node_input key, rpr_buffer buffer);

		MaterialSystem GetMaterialSystem() const;

		/// do not use if you can avoid it (unsafe)
		void _SetInputNode(rpr_material_node_input key, const Shader& shader);

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
			rpr_int res = RPR_SUCCESS;

			res = rprMaterialNodeGetInputInfo(Handle(), i, NodeInputInfoId, sizeof(info.id), &info.id, nullptr);
			checkStatus(res);

			// get type
			res = rprMaterialNodeGetInputInfo(Handle(), i, NodeInputInfoType, sizeof(info.type), &info.type, nullptr);
			checkStatus(res);

			info.name = "";
#if RPR_VERSION_MAJOR_MINOR_REVISION < 0x00103402 
			// Get name
			res = rprMaterialNodeGetInputInfo(Handle(), i, NodeInputInfoName, 0, nullptr, &size);
			checkStatus(res);

			info.name.resize(size);
			res = rprMaterialNodeGetInputInfo(Handle(), i, NodeInputInfoName, size, const_cast<char*>(info.name.data()), nullptr);
			checkStatus(res);
#endif

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
			if (type != rhs.type)
			{
				return false;
			}

			if ((type == NODE) &&
				(node == rhs.node))
			{
				return true;
			}

			// we should use correct float comparison method
			return IsAlmostEqual(x, rhs.x) && 
					IsAlmostEqual(y, rhs.y) && 
					IsAlmostEqual(z, rhs.z) && 
					IsAlmostEqual(w, rhs.w);
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
		Value SelectW() const;

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

	class DataBuffer : public Object
	{
		DECLARE_OBJECT_NO_DATA(DataBuffer, Object);

	public:
		explicit DataBuffer(rpr_buffer h, const Context &context) : Object(h, context, true, new Data()) {}

		DataBuffer(Context context, const rpr_buffer_desc& bufferDesc, const float* data);
	};

	class Shape : public Object
	{
		friend class Shader;

		DECLARE_OBJECT(Shape, Object);
		class Data : public Object::Data
		{
			DECLARE_OBJECT_DATA
		public:
			Object shader; // optimization for shape with only one shader
			std::vector<Object> shaders;
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
		void SetPerFaceShader(Shader shader, std::vector<int>& face_ids);

		void SetVolumeShader( const Shader& shader );
		Shader GetVolumeShader() const;

		void SetVisibility(bool visible)
		{
			auto res = rprShapeSetVisibility(Handle(), visible);

			if (res == RPR_ERROR_UNSUPPORTED)
			{
				return;
			}
			else
			{
				checkStatus(res);
			}
		}
		void SetPrimaryVisibility(bool visible)
		{
			auto res = rprShapeSetVisibilityFlag(Handle(), RPR_SHAPE_VISIBILITY_PRIMARY_ONLY_FLAG, visible);

			if (res == RPR_ERROR_UNSUPPORTED)
			{
				return;
			}
			else
			{
				checkStatus(res);
			}
		}
		void SetReflectionVisibility(bool visible)
		{
			auto res = rprShapeSetVisibilityFlag(Handle(), RPR_SHAPE_VISIBILITY_REFLECTION, visible);
			if (res == RPR_ERROR_UNSUPPORTED)
			{
				return;
			}
			else
			{
				checkStatus(res);
			}

			res = rprShapeSetVisibilityFlag(Handle(), RPR_SHAPE_VISIBILITY_GLOSSY_REFLECTION, visible);

			if (res == RPR_ERROR_UNSUPPORTED)
			{
				return;
			}
			else
			{
				checkStatus(res);
			}
		}

		void setRefractionVisibility(bool visible)
		{
			auto res = rprShapeSetVisibilityFlag(Handle(), RPR_SHAPE_VISIBILITY_REFRACTION, visible);
			if (res == RPR_ERROR_UNSUPPORTED)
			{
				return;
			}
			else
			{
				checkStatus(res);
			}

			res = rprShapeSetVisibilityFlag(Handle(), RPR_SHAPE_VISIBILITY_GLOSSY_REFRACTION, visible);
			if (res == RPR_ERROR_UNSUPPORTED)
			{
				return;
			}
			else
			{
				checkStatus(res);
			}
		}

		void SetLightShapeVisibilityEx(bool visible)
		{
			auto res = rprShapeSetVisibilityFlag(Handle(), RPR_SHAPE_VISIBILITY_LIGHT, visible);
			if (res == RPR_ERROR_UNSUPPORTED)
			{
				return;
			}

			checkStatus(res);
		}

		void SetLightGroupId(rpr_uint id)
		{
			rpr_status res = rprShapeSetLightGroupID(Handle(), id);
			checkStatus(res);
		}

		Shape CreateInstance(Context context) const;
		void SetTransform(const float* tm, bool transpose = false)
		{
			auto res = rprShapeSetTransform(Handle(), transpose, tm);
			checkStatus(res);
		}

		void SetMotionTransform(const float* tm, bool transpose = false)
		{
			rpr_status res = rprShapeSetMotionTransform(Handle(), false, tm, 1); // matrix at time=1
			checkStatus(res);

			res = rprShapeSetMotionTransformCount(Handle(), 1);
			checkStatus(res);
		}

		void SetObjectId(rpr_uint id)
		{
			auto res = rprShapeSetObjectID(Handle(), id);
			checkStatus(res);
		}

		void SetShadowColor(const Value &color)
		{
			auto res = rprShapeSetShadowColor(Handle(), color.GetX(), color.GetY(), color.GetZ());
			checkStatus(res);
		}

		void SetVertexColors(const std::vector<int>& vertexIndices, const std::vector<MColor>& vertexColors, rpr_int indexCount)
		{
			const int numComponents = 4;
			std::vector<rpr_float> colors;
			colors.resize(indexCount * numComponents);

			for (int colorComponent = 0; colorComponent < numComponents; colorComponent++)
			{
				for (int vertexIndex : vertexIndices)
				{
					switch (colorComponent)
					{
					case 0:
						colors[vertexIndex * numComponents] = vertexColors[vertexIndex].r;
						break;
					case 1:
						colors[vertexIndex * numComponents + 1] = vertexColors[vertexIndex].g;
						break;
					case 2:
						colors[vertexIndex * numComponents + 2] = vertexColors[vertexIndex].b;
						break;
					case 3:
						colors[vertexIndex * numComponents + 3] = vertexColors[vertexIndex].a;
						break;
					}
				}
			}
			rprShapeSetPrimvar(Handle(), 0, colors.data(), colors.size(), numComponents, RPR_PRIMVAR_INTERPOLATION_VERTEX); // we use primvar channel with number zero to store vertex colors
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
			auto res = rprShapeSetVisibilityFlag(Handle(), RPR_SHAPE_VISIBILITY_SHADOW, castsShadows);

			if (res == RPR_ERROR_UNSUPPORTED)
			{
				return;
			}
			else
			{
				checkStatus(res);
			}
		}

		void SetReceiveShadowFlag(bool receivesShadows)
		{
			auto res = rprShapeSetVisibilityFlag(Handle(), RPR_SHAPE_VISIBILITY_RECEIVE_SHADOW, receivesShadows);

			if (res == RPR_ERROR_UNSUPPORTED)
			{
				return;
			}
			else
			{
				checkStatus(res);
			}
		}

		void SetContourVisibilityFlag(bool isContourVisible)
		{
			auto res = rprShapeSetContourIgnore(Handle(), !isContourVisible);

			if (res == RPR_ERROR_UNSUPPORTED)
			{
				return;
			}
			else
			{
				checkStatus(res);
			}
		}

		void SetShadowCatcherFlag(bool enableShadowCatcher)
		{
			auto res = rprShapeSetShadowCatcher(Handle(), enableShadowCatcher);
			if (res == RPR_ERROR_UNSUPPORTED)
			{
				return;
			}
			else
			{
				checkStatus(res);
			}
		}

		void SetReflectionCatcherFlag(bool enableReflectionCatcher)
		{
			auto res = rprShapeSetReflectionCatcher(Handle(), enableReflectionCatcher);

			if (res == RPR_ERROR_UNSUPPORTED)
			{
				return;
			}
			else
			{
				checkStatus(res);
			}
		}

		void SetDisplacementNoSubdiv(Value image, float minscale = 0, float maxscale = 1);
		void SetDisplacement(Value image, float minscale = 0, float maxscale = 1);
		Node GetDisplacementMap();
		void RemoveDisplacement();
		void SetSubdivisionFactor(int sub);
		void SetAdaptiveSubdivisionFactor(float adaptiveFactor, int height, rpr_camera camera, rpr_framebuffer frameBuf, bool isRpr20);
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

		void SetLightGroupId(rpr_uint id)
		{
			rpr_status res = rprLightSetGroupId(Handle(), id);
			checkStatus(res);
		}

		void AddGLTFExtraIntAttribute(const std::string& attrName, int value)
		{
			rprGLTF_AddExtraLightParameter(Handle(), attrName.c_str(), value);
		}
	};

	class Image : public Object
	{
		DECLARE_OBJECT(Image, Object);

		class Data : public Object::Data
		{
			DECLARE_OBJECT_DATA
		public:
			virtual ~Data()
			{
				for (auto it = m_udimsMap.begin(); it != m_udimsMap.end(); ++it)
				{
					rprImageSetUDIM(Handle(), it->first, nullptr);
				}

				m_udimsMap.clear();
			}
			
			std::map<rpr_uint, Image> m_udimsMap;
		};


	public:
		explicit Image(rpr_image h, const Context& context) : Object(h, context, true, new Data()) {}

		Image(Context context, float r, float g, float b);
		Image(Context context, const rpr_image_format& format, const rpr_image_desc& image_desc, const void* data);

		// create empty image (used for UDIM creation of master image
		Image(Context context, const rpr_image_format& format);
		Image(Context context, const char * filename);
		void SetGamma(float gamma)
		{
			rpr_int res = rprImageSetGamma(Handle(), gamma);
			checkStatus(res);
		}

		void SetColorSpace(const char *colorspace)
		{
			if (data().m_udimsMap.empty())
			{
				rpr_int res = rprImageSetOcioColorspace(Handle(), colorspace);
				checkStatus(res);
			}
			else
			{
				for (auto udimMap : data().m_udimsMap)
				{
					udimMap.second.SetColorSpace(colorspace);
				}
			}
		}

		void SetUDIM(rpr_uint tileIndex, frw::Image image)
		{
			rpr_int res = rprImageSetUDIM(Handle(), tileIndex, image.Handle());
			checkStatus(res);

			data().m_udimsMap[tileIndex] = image;
		}

		bool HasAlphaChannel()
		{
			rpr_image_format format;
			rpr_int status = rprImageGetInfo(Handle(), RPR_IMAGE_FORMAT, sizeof(format), &format, NULL);
			checkStatus(status);
			return format.num_components == 4;
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

	class SphereLight : public Light
	{
		DECLARE_OBJECT_NO_DATA(SphereLight, Light);
	public:
		SphereLight(rpr_light h, const Context &context) : Light(h, context, new Data()) {}
		void SetRadiantPower(float r, float g, float b)
		{
			auto res = rprSphereLightSetRadiantPower3f(Handle(), r, g, b);
			checkStatus(res);
		}

		void SetRadius(float radius)
		{
			auto res = rprSphereLightSetRadius(Handle(), radius);
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

	class DiskLight : public Light
	{
		DECLARE_OBJECT_NO_DATA(DiskLight, Light);
	public:
		DiskLight(rpr_light h, const Context &context) : Light(h, context, new Data()) {}
		void SetRadiantPower(float r, float g, float b)
		{
			auto res = rprDiskLightSetRadiantPower3f(Handle(), r, g, b);
			checkStatus(res);
		}

		void SetRadius(float radius)
		{
			auto res = rprDiskLightSetRadius(Handle(), radius);
			checkStatus(res);
		}

		void SetAngle(float angle)
		{
			auto res = rprDiskLightSetAngle(Handle(), angle);
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

			std::map<EnvironmentOverride, EnvironmentLight> overrides;
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

		void SetEnvironmentOverride(EnvironmentOverride e, EnvironmentLight light)
		{
			auto res = rprEnvironmentLightSetEnvironmentLightOverride(Handle(), e, light.Handle());
			checkStatus(res);
			data().overrides[e] = light;
		}
	};

	class Curve : public Object
	{
		friend class Shader;

		DECLARE_OBJECT(Curve, Object);
		class Data : public Object::Data
		{
			DECLARE_OBJECT_DATA
		public:
			Object shader;
			virtual ~Data();
		};

	public:
		Curve(rpr_curve h, const Context &context) : Object(h, context, true, new Data()) {}

		void SetShader(Shader shader);
		Shader GetShader() const;

		void SetTransform(rpr_bool transpose, rpr_float const * transform)
		{
			rpr_int res = rprCurveSetTransform(Handle(), transpose, transform);
			checkStatus(res);
		}

		void SetShadowFlag(bool castsShadows)
		{
			auto res = rprCurveSetVisibilityFlag(Handle(), RPR_CURVE_VISIBILITY_SHADOW, castsShadows);

			if (res == RPR_ERROR_UNSUPPORTED)
			{
				return;
			}
			else
			{
				checkStatus(res);
			}
		}

		void SetReceiveShadowFlag(bool receivesShadows)
		{
			auto res = rprCurveSetVisibilityFlag(Handle(), RPR_CURVE_VISIBILITY_RECEIVE_SHADOW, receivesShadows);

			if (res == RPR_ERROR_UNSUPPORTED)
			{
				return;
			}
			else
			{
				checkStatus(res);
			}
		}

		void SetPrimaryVisibility(bool visible)
		{
			auto res = rprCurveSetVisibilityFlag(Handle(), RPR_CURVE_VISIBILITY_PRIMARY_ONLY_FLAG, visible);

			if (res == RPR_ERROR_UNSUPPORTED)
			{
				return;
			}
			else
			{
				checkStatus(res);
			}

			res = rprCurveSetVisibilityFlag(Handle(), RPR_CURVE_VISIBILITY_DIFFUSE, visible);

			if (res == RPR_ERROR_UNSUPPORTED)
			{
				return;
			}
			else
			{
				checkStatus(res);
			}
		}

		void SetReflectionVisibility(bool visible)
		{
			auto res = rprCurveSetVisibilityFlag(Handle(), RPR_CURVE_VISIBILITY_REFLECTION, visible);
			if (res == RPR_ERROR_UNSUPPORTED)
			{
				return;
			}
			else
			{
				checkStatus(res);
			}

			res = rprCurveSetVisibilityFlag(Handle(), RPR_CURVE_VISIBILITY_GLOSSY_REFLECTION, visible);

			if (res == RPR_ERROR_UNSUPPORTED)
			{
				return;
			}
			else
			{
				checkStatus(res);
			}
		}

		void setRefractionVisibility(bool visible)
		{
			auto res = rprCurveSetVisibilityFlag(Handle(), RPR_CURVE_VISIBILITY_REFRACTION, visible);
			if (res == RPR_ERROR_UNSUPPORTED)
			{
				return;
			}
			else
			{
				checkStatus(res);
			}

			res = rprCurveSetVisibilityFlag(Handle(), RPR_CURVE_VISIBILITY_GLOSSY_REFRACTION, visible);
			if (res == RPR_ERROR_UNSUPPORTED)
			{
				return;
			}
			else
			{
				checkStatus(res);
			}
		}

		void SetLightShapeVisibilityEx(bool visible)
		{
			auto res = rprCurveSetVisibilityFlag(Handle(), RPR_CURVE_VISIBILITY_LIGHT, visible);
			if (res == RPR_ERROR_UNSUPPORTED)
			{
				return;
			}

			checkStatus(res);
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


		void SetMotionTransform(const float* tm, bool transpose = false)
		{
			rpr_status res = rprCameraSetMotionTransform(Handle(), false, tm, 1); // matrix at time=1
			checkStatus(res);

			res = rprCameraSetMotionTransformCount(Handle(), 1);
			checkStatus(res);
		}

	};

	class VolumeGrid : public Object
	{
		DECLARE_OBJECT_NO_DATA(VolumeGrid, Object);

	public:
		VolumeGrid(rpr_grid h, const Context &context) : Object(h, context, true, new Data()) {}

	};

	class Volume : public Object
	{
		DECLARE_OBJECT_NO_DATA(Volume, Object);

	public:
		Volume(rpr_hetero_volume h, const Context &context) : Object(h, context, true, new Data()) {}

		void SetTransform(rpr_float const * transform, rpr_bool transpose)
		{
			rpr_int res = rprHeteroVolumeSetTransform(Handle(), transpose, transform);
			checkStatus(res);
		}

		void AttachToShape(Shape v)
		{
			rpr_int res = rprShapeSetHeteroVolume(v.Handle(), Handle());
			checkStatus(res);
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

			EnvironmentLight environmentLight;
			
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

			if (v.IsAreaLight())
			{
				data().mAreaLightCount++;
			}

			data().mShapeCount++;
		}
		void Attach(Light v)
		{
			AddReference(v);
			auto res = rprSceneAttachLight(Handle(), v.Handle());
			checkStatus(res);

			data().mLightCount++;
		}
		void Attach(Volume v)
		{
			AddReference(v);
			auto res = rprSceneAttachHeteroVolume(Handle(), v.Handle());
			checkStatus(res);
			// need to create fake shape for this volume that it needs to be attached to exist
			// (you can see the rpr_hetero_volume as a "material")
			// note that the shape must have the material RPR_MATERIAL_NODE_TRANSPARENT attached to it, in order to render the volume
		}
		void Attach(Curve crv)
		{
			AddReference(crv);
			auto res = rprSceneAttachCurve(Handle(), crv.Handle());
			checkStatus(res);
		}
		void Detach(Shape v)
		{
			RemoveReference(v);
			auto res = rprSceneDetachShape(Handle(), v.Handle());
			checkStatus(res);

			if (v.IsAreaLight()) data().mAreaLightCount--;
			data().mShapeCount--;
		}
		void Detach(Volume v)
		{
			RemoveReference(v);
			auto res = rprSceneDetachHeteroVolume(Handle(), v.Handle());
			checkStatus(res);
		}
		void Detach(Light v)
		{
			RemoveReference(v);
			auto res = rprSceneDetachLight(Handle(), v.Handle());
			checkStatus(res);

			data().mLightCount--;
		}
		void Detach(Curve crv)
		{
			RemoveReference(crv);
			auto res = rprSceneDetachCurve(Handle(), crv.Handle());
			checkStatus(res);
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

		// Used for Hybrid
		void SetEnvironmentLight(EnvironmentLight light)
		{
			rpr_uint res = rprSceneSetEnvironmentLight(Handle(), light.Handle());
			checkStatus(res);

			data().environmentLight = light;

			if (light.IsValid())
			{
				data().mLightCount++;
			}
			else
			{
				data().mLightCount--;
			}
		}

		rpr_int SetBackgroundImage(Image v)
		{
			auto res = rprSceneSetBackgroundImage(Handle(), v.Handle());
			if (res == RPR_SUCCESS)
			{
				data().backgroundImage = v;
			}

			return res;
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

//#if RPR_VERSION_MAJOR_MINOR_REVISION < 0x00103402 
		void SetParameter(rpr_context_info parameter, rpr_uint value)
		{
			auto res = rprContextSetParameterByKey1u(Handle(), parameter, value);

			if (res == RPR_ERROR_UNSUPPORTED ||
				res == RPR_ERROR_INVALID_PARAMETER)
			{
				return;
			}
			else
			{
				checkStatus(res);
			}

		}

		void SetParameter(rpr_context_info parameter, int value)
		{
			auto res = rprContextSetParameterByKey1u(Handle(), parameter, value);

			if (res == RPR_ERROR_UNSUPPORTED ||
				res == RPR_ERROR_INVALID_PARAMETER)
			{
				return;
			}
			else
			{
				checkStatus(res);
			}
		}

		void SetParameter(rpr_context_info parameter, double value)
		{
			auto res = rprContextSetParameterByKey1f(Handle(), parameter, (rpr_float)value);

			if (res == RPR_ERROR_UNSUPPORTED ||
				res == RPR_ERROR_INVALID_PARAMETER)
			{
				return;
			}
			else
			{
				checkStatus(res);
			}
		}

		void SetParameter(rpr_context_info parameter, double x, double y, double z)
		{
			auto res = rprContextSetParameterByKey3f(Handle(), parameter, (rpr_float)x, (rpr_float)y, (rpr_float)z);

			if (res == RPR_ERROR_UNSUPPORTED ||
				res == RPR_ERROR_INVALID_PARAMETER)
			{
				return;
			}
			else
			{
				checkStatus(res);
			}
		}

		void SetParameter(rpr_context_info parameter, double x, double y, double z, double w)
		{
			auto res = rprContextSetParameterByKey4f(Handle(), parameter, (rpr_float)x, (rpr_float)y, (rpr_float)z, (rpr_float)w);

			if (res == RPR_ERROR_UNSUPPORTED ||
				res == RPR_ERROR_INVALID_PARAMETER)
			{
				return;
			}
			else
			{
				checkStatus(res);
			}
		}
//#endif

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
			const rpr_int* tidx_stride, const rpr_int * num_face_vertices, size_t num_faces, rpr_mesh_info* meshAttrArray = nullptr, const std::string& optionalMeshName = "") const;

		Shape CreateVoidMesh();

		PointLight CreatePointLight()
		{
			FRW_PRINT_DEBUG("CreatePointLight()");
			rpr_light h;
			auto status = rprContextCreatePointLight(Handle(), &h);
			checkStatusThrow(status, "Unable to create point light");

			return PointLight(h, *this);
		}

		SphereLight CreateSphereLight()
		{
			FRW_PRINT_DEBUG("CreateSphereLight()");
			rpr_light h;
			auto status = rprContextCreateSphereLight(Handle(), &h);
			checkStatusThrow(status, "Unable to create sphere light");

			return SphereLight(h, *this);
		}

		SpotLight CreateSpotLight()
		{
			FRW_PRINT_DEBUG("CreateSpotLight()");
			rpr_light h;
			auto status = rprContextCreateSpotLight(Handle(), &h);
			checkStatusThrow(status, "Unable to create spot light");

			return SpotLight(h, *this);
		}

		DiskLight CreateDiskLight()
		{
			FRW_PRINT_DEBUG("CreateDiskLight()");
			rpr_light h;
			auto status = rprContextCreateDiskLight(Handle(), &h);
			checkStatusThrow(status, "Unable to create disk light");

			return DiskLight(h, *this);
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

		VolumeGrid CreateVolumeGrid(size_t gridSizeX, size_t gridSizeY,	size_t gridSizeZ,
			const std::vector<uint32_t>& gridOnIndices, const std::vector<float>& gridOnValueIndices, 
			rpr_grid_indices_topology indicesListTopology)
		{
			rpr_grid h = 0;

			rpr_int status = rprContextCreateGrid(Handle(), &h
				, gridSizeX, gridSizeY, gridSizeZ
				, &gridOnIndices[0], gridOnIndices.size() / 3, indicesListTopology
				, &gridOnValueIndices[0], gridOnValueIndices.size() * sizeof(gridOnValueIndices[0])
				, 0);
			checkStatusThrow(status, "Unable to create Hetero Volume - RPR failed to create albedo grid!");

			return VolumeGrid(h, *this);
		}

		Volume CreateVolume(rpr_grid densityGrid, rpr_grid albedoGrid, rpr_grid emissionGrid,
			const std::vector<float>& densityGridValues, 
			const std::vector<float>& albedoGridValues,
			const std::vector<float>& emissiveGridValues)
		{
			rpr_hetero_volume h = 0;
			rpr_int status = rprContextCreateHeteroVolume(Handle(), &h);
			checkStatusThrow(status, "Unable to create Hetero Volume - RPR create volume failed!");

			if (densityGrid != nullptr)
			{
				status = rprHeteroVolumeSetDensityGrid(h, densityGrid);
				checkStatusThrow(status, "Unable to set densiy grid - RPR create volume failed!");

				status = rprHeteroVolumeSetDensityLookup(h, &densityGridValues[0], (rpr_uint)densityGridValues.size() / 3);
				checkStatusThrow(status, "Unable to set densiy lookup table - RPR create volume failed!");
			}

			if (albedoGrid != nullptr)
			{
				status = rprHeteroVolumeSetAlbedoGrid(h, albedoGrid);
				checkStatusThrow(status, "Unable to set albedo grid - RPR create volume failed!");

				status = rprHeteroVolumeSetAlbedoLookup(h, &albedoGridValues[0], (rpr_uint)albedoGridValues.size() / 3);
				checkStatusThrow(status, "Unable to set albedo lookup table - RPR create volume failed!");
			}

			if (emissionGrid != nullptr)
			{
				status = rprHeteroVolumeSetEmissionGrid(h, emissionGrid);
				checkStatusThrow(status, "Unable to set emission grid - RPR create volume failed!");

				status = rprHeteroVolumeSetEmissionLookup(h, &emissiveGridValues[0], (rpr_uint)emissiveGridValues.size() / 3);
				checkStatusThrow(status, "Unable to set emission lookup table - RPR create volume failed!");
			}

			return Volume(h, *this);
		}

		struct VolumeData
		{
			rpr_grid m_densityGrid;
			rpr_grid m_albedoGrid;
			rpr_grid m_emissionGrid;
			std::vector<float> m_densityLookup;
			std::vector<float> m_albedoLookup;
			std::vector<float> m_emissionLookup;
		};

		Volume CreateVolume(size_t gridSizeX,
			size_t gridSizeY,
			size_t gridSizeZ,
			float const * voxelData, // float3 albedo RGB, float3 emision, float density (currently not used because we set ramps instead of passing volume color and density values directly
			size_t numberOfVoxels,
			float const * albedoCtrPoints = nullptr,
			size_t countOfAlbedoCtrlPoints = 0,
			float const * albedoVal = nullptr,
			float const * emissionCtrPoints = nullptr,
			size_t countOfEmissionCtrlPoints = 0,
			float const * emissionVal = nullptr,
			float const * densityCtrPoints = nullptr,
			size_t countOfDensityCtrlPoints = 0,
			float const * densityVal = nullptr)
		{
			FRW_PRINT_DEBUG("CreateVolume()");

			// create rpr volume node
			rpr_hetero_volume h = 0;
			rpr_int status = rprContextCreateHeteroVolume(Handle(), &h);
			checkStatusThrow(status, "Unable to create Hetero Volume - RPR create volume failed!");

			// create volume data
			auto volumeData = CreateVolumeData(gridSizeX, gridSizeY, gridSizeZ, voxelData, numberOfVoxels, albedoCtrPoints,
				countOfAlbedoCtrlPoints, albedoVal, emissionCtrPoints, countOfEmissionCtrlPoints, emissionVal,
				densityCtrPoints, countOfDensityCtrlPoints, densityVal);

			// - attach albedo grid and lookup to volume
			status = rprHeteroVolumeSetAlbedoGrid(h, volumeData.m_albedoGrid);
			checkStatusThrow(status, "Unable to create Hetero Volume - RPR failed to attach albedo grid!");
			status = rprHeteroVolumeSetAlbedoLookup(h, volumeData.m_albedoLookup.data(), (rpr_uint) (volumeData.m_albedoLookup.size() / 3));
			checkStatusThrow(status, "Unable to create Hetero Volume - RPR failed to attach albedo lookup table!");

			// - attach emission grid and lookup to volume
			status = rprHeteroVolumeSetEmissionGrid(h, volumeData.m_emissionGrid);
			checkStatusThrow(status, "Unable to create Hetero Volume - RPR failed to attach emission grid!");
			status = rprHeteroVolumeSetEmissionLookup(h, volumeData.m_emissionLookup.data(), (rpr_uint) (volumeData.m_emissionLookup.size() / 3));
			checkStatusThrow(status, "Unable to create Hetero Volume - RPR failed to attach emission lookup table!");

			// - attach density grid and lookup to volume
			status = rprHeteroVolumeSetDensityGrid(h, volumeData.m_densityGrid);
			checkStatusThrow(status, "Unable to create Hetero Volume - RPR failed to attach grid!");
			status = rprHeteroVolumeSetDensityLookup(h, volumeData.m_densityLookup.data(), (rpr_uint) (volumeData.m_densityLookup.size() / 3) );
			checkStatusThrow(status, "Unable to create Hetero Volume - RPR failed to attach lookup table!");
			
			return Volume(h, *this);
		}

		VolumeData CreateVolumeData(size_t gridSizeX,
			size_t gridSizeY,
			size_t gridSizeZ,
			float const* voxelData, // float3 albedo RGB, float3 emision, float density (currently not used because we set ramps instead of passing volume color and density values directly
			size_t numberOfVoxels,
			float const* albedoCtrPoints = nullptr,
			size_t countOfAlbedoCtrlPoints = 0,
			float const* albedoVal = nullptr,
			float const* emissionCtrPoints = nullptr,
			size_t countOfEmissionCtrlPoints = 0,
			float const* emissionVal = nullptr,
			float const* densityCtrPoints = nullptr,
			size_t countOfDensityCtrlPoints = 0,
			float const* densityVal = nullptr)
		{
			FRW_PRINT_DEBUG("CreateVolumeData()");

			// create data object
			VolumeData volumeData;

			// ensure correct volume grid data size
			bool isGridDataValid = numberOfVoxels == gridSizeX * gridSizeY * gridSizeZ;
			if (!isGridDataValid)
			{
				checkStatusThrow(RPR_ERROR_INVALID_PARAMETER, "Unable to create Hetero Volume - invalid indices data!");
			}

			// create indices array 
			// we should have 1 index per voxel; index is index of value in grid
			// (we don't do optimization for cells with identical data now, but we might do in the future) 
			std::vector<size_t> indicesList(numberOfVoxels);
			for (size_t idx = 0; idx < numberOfVoxels; ++idx)
			{
				indicesList[idx] = idx;
			}

			// albedo
			// - atm only ctrl points are supported
			// - create look up table
			assert(countOfAlbedoCtrlPoints);
			std::vector<float> albedo_look_up; albedo_look_up.reserve(countOfAlbedoCtrlPoints);
			for (size_t idx = 0; idx < countOfAlbedoCtrlPoints; ++idx)
			{
				albedo_look_up.push_back(albedoCtrPoints[idx]);
			}

			// - create albedo grid
			std::vector<float> albedo; albedo.reserve(numberOfVoxels);
			for (size_t idx = 0; idx < numberOfVoxels; ++idx)
			{
				albedo.push_back(albedoVal[idx]);
			}

			rpr_grid albedoGrid;
			rpr_int status = rprContextCreateGrid(Handle(), &albedoGrid,
				gridSizeX, gridSizeY, gridSizeZ,
				&indicesList[0], indicesList.size(), RPR_GRID_INDICES_TOPOLOGY_I_U64,
				&albedo[0], albedo.size() * sizeof(albedo[0]), 0
			);
			checkStatusThrow(status, "Unable to create Hetero Volume - RPR failed to create albedo grid!");

			// store grid and lookup into data object
			volumeData.m_albedoGrid = albedoGrid;
			volumeData.m_albedoLookup = albedo_look_up;

			// emission
			// - atm only ctrl points are supported
			// - create look up table
			assert(countOfEmissionCtrlPoints);
			std::vector<float> emission_look_up; emission_look_up.reserve(countOfEmissionCtrlPoints * 3);
			for (size_t idx = 0; idx < countOfEmissionCtrlPoints; ++idx)
			{
				emission_look_up.push_back(emissionCtrPoints[idx] * 10.f);
			}

			// - create emission grid
			std::vector<float> emission; emission.reserve(numberOfVoxels);
			for (size_t idx = 0; idx < numberOfVoxels; ++idx)
			{
				emission.push_back(emissionVal[idx]);
			}

			rpr_grid emissionGrid;
			status = rprContextCreateGrid(Handle(), &emissionGrid,
				gridSizeX, gridSizeY, gridSizeZ,
				&indicesList[0], indicesList.size(), RPR_GRID_INDICES_TOPOLOGY_I_U64,
				&emission[0], emission.size() * sizeof(emission[0]), 0
			);
			checkStatusThrow(status, "Unable to create Hetero Volume - RPR failed to create emission grid!");

			// store grid and lookup into data object
			volumeData.m_emissionGrid = emissionGrid;
			volumeData.m_emissionLookup = emission_look_up;

			// density
			// - atm only ctrl points are supported
			// - create look up table
			assert(countOfDensityCtrlPoints);
			std::vector<float> density_look_up; density_look_up.reserve(countOfDensityCtrlPoints);
			for (size_t idx = 0; idx < countOfDensityCtrlPoints; ++idx)
			{
				density_look_up.push_back(densityCtrPoints[idx] * 1000.0f);
				density_look_up.push_back(densityCtrPoints[idx] * 1000.0f);
				density_look_up.push_back(densityCtrPoints[idx] * 1000.0f);
			}

			// - create density grid
			std::vector<float> density; density.reserve(numberOfVoxels);
			for (size_t idx = 0; idx < numberOfVoxels; ++idx)
			{
				density.push_back(densityVal[idx]);
			}

			rpr_grid densityGrid;
			status = rprContextCreateGrid(Handle(), &densityGrid,
				gridSizeX, gridSizeY, gridSizeZ,
				&indicesList[0], indicesList.size(), RPR_GRID_INDICES_TOPOLOGY_I_U64,
				&density[0], density.size() * sizeof(density[0]), 0
			);
			checkStatusThrow(status, "Unable to create Hetero Volume - RPR failed to create densitty grid!");

			// store grid and lookup into data object
			volumeData.m_densityGrid = densityGrid;
			volumeData.m_densityLookup = density_look_up;

			return volumeData;
		}

		Curve CreateCurve(size_t num_controlPoints, rpr_float const * controlPointsData, 
			rpr_int controlPointsStride, size_t num_indices, 
			rpr_uint curveCount, rpr_uint const * indicesData, 
			rpr_float const * radiuses, rpr_float const * textureCoordUV, 
			rpr_int * segmentsPerCurve)
		{
			FRW_PRINT_DEBUG("CreateCurve()");

			// Ensure correct points count
			size_t totalSegmentsCount = 0;

			for (size_t idx = 0; idx < curveCount; ++idx)
			{
				totalSegmentsCount += segmentsPerCurve[idx];
			}

			// Segment of RPR curve should always be composed of 4 3D points
			const unsigned int pointsPerSegment = 4;

			if (num_indices != (totalSegmentsCount * pointsPerSegment))
			{
				checkStatusThrow(RPR_ERROR_INVALID_PARAMETER, "Unable to create Hair Curve - invalid indices data!");
			}

			// create curve
			rpr_curve h = 0;
			auto status = rprContextCreateCurve(Handle(), &h,
				num_controlPoints, controlPointsData,
				controlPointsStride, num_indices,
				curveCount, indicesData,
				radiuses, textureCoordUV,
				segmentsPerCurve, 1);

			checkStatusThrow(status, "Unable to create Hair Curve");

			return Curve(h, *this);
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

		void SetUpdateCallback(void* callback, void* userData)
		{
			rpr_int status = RPR_SUCCESS;
			status = rprContextSetParameterByKeyPtr(Handle(), RPR_CONTEXT_RENDER_UPDATE_CALLBACK_FUNC, callback);
			assert(status == RPR_SUCCESS);

			status = rprContextSetParameterByKeyPtr(Handle(), RPR_CONTEXT_RENDER_UPDATE_CALLBACK_DATA, userData);
			assert(status == RPR_SUCCESS);
		}

		void SetSceneSyncFinCallback(void* callback, void* userData)
		{
			rpr_int status = RPR_SUCCESS;
			status = rprContextSetParameterByKeyPtr(Handle(), RPR_CONTEXT_UPDATE_TIME_CALLBACK_FUNC, callback);
			assert(status == RPR_SUCCESS);

			status = rprContextSetParameterByKeyPtr(Handle(), RPR_CONTEXT_UPDATE_TIME_CALLBACK_DATA, userData);
			assert(status == RPR_SUCCESS);
		}

		void SetFirstIterationCallback(void* callback, void* userData)
		{
			rpr_int status = RPR_SUCCESS;
			status = rprContextSetParameterByKeyPtr(Handle(), RPR_CONTEXT_FIRST_ITERATION_TIME_CALLBACK_FUNC, callback);
			assert(status == RPR_SUCCESS);

			status = rprContextSetParameterByKeyPtr(Handle(), RPR_CONTEXT_FIRST_ITERATION_TIME_CALLBACK_DATA, userData);
			assert(status == RPR_SUCCESS);
		}

		void SetRenderTimeCallback(void* callback, void* userData)
		{
			rpr_int status = RPR_SUCCESS;
			status = rprContextSetParameterByKeyPtr(Handle(), RPR_CONTEXT_RENDER_TIME_CALLBACK_FUNC, callback);
			assert(status == RPR_SUCCESS);

			status = rprContextSetParameterByKeyPtr(Handle(), RPR_CONTEXT_RENDER_TIME_CALLBACK_DATA, userData);
			assert(status == RPR_SUCCESS);
		}

		void AbortRender()
		{
			rpr_int status = RPR_SUCCESS;
			status = rprContextAbortRender(Handle());
			assert(status == RPR_SUCCESS);
		}

		void Render()
		{
			auto status = rprContextRender(Handle());

			if (RPR_ERROR_ABORTED == status)
				return;

#define SHOW_EXTENDED_ERROR_MSG
#ifndef SHOW_EXTENDED_ERROR_MSG
			checkStatusThrow(status, "Unable to render");
#else
			if (status != RPR_SUCCESS)
			{
				size_t length = 0;
				rpr_int status2 = rprContextGetInfo(Handle(), RPR_CONTEXT_LAST_ERROR_MESSAGE, sizeof(size_t), nullptr, &length);

				std::vector<rpr_char> path;
				if (status2 == RPR_SUCCESS && length > 0)
				{
					path.resize(length);
					rprContextGetInfo(Handle(), RPR_CONTEXT_LAST_ERROR_MESSAGE, path.size(), &path[0], nullptr);
				}

				std::string error_msg(path.begin(), path.end());
				LogPrint(error_msg.c_str());

				checkStatusThrow(status, error_msg.c_str());
			}
#endif
		}

		void RenderTile(int rxmin, int rxmax, int rymin, int rymax)
		{
			auto status = rprContextRenderTile(Handle(), rxmin, rxmax, rymin, rymax);

			if (RPR_ERROR_ABORTED == status)
				return;

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

		bool IsAdaptiveSamplingFinalized()
		{
			rpr_uint activePixelCount;
			rpr_int res = rprContextGetInfo(Handle(), RPR_CONTEXT_ACTIVE_PIXEL_COUNT, sizeof(activePixelCount), &activePixelCount, 0);

			if (res == RPR_ERROR_UNSUPPORTED || res == RPR_ERROR_UNIMPLEMENTED)
			{
				return false;
			}

			// comment this code to avoid error messages since northstar (tahoe 2.0) does not support contextgetinfo call
			if (!checkStatus(res))
			{
				return false;
			}

			return activePixelCount == 0;
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

			info.name = "";

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
			rprContextSetParameterByKey1u(nullptr, RPR_CONTEXT_TRACING_ENABLED, 0);

			if (tracingfolder)
			{
				auto res = rprContextSetParameterByKeyString(nullptr, RPR_CONTEXT_TRACING_PATH, tracingfolder);

				if (RPR_SUCCESS == res)
					rprContextSetParameterByKey1u(nullptr, RPR_CONTEXT_TRACING_ENABLED, 1);
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
			return rprMaterialNodeSetInputImageDataByKey(Handle(), RPR_MATERIAL_INPUT_DATA, v.Handle());
		}
	};

	class GridNode : public ValueNode
	{
	public:
		explicit GridNode(const MaterialSystem& h) : ValueNode(h, ValueTypeGridSampler) {}
		rpr_int SetGrid(VolumeGrid v)
		{
			AddReference(v);
			return rprMaterialNodeSetInputGridDataByKey(Handle(), RPR_MATERIAL_INPUT_DATA, v.Handle());
		}
	};

	class BlackBodyNode : public ValueNode
	{
	public:
		explicit BlackBodyNode(const MaterialSystem& h) : ValueNode(h, ValueTypeBlackBody) {}
		rpr_int SetGridSampler(GridNode v)
		{
			AddReference(v);
			return rprMaterialNodeSetInputNByKey(Handle(), RPR_MATERIAL_INPUT_TEMPERATURE, v.Handle());
		}
		rpr_int SetTemperatureValueKelvin(float f)
		{
			return rprMaterialNodeSetInputFByKey(Handle(), RPR_MATERIAL_INPUT_KELVIN, f, 0.0f, 0.0f, 0.0);
		}
	};

	class LookupNode : public ValueNode
	{
	public:
		LookupNode(const MaterialSystem& h, LookupType v) : ValueNode(h, ValueTypeLookup)
		{
			SetValueInt(RPR_MATERIAL_INPUT_VALUE, v);
		}
	};

	class PrimvarLookupNode : public ValueNode
	{
	public:
		PrimvarLookupNode(const MaterialSystem& h, rpr_int key) : ValueNode(h, ValueTypePrimvarLookup)
		{
			SetValueInt(RPR_MATERIAL_INPUT_VALUE, key);
		}
	};

	class BufferNode : public ValueNode
	{
	public:
		BufferNode(const MaterialSystem& h) : ValueNode(h, ValueTypeBufferSampler) {}
		bool SetBuffer(DataBuffer buff )
		{
			AddReference(buff);
			return SetValueBuffer(RPR_MATERIAL_INPUT_DATA, buff.Handle());
		}
		bool SetUV(const Value& uv)
		{
			return SetValue(RPR_MATERIAL_INPUT_UV, uv);
		}
	};

	class ArithmeticNode : public ValueNode
	{
	public:
		ArithmeticNode(const MaterialSystem& h, Operator op) : ValueNode(h, ValueTypeArithmetic)
		{
			SetValueInt(RPR_MATERIAL_INPUT_OP, op);
		}
		ArithmeticNode(const MaterialSystem& h, Operator op, const Value& a) : ValueNode(h, ValueTypeArithmetic)
		{
			SetValueInt(RPR_MATERIAL_INPUT_OP, op);
			SetValue(RPR_MATERIAL_INPUT_COLOR0, a);
		}
		ArithmeticNode(const MaterialSystem& h, Operator op, const Value& a, const Value& b) : ValueNode(h, ValueTypeArithmetic)
		{
			SetValueInt(RPR_MATERIAL_INPUT_OP, op);
			SetValue(RPR_MATERIAL_INPUT_COLOR0, a);
			SetValue(RPR_MATERIAL_INPUT_COLOR1, b);
		}
		// arithmetic node doesn't support 3 operators
		void SetArgument(int i, const Value& v)
		{
			switch (i)
			{
			case 0: SetValue(RPR_MATERIAL_INPUT_COLOR0, v); break;
			case 1: SetValue(RPR_MATERIAL_INPUT_COLOR1, v); break;
			}
		}
	};

	class RampNode : public ValueNode
	{
	public:
		explicit RampNode(const MaterialSystem& h) : ValueNode(h, ValueTypeRamp) {}

		void SetLookup(const Value& v)
		{
			if (v.IsNode())
			{
				Node n = v.GetNode();
				AddReference(n);
				auto res = rprMaterialNodeSetInputNByKey(Handle(), RPR_MATERIAL_INPUT_UV, n.Handle());
				checkStatus(res);
			}
		}

		void SetControlPoints(const float* ctrlp, int bufferSize)
		{
			assert(ctrlp != nullptr);
			assert(bufferSize % 4 == 0); // control points should be float4
			auto res = rprMaterialNodeSetInputDataByKey(Handle(), RPR_MATERIAL_INPUT_DATA, ctrlp, bufferSize * sizeof(float));
			checkStatus(res);
		}

		void SetInterpolationMode(frw::RampInterpolationMode mode)
		{
			auto res = rprMaterialNodeSetInputUByKey(Handle(), RPR_MATERIAL_INPUT_TYPE, mode);
			checkStatus(res);
		}

		void SetMaterialControlPointValue(unsigned idx, const Value& v)
		{
			assert(idx <= 15);
			if (idx > 15)
				return;

			static std::vector<rpr_material_node_input> rampOverrideInputs =
			{ 
				RPR_MATERIAL_INPUT_0,
				RPR_MATERIAL_INPUT_1,
				RPR_MATERIAL_INPUT_2,
				RPR_MATERIAL_INPUT_3,
				RPR_MATERIAL_INPUT_4,
				RPR_MATERIAL_INPUT_5,
				RPR_MATERIAL_INPUT_6,
				RPR_MATERIAL_INPUT_7,
				RPR_MATERIAL_INPUT_8,
				RPR_MATERIAL_INPUT_9,
				RPR_MATERIAL_INPUT_10,
				RPR_MATERIAL_INPUT_11,
				RPR_MATERIAL_INPUT_12,
				RPR_MATERIAL_INPUT_13,
				RPR_MATERIAL_INPUT_14,
				RPR_MATERIAL_INPUT_15,
			};

			if (v.IsNode())
			{
				Node n = v.GetNode();
				AddReference(n);
				auto res = rprMaterialNodeSetInputNByKey(Handle(), rampOverrideInputs[idx], n.Handle());
				checkStatus(res);
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
				auto res = rprMaterialNodeSetInputNByKey(Handle(), RPR_MATERIAL_INPUT_COLOR, n.Handle());
				checkStatus(res);
			}
		}
	};

	class RGBToHSVNode : public ValueNode
	{
	public:
		explicit RGBToHSVNode(const MaterialSystem& h) : ValueNode(h, ValueTypeRRGToHSV) {}

		void SetInputColor(const Value& inputRGB)
		{
			SetValue(RPR_MATERIAL_INPUT_COLOR, inputRGB);
		}
	};

	class HSVToRGBNode : public ValueNode
	{
	public:
		explicit HSVToRGBNode(const MaterialSystem& h) : ValueNode(h, ValueTypeHSVToRGB) {}

		void SetInputColor(const Value& inputHSV)
		{
			SetValue(RPR_MATERIAL_INPUT_COLOR, inputHSV);
		}
	};

	class ToonRampNode : public ValueNode
	{
	public:
		explicit ToonRampNode(const MaterialSystem& h) : ValueNode(h, ValueTypeToonRamp) {}
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
				auto res = rprMaterialNodeSetInputNByKey(Handle(), RPR_MATERIAL_INPUT_COLOR, n.Handle());
				checkStatus(res);
			}
		}
	};

	class MaterialSystem : public Object
	{
		static const bool allowShortcuts = true;
		DECLARE_OBJECT(MaterialSystem, Object);

		class Data : public Object::Data
		{
			DECLARE_OBJECT_DATA;
		};

	protected:
		friend class Node;
		rpr_material_node CreateNode(rpr_material_node_type type) const
		{
			FRW_PRINT_DEBUG("CreateNode(%d) in MaterialSystem: 0x%016llX", type, Handle());
			rpr_material_node node = nullptr;
			auto res = rprMaterialSystemCreateNode(Handle(), type, &node);

			if (res != RPR_ERROR_UNSUPPORTED &&
				res != RPR_ERROR_INVALID_PARAMETER)
			{
				checkStatus(res);
			}

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

				if (Handle() == nullptr)
				{
					res = RPR_ERROR_INVALID_PARAMETER;
				}
				checkStatus(res);
				FRW_PRINT_DEBUG("\tCreated RPRX context 0x%016llX", data().rprxContext);
			}
		}

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
			node.SetValue(RPR_MATERIAL_INPUT_COLOR0, a);
			node.SetValue(RPR_MATERIAL_INPUT_COLOR1, b);
			node.SetValue(RPR_MATERIAL_INPUT_WEIGHT, t);
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

		Value ValueSelectW(const Value& a) const
		{
			if (allowShortcuts && a.IsFloat())
				return Value(a.w);

			return ArithmeticNode(*this, OperatorSelectW, a);
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

			node.SetValue(RPR_MATERIAL_INPUT_IOR, ior);
			// TODO the following fail if set to expected lookup values
			node.SetValue(RPR_MATERIAL_INPUT_INVEC, v);
			node.SetValue(RPR_MATERIAL_INPUT_NORMAL, n);
			return node;
		}

		Value ValueFresnelSchlick(const Value& reflectance, Value n = Value(), Value v = Value()) const
		{
			ValueNode node(*this, ValueTypeFresnelSchlick);

			node.SetValue(RPR_MATERIAL_INPUT_REFLECTANCE, reflectance);
			// TODO the following fail if set to expected lookup values
			node.SetValue(RPR_MATERIAL_INPUT_INVEC, v);
			node.SetValue(RPR_MATERIAL_INPUT_NORMAL, n);
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

		Value ValueConvertToLuminance(const frw::Value& value) const
		{
			frw::Value red = ValueMul(value.SelectX(), 0.3f);
			frw::Value green = ValueMul(value.SelectY(), 0.59f);
			frw::Value blue = ValueMul(value.SelectZ(), 0.11f);
			frw::Value action1 = ValueAdd(red, green);
			return ValueAdd(action1, blue);
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
		Value ValueMin(const Value& a, const Value& b) const
		{
			if (allowShortcuts && a.IsFloat() && b.IsFloat())
				return Value(std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z), std::min(a.w, b.w));

			return ArithmeticNode(*this, OperatorMin, a, b);
		}

		Value ValueMax(const Value& a, const Value& b) const
		{
			if (allowShortcuts && a.IsFloat() && b.IsFloat())
				return Value(std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z), std::max(a.w, b.w));

			return ArithmeticNode(*this, OperatorMax, a, b);
		}

		Value ValueClamp(const Value& v, const Value& minValue = 0.0f, const Value& maxValue = 1.0f) const
		{
			return ValueMin(ValueMax(v, minValue), maxValue);
		}

		Shader ShaderBlend(const Shader& a, const Shader& b, const Value& t) const;
		Shader ShaderDoubleSided(const Shader& a, const Shader& b) const;
		Shader ShaderAdd(const Shader& a, const Shader& b) const;
	};

    class AOMapNode : public ValueNode
    {
    public:
        explicit AOMapNode(const MaterialSystem& h, const Value& radius, const Value& side, const Value& samplesCount) : ValueNode(h, ValueTypeAOMap)
        {
            SetValue(RPR_MATERIAL_INPUT_RADIUS, radius);
            SetValue(RPR_MATERIAL_INPUT_SIDE, side);

			// set node value is temporarily commented out, because at the moment (core 1325) number of samples for ao is a parameter of context instead of parameter of node
			// this was discussed with Takahiro
			//SetValueInt("aoraycount", samplesCount.GetInt());
			Context ctx = h.GetContext();
			ctx.SetParameter(RPR_CONTEXT_AO_RAY_COUNT, samplesCount.GetInt());
        }
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

		void Resolve(FrameBuffer dest, bool normalizeOnly)
		{
			auto status = rprContextResolveFrameBuffer(GetContext().Handle(), Handle(), dest.Handle(), normalizeOnly);
			checkStatusThrow(status, "Unable to resolve frame buffer");
		}

		void Clear()
		{
			auto status = rprFrameBufferClear(Handle());
			checkStatusThrow(status, "Unable to clear frame buffer");
		}

		void DebugDumpFrameBufferToFile(std::string& fbName)
		{
			static int debugDumpIdx = 100;
			std::string dumpAddr = "C:\\temp\\dbg\\" + fbName + std::to_string(debugDumpIdx++) + ".bmp";
			rpr_int status = rprFrameBufferSaveToFile(Handle(), dumpAddr.c_str());
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
			};

		public:

			virtual bool IsValid() const
			{
				return Handle() != nullptr;
			}

			bool bDirty = true;

			int numAttachedShapes = 0;
			ShaderType shaderType = ShaderTypeInvalid;

			bool isShadowCatcher = false;
			ShadowCatcherParams mShadowCatcherParams;
			bool isReflectionCatcher = false;
		};

	public:

		void SetShadowCatcher(bool isShadowCatcher)
		{
			data().isShadowCatcher = isShadowCatcher;
		}

		bool IsShadowCatcher() const
		{
			return data().isShadowCatcher;
		}

		void SetShadowColor(float r, float g, float b, float a)
		{
			data().mShadowCatcherParams.mShadowR = r;
			data().mShadowCatcherParams.mShadowG = g;
			data().mShadowCatcherParams.mShadowB = b;
			data().mShadowCatcherParams.mShadowA = a;
		}

		void GetShadowColor(float* r, float* g, float* b, float* a) const
		{
			*r = data().mShadowCatcherParams.mShadowR;
			*g = data().mShadowCatcherParams.mShadowG;
			*b = data().mShadowCatcherParams.mShadowB;
			*a = data().mShadowCatcherParams.mShadowA;
		}

		void SetShadowWeight(float w)
		{
			data().mShadowCatcherParams.mShadowWeight = w;
		}

		float GetShadowWeight() const
		{
			return data().mShadowCatcherParams.mShadowWeight;
		}

		void SetReflectionCatcher(bool isReflectionCatcher)
		{
			data().isReflectionCatcher = isReflectionCatcher;
		}

		bool IsReflectionCatcher(void) const
		{
			return data().isReflectionCatcher;
		}

		Shader(DataPtr p)
		{
			m = p;
		}

		Shader(const MaterialSystem& ms, ShaderType type, bool destroyOnDelete = true) : Node(ms, type, destroyOnDelete, new Data())
		{
			data().shaderType = type;
		}

		Shader(const MaterialSystem& ms, const Context& context, bool destroyOnDelete = true) : Node(ms, ShaderType::ShaderTypeStandard, destroyOnDelete, new Data())
		{
			data().shaderType = ShaderType::ShaderTypeStandard;
		}

		frw::ShaderType GetShaderType() const
		{
			if (!IsValid())
				return ShaderTypeInvalid;
			return data().shaderType;
		}

		void SetDirty(bool value)
		{
			data().bDirty = value;
		}

		bool IsDirty() const
		{
			return data().bDirty;
		}

		void AttachToShape(Shape::Data& shape)
		{
			Data& d = data();
			d.numAttachedShapes++;
			rpr_int res;
			if (Handle())
			{
				FRW_PRINT_DEBUG("\tShape.AttachMaterial: d: 0x%016llX - numAttachedShapes: %d shape=0x%016llX x_material=0x%016llX", &d, d.numAttachedShapes, shape.Handle(), Handle());
				res = rprShapeSetMaterial(shape.Handle(), Handle());
				checkStatus(res);

				if (d.isShadowCatcher)
				{
					res = rprShapeSetShadowCatcher(shape.Handle(), true);
					if (res != RPR_ERROR_UNSUPPORTED)
					{
						checkStatus(res);
					}
				}

				if (d.isReflectionCatcher)
				{
					res = rprShapeSetReflectionCatcher(shape.Handle(), true);
					checkStatus(res);
				}
			}
		}

		void DetachFromShape(Shape::Data& shape)
		{
			Data& d = data();
			d.numAttachedShapes--;
			FRW_PRINT_DEBUG("\tShape.DetachMaterial: d: 0x%016llX - numAttachedShapes: %d shape=0x%016llX, material=0x%016llX", &d, d.numAttachedShapes, shape.Handle(), Handle());
			if (Handle())
			{
				rpr_int res = rprShapeSetMaterial(shape.Handle(), nullptr);
				checkStatus(res);

				if (d.isShadowCatcher)
				{
					res = rprShapeSetShadowCatcher(shape.Handle(), false);

					if (res != RPR_ERROR_UNSUPPORTED)
					{
						checkStatus(res);
					}
				}
			}
		}

		void AttachToShape(Shape::Data& shape, std::vector<int>& face_ids)
		{
			Data& d = data();
			d.numAttachedShapes++;
			rpr_int res;

			if (!Handle())
				return;

			FRW_PRINT_DEBUG("\tShape.AttachMaterial: d: 0x%016llX - numAttachedShapes: %d shape=0x%016llX x_material=0x%016llX", &d, d.numAttachedShapes, shape.Handle(), Handle());
			res = rprShapeSetMaterialFaces(shape.Handle(), Handle(), face_ids.data(), face_ids.size());
			checkStatus(res);

			if (d.isShadowCatcher)
			{
				res = rprShapeSetShadowCatcher(shape.Handle(), true);
				if (res != RPR_ERROR_UNSUPPORTED)
				{
					checkStatus(res);
				}
			}

			if (d.isReflectionCatcher)
			{
				res = rprShapeSetReflectionCatcher(shape.Handle(), true);
				checkStatus(res);
			}
		}

		void AttachToCurve(frw::Curve::Data& crv)
		{
			Data& d = data();
			d.numAttachedShapes++;

			if (Handle())
			{
				FRW_PRINT_DEBUG("\tShape.AttachMaterial: d: 0x%016llX - numAttachedShapes: %d shape=0x%016llX x_material=0x%016llX", &d, d.numAttachedShapes, shape.Handle(), Handle());
				rpr_int res;
				res = rprCurveSetMaterial(crv.Handle(), Handle());
				checkStatus(res);
			}
		}

		void DetachFromCurve(frw::Curve::Data& crv)
		{
			Data& d = data();
			d.numAttachedShapes--;
			FRW_PRINT_DEBUG("\tShape.DetachMaterial: d: 0x%016llX - numAttachedShapes: %d shape=0x%016llX, material=0x%016llX", &d, d.numAttachedShapes, shape.Handle(), Handle());

			if (Handle())
			{
				rpr_int res;
				res = rprCurveSetMaterial(crv.Handle(), nullptr);
				checkStatus(res);
			}
		}

		void AttachToMaterialInput(rpr_material_node node, rpr_material_node_input inputKey) const
		{
			auto& d = data();
			rpr_int res;
			FRW_PRINT_DEBUG("\tShape.AttachToMaterialInput: node=0x%016llX, material=0x%016llX on %s", node, Handle(), inputKey);
			if (Handle())
			{
				res = rprMaterialNodeSetInputNByKey(node, inputKey, Handle());
				checkStatus(res);
			}

			checkStatus(res);
		}

		void xSetParameterN(rpr_material_node_input parameter, rpr_material_node node)
		{
			const Data& d = data();
			rpr_int res = rprMaterialNodeSetInputNByKey(Handle(), parameter, node);

			if (res == RPR_ERROR_UNSUPPORTED ||
				res == RPR_ERROR_INVALID_PARAMETER)
			{
				// print error/warning if needed
			}
			else
			{
				checkStatus(res);
			}
		}

		void xSetParameterU(rpr_material_node_input parameter, rpr_uint value)
		{
			const Data& d = data();
			rpr_int res = rprMaterialNodeSetInputUByKey(Handle(), parameter, value);
			if (res == RPR_ERROR_UNSUPPORTED ||
				res == RPR_ERROR_INVALID_PARAMETER)
			{
				// print error/warning if needed
			}
			else
			{
				checkStatus(res);
			}

		}
		
		void xSetParameterF(rpr_material_node_input parameter, rpr_float x, rpr_float y, rpr_float z, rpr_float w)
		{
			const Data& d = data();
			rpr_int res = rprMaterialNodeSetInputFByKey(Handle(), parameter, x, y, z, w);
			if (res == RPR_ERROR_UNSUPPORTED ||
				res == RPR_ERROR_INVALID_PARAMETER)
			{
				// print error/warning if needed
			}
			else
			{
				checkStatus(res);
			}
		}

		bool xSetValue(rpr_material_node_input parameter, const Value& v)
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

		void LinkLight(const Light& light)
		{
			AddReference(light);

			rpr_int res = rprMaterialNodeSetInputLightDataByKey(Handle(), RPR_MATERIAL_INPUT_LIGHT, light.Handle());
			if (res == RPR_ERROR_UNSUPPORTED ||
				res == RPR_ERROR_INVALID_PARAMETER)
			{
				// print error/warning if needed
			}
			else
			{
				checkStatus(res);
			}
		}

		void ClearLinkedLight(const Light& light)
		{
			RemoveReference(light);

			rpr_int res = rprMaterialNodeSetInputLightDataByKey(Handle(), RPR_MATERIAL_INPUT_LIGHT, nullptr);
			if (res == RPR_ERROR_UNSUPPORTED ||
				res == RPR_ERROR_INVALID_PARAMETER)
			{
				// print error/warning if needed
			}
			else
			{
				checkStatus(res);
			}
		}

		void SetMaterialId(rpr_uint id)
		{
			rpr_int res = rprMaterialNodeSetID(Handle(), id);

			if (res == RPR_ERROR_UNSUPPORTED ||
				res == RPR_ERROR_INVALID_PARAMETER)
			{
				// should not cancel execution if such error happens
			}
			else
			{
				checkStatus(res);
			}
		}

	};

	class DiffuseShader : public Shader
	{
	public:
		explicit DiffuseShader(const MaterialSystem& h) : Shader(h, ShaderTypeDiffuse) {}

		void SetColor(Value value) { SetValue(RPR_MATERIAL_INPUT_COLOR, value); }
		void SetNormal(Value value) { SetValue(RPR_MATERIAL_INPUT_NORMAL, value); }
		void SetRoughness(Value value) { SetValue(RPR_MATERIAL_INPUT_ROUGHNESS, value); }
	};

	class EmissiveShader : public Shader
	{
	public:
		explicit EmissiveShader(MaterialSystem& h) : Shader(h, ShaderTypeEmissive) {}

		void SetColor(Value value) { SetValue(RPR_MATERIAL_INPUT_COLOR, value); }
	};

	class TransparentShader : public Shader
	{
	public:
		explicit TransparentShader(const MaterialSystem& h) : Shader(h, ShaderTypeTransparent) {}

		void SetColor(Value value) { SetValue(RPR_MATERIAL_INPUT_COLOR, value); }
	};

	class BaseUVNode : public ValueNode
	{
	public:
		BaseUVNode(const MaterialSystem& h, ValueType nodeType) : ValueNode(h, nodeType) {}
		virtual ~BaseUVNode() = default;

		virtual void SetOrigin(const frw::Value& value) = 0;
		void SetInputZAxis(const frw::Value& value);
		void SetInputXAxis(const frw::Value& value);
		void SetInputUVScale(const frw::Value& value);
	};

	class UVProceduralNode : public BaseUVNode
	{
	public:
		UVProceduralNode(const MaterialSystem& h, UVTypeValue type) : BaseUVNode(h, ValueTypeUVProcedural)
		{
			SetValueInt(NodeInputUVType, static_cast<int>(type));
		}

		virtual void SetOrigin(const frw::Value& value) override;
	};

	class UVTriplanarNode : public BaseUVNode
	{
	public:
        UVTriplanarNode(const MaterialSystem& h) : BaseUVNode(h, ValueTypeUVTriplanar) {}

		virtual void SetOrigin(const frw::Value& value) override;
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
			{
				DeleteAndClear();
			}

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
			{
				DeleteAndClear();
			}

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

	inline bool Node::SetValue(rpr_material_node_input key, const Value& v)
	{
		switch (v.type)
		{
			case Value::FLOAT:
				return RPR_SUCCESS == rprMaterialNodeSetInputFByKey(Handle(), key, v.x, v.y, v.z, v.w);
			case Value::NODE:
			{
				if (!v.node)	// in theory we should now allow this, as setting a NULL input is legal (as of FRSDK 1.87)
					return false;
				AddReference(v.node);
				return RPR_SUCCESS == rprMaterialNodeSetInputNByKey(Handle(), key, v.node.Handle());	// should be ok to set null here now
			}
		}
		assert(!"bad type");
		return false;
	}


	inline bool Node::SetValueInt(rpr_material_node_input key, int v)
	{
		return RPR_SUCCESS == rprMaterialNodeSetInputUByKey(Handle(), key, v);
	}

	inline bool Node::SetValueBuffer(rpr_material_node_input key, rpr_buffer buffer)
	{
		return RPR_SUCCESS == rprMaterialNodeSetInputBufferDataByKey(Handle(), key, buffer);
	}

	inline MaterialSystem Node::GetMaterialSystem() const
	{
		return data().materialSystem.As<MaterialSystem>();
	}


	inline Shader MaterialSystem::ShaderBlend(const Shader& a, const Shader& b, const Value& t) const
	{
		// in theory a null could be considered a transparent material, but would create unexpected node
		if (!a.IsValid())
		{
			return b;
		}

		if (!b.IsValid())
		{
			return a;
		}

		Shader node(*this, ShaderTypeBlend);

		node._SetInputNode(RPR_MATERIAL_INPUT_COLOR0, a);
		node._SetInputNode(RPR_MATERIAL_INPUT_COLOR1, b);
		node.SetValue(RPR_MATERIAL_INPUT_WEIGHT, t);

		node.SetDirty(false);

		return node;
	}

	inline Shader MaterialSystem::ShaderDoubleSided(const Shader& a, const Shader& b) const
	{
		if (!a.IsValid())
		{
			return b;
		}

		if (!b.IsValid())
		{
			return a;
		}

		Shader node(*this, ShaderTypeDoubleSided);
		node._SetInputNode(RPR_MATERIAL_INPUT_FRONTFACE, a);
		node._SetInputNode(RPR_MATERIAL_INPUT_BACKFACE, b);

		node.SetDirty(false);

		return node;
	}

	inline Shader MaterialSystem::ShaderAdd(const Shader& a, const Shader& b) const
	{
		if (!a)
			return b;
		if (!b)
			return a;

		Shader node(*this, ShaderTypeAdd);
		node._SetInputNode(RPR_MATERIAL_INPUT_COLOR0, a);
		node._SetInputNode(RPR_MATERIAL_INPUT_COLOR1, b);
		return node;
	}

	inline Value Value::SelectX() const
	{
		const frw::MaterialSystem materialSystem = GetMaterialSystem();
		return materialSystem.ValueSelectX(*this);
	}
	inline Value Value::SelectY() const
	{
		const frw::MaterialSystem materialSystem = GetMaterialSystem();
		return materialSystem.ValueSelectY(*this);
	}
	inline Value Value::SelectZ() const
	{
		const frw::MaterialSystem materialSystem = GetMaterialSystem();
		return materialSystem.ValueSelectZ(*this);
	}
	inline Value Value::SelectW() const
	{
		const frw::MaterialSystem materialSystem = GetMaterialSystem();
		return materialSystem.ValueSelectW(*this);
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

		for (auto it = data().shaders.begin(); it != data().shaders.end(); ++it)
		{
			Shader oldShader = it->As<Shader>();
			RemoveReference(oldShader);
			oldShader.DetachFromShape(data());
		}
		data().shaders.clear();

		AddReference(shader);
		data().shader = shader;
		shader.AttachToShape(data());
	}

	inline Shader Shape::GetShader() const
	{
		return data().shader.As<Shader>();
	}

	// note that old shaders must be removed before this function is called!
	inline void Shape::SetPerFaceShader(Shader shader, std::vector<int>& face_ids)
	{
		AddReference(shader);
		data().shaders.push_back(shader);
		shader.AttachToShape(data(), face_ids);
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

		if (shader.GetShaderType() != ShaderType::ShaderTypeStandard)
		{
			auto res = rprShapeSetVolumeMaterial(Handle(), shader.Handle());

			if (res == RPR_ERROR_UNSUPPORTED)
			{
				if (shader)
				{
					// display
				}
			}
			else
			{
				checkStatus(res);
			}
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

	inline void Shape::SetDisplacementNoSubdiv(Value image, float minscale, float maxscale)
	{
		RemoveDisplacement();

		if (!image.IsNode())
			return;

		Node n = image.GetNode();
		const MaterialSystem& ms = n.GetMaterialSystem();

		MDistance::Unit sceneUnits = MDistance::uiUnit();
		MDistance distance(1.0, sceneUnits);
		float scale_multiplier = (float)distance.asMeters() * 100;
		minscale *= scale_multiplier;
		maxscale *= scale_multiplier;

		ArithmeticNode displaced_material(ms, OperatorMultiply, Value(maxscale, 0.0f, 0.0f), image);

		auto res = rprShapeSetDisplacementMaterial(Handle(), displaced_material.Handle());
		checkStatus(res);
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
		
			MDistance::Unit sceneUnits = MDistance::uiUnit();
			MDistance distance(1.0, sceneUnits);
			float scale_multiplier = (float) distance.asMeters() * 100;
			minscale *= scale_multiplier;
			maxscale *= scale_multiplier;
		
			if (minscale > maxscale)
			{
				// render with no displacement
				res = rprShapeSetDisplacementScale(Handle(), maxscale, minscale);
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

	inline void Shape::SetAdaptiveSubdivisionFactor(float adaptiveFactor, int image_height, rpr_camera camera, rpr_framebuffer frameBuf, bool isRpr20)
	{
		// convert factor from size of subdiv in pixel to RPR
		// RPR wants the subdiv factor as the "number of faces per pixel" 
		// the setting gives user the size of face in number pixels.    
		// rpr internally does: subdiv size in pixel = 2^factor  / 16.0 
		// The log2 is reversing that for us to get the factor we want. 
		// also, guard against 0.  
		
		if (adaptiveFactor < 0.0001f)
		{
			adaptiveFactor = 0.0001f;
		}

		// commenting out the formula but not deleting it in case we will need to return it
		rpr_int calculatedFactor = int(log2(1.0 / adaptiveFactor * 16.0));

		if (!isRpr20)
		{
			rpr_int res = rprShapeAutoAdaptSubdivisionFactor(Handle(), frameBuf, camera, calculatedFactor);
			checkStatusThrow(res, "Unable to set Adaptive Subdivision!");
		}
		else
		{
			float autoRatioCap = 1.0f / image_height;
			rpr_int res = rprShapeSetSubdivisionAutoRatioCap(Handle(), autoRatioCap);
			checkStatusThrow(res, "Unable to set Adaptive Subdivision!");

			res = rprShapeSetSubdivisionFactor(Handle(), calculatedFactor);
			checkStatusThrow(res, "Unable to set Adaptive Subdivision!");
		}
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

	inline DataBuffer::DataBuffer(Context context, const rpr_buffer_desc& bufferDesc, const float* data)
		: Object(nullptr, context, true, new Data())
	{
		FRW_PRINT_DEBUG("CreateDataBuffer()");

		rpr_buffer h = nullptr;
		rpr_int res = rprContextCreateBuffer(context.Handle(), &bufferDesc, data, &h);
		if (checkStatus(res, "Unable to create data buffer!"))
			m->Attach(h);
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
		{
			m->Attach(h);
		}
	}

	inline Image::Image(Context context, const rpr_image_format& format, const rpr_image_desc& image_desc, const void* data)
		: Object(nullptr, context, true, new Data())
	{
		FRW_PRINT_DEBUG("CreateImage()");
		rpr_image h = nullptr;
		auto res = rprContextCreateImage(context.Handle(), format, &image_desc, data, &h);
		if (checkStatus(res, "Unable to create image"))
		{
			m->Attach(h);
		}
	}

	inline Image::Image(Context context, const rpr_image_format& format)
		: Object(nullptr, context, true, new Data())
	{
		FRW_PRINT_DEBUG("CreateImage()");
		rpr_image h = nullptr;
		auto res = rprContextCreateImage(context.Handle(), format, nullptr, nullptr, &h);
		if (checkStatus(res, "Unable to create image"))
		{
			m->Attach(h);
		}
	}


	inline Image::Image(Context context, const char* filename)
		: Object(nullptr, context, true, new Data())
	{
		rpr_image h = nullptr;

		rpr_int res = RPR_SUCCESS;

		// It sometimes throw an exception instead of returning error code for some reason in case if incorrect path was specified
		try
		{
			res = rprContextCreateImageFromFile(context.Handle(), filename, &h);
		}
		catch (...)
		{
			res = RPR_ERROR_INVALID_PARAMETER;
		}

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

		if (res != RPR_ERROR_UNSUPPORTED)
		{
			checkStatus(res);
			AddReference(shape);
		}
	}

	inline void EnvironmentLight::DetachPortal(Shape shape)
	{
		auto res = rprEnvironmentLightDetachPortal(GetContext().GetScene(), Handle(), shape.Handle());

		if (res != RPR_ERROR_UNSUPPORTED)
		{
			checkStatus(res);
			RemoveReference(shape);
		}
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
		if (res == RPR_ERROR_UNSUPPORTED)
		{
			return;
		}
		else
		{
			checkStatus(res);
		}
	}

	inline void Context::Detach(PostEffect post_effect)
	{
		FRW_PRINT_DEBUG("DetachPostEffect(%p)\n", post_effect.Handle());
		auto res = rprContextDetachPostEffect(Handle(), post_effect.Handle());
		if (res == RPR_ERROR_UNSUPPORTED)
		{
			return;
		}
		else
		{
			checkStatus(res);
		}
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
		const rpr_int* tidx_stride, const rpr_int * num_face_vertices, size_t num_faces, rpr_mesh_info* meshAttrArray, const std::string& optionalMeshName) const
	{
		FRW_PRINT_DEBUG("CreateMesh() - %d faces\n", num_faces);
		assert(num_vertices != 0);
		assert(num_faces != 0);

		if (num_vertices == 0 || num_faces == 0)
			return Shape();

		rpr_shape shape = nullptr;

		auto status = rprContextCreateMeshEx2(Handle(),
			vertices, num_vertices, vertex_stride,
			normals, num_normals, normal_stride,
			perVertexFlag, num_perVertexFlags, perVertexFlag_stride,
			numberOfTexCoordLayers, texcoords, num_texcoords, texcoord_stride,
			vertex_indices, vidx_stride,
			normal_indices, nidx_stride, texcoord_indices,
			tidx_stride, num_face_vertices, num_faces, meshAttrArray,
			&shape);

		checkStatusThrow(status, ("Unable to create mesh: " + optionalMeshName).c_str());

		Shape shapeObj (shape, *this);
		shapeObj.SetUVCoordinatesSetFlag(numberOfTexCoordLayers > 0);

		return shapeObj;
	}

	// This method is being used by Northstar volumes
	inline Shape Context::CreateVoidMesh()
	{
		rpr_shape shape = nullptr;

		rpr_mesh_info mesh_properties[16];
		mesh_properties[0] = (rpr_mesh_info)RPR_MESH_VOLUME_FLAG;
		mesh_properties[1] = (rpr_mesh_info)1; // enable the Volume flag for the Mesh
		mesh_properties[2] = (rpr_mesh_info)0;


		auto status = rprContextCreateMeshEx2(Handle(), nullptr, 0, 0, nullptr, 0, 0, nullptr, 0, 0, 0, nullptr, nullptr,
			nullptr, nullptr, 0, nullptr, 0, nullptr, nullptr, nullptr, 0, mesh_properties, &shape);

		checkStatusThrow(status, ("Unable to create mesh"));

		Shape shapeObj(shape, *this);

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
	
	inline void Curve::SetShader(Shader shader)
	{
		if (Shader old = GetShader())
		{
			if (old == shader)	// no change?
				return;

			RemoveReference(old);
			old.DetachFromCurve(data());
		}

		AddReference(shader);
		data().shader = shader;
		shader.AttachToCurve(data());
	}

	inline Shader Curve::GetShader() const
	{
		return data().shader.As<Shader>();
	}

	inline Curve::Data::~Data()
	{
		Shader sh = shader.As<Shader>();
		if (sh.IsValid())
		{
			sh.DetachFromCurve(*this);
		}
	}

	class RPRSContext
	{
		class Data
		{
			friend RPRSContext;

			RPRS_context m_handle;

			bool operator==(void* h) const = delete;

		public:
			Data(void)
				: m_handle(nullptr)
			{
				rpr_int statusContext = rprsCreateContext(&m_handle);
				assert(statusContext == RPR_SUCCESS);
			}

			~Data(void)
			{
				if (!IsValid())
					return;

				rprsDeleteContext(m_handle);
			}

			bool IsValid() const { return m_handle != nullptr; }

			RPRS_context Handle() const { return m_handle; }
		};
		typedef std::shared_ptr<Data> DataPtr;

		DataPtr m; // never null

	protected:
		RPRSContext(DataPtr p) : m(p) {}

	public:
		RPRSContext(Data* data = nullptr)
		{
			if (!data) data = new Data();

			m.reset(data);
		}

		void Reset()
		{
			m = std::make_shared<Data>();
		}

		bool operator==(const RPRSContext& rhs) const { return m.get() == rhs.m.get(); }

		long UseCount() const { return m.use_count(); }
		RPRS_context Handle() const { return m->Handle(); }

		// for easier scope and creation flow
		explicit operator bool() const { return m->IsValid(); }
		bool IsValid() const { return m->IsValid(); }
	};
}
