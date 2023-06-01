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
#include "FireMaya.h"

#include <maya/MPxNode.h>
#include <maya/MFnEnumAttribute.h>

class FireRenderGlobals : public MPxNode
{
public:
	enum CompletionCriteriaType
	{
		kIterations = 0,
		kTime,
		kUnlimited,
	};

	enum RenderModeType {
		kGlobalIllumination = 1,
		kDirectIllumination,
		kDirectIlluminationNoShadow,
		kWireframe,
		kMaterialId,//this is actually material id, waiting to be renamed in the Core API
		kPosition,
		kNormal,
		kTexcoord,
		kAmbientOcclusion
	};

	enum PixelFilterType {
		kBoxFilter = 1,
		kTriangleFilter = 2,
		kGaussianFilter = 3,
		kMitchellFilter = 4,
		kLanczosFilter = 5,
		kBlackmanHarrisFilter = 6
	};

	enum ToneMappingType {
		kNone = 0,
		kLinear,
		kPhotolinear,
		kAutolinear,
		kMaxWhite,
		kReinhard02,
		kSimple
	};

	enum ResourceType {
		kGPU = 0,
		kCPU,
		kGPUnCPU,
	};

	enum CameraType {
		kCameraDefault = 0,
		kSphericalPanorama,
		kSphericalPanoramaStereo,
		kCubeMap,
		kCubeMapStereo
	};

	//Please don't change position of kML item, its index used to properly turn off ML denoiser if unsupported by CPU
	enum DenoiserType {
		kBilateral,
		kLWR,
		kEAW,
		kML
	};

	enum PtDenoiserType {
		kNoPtDenoiser = 0,
		kSVGF,
		kASVGF
	};

	enum FSRType {
		kNoFSR = 0,
		kUltraFSR = 1u,
		kQualityFSR = 2u,
		kBalanceFSR = 3u,
		kPerformanceFSR = 4u,
		kUltraPerformanceFSR = 5u
	};

	enum RestirGIBiasCorrection {
		kNoBiasCorrection = 0,
		kUniformWeights,
		kStochasticMIS,
		kDeterministicMIS
	};

	enum ReservoirSampling {
		kDisabled = 0,
		kScreenSpace,
		kWorldSpace
	};

	FireRenderGlobals();

	virtual ~FireRenderGlobals();

	void postConstructor() override;

	virtual MStatus compute(const MPlug& plug, MDataBlock& data);

	static MTypeId FRTypeID() { return FireMaya::TypeId::FireRenderGlobals; }

	static void* creator();

	static MStatus initialize();

	static void addRenderQualityModes(MFnEnumAttribute& eAttr);

	static void createDenoiserAttributes();
	static void createAirVolumeAttributes();
	static void createCryptomatteAttributes();

    static void createFinalRenderAttributes();
	static void createViewportAttributes();
	static void createCompletionCriteriaAttributes();
	static void createTileRenderAttributes();

	static void createContourEffectAttributes();

	static MObject createRenderModeAttr(const char* attrName, const char* attrNameShort, MFnEnumAttribute& eAttr);

	/** Return the FR camera mode that matches the given camera type. */
	static frw::CameraMode getCameraModeForType(CameraType type, bool defaultIsOrtho = false);

	// Render stamp
	static MObject m_useRenderStamp;
	static MObject m_renderStampText;
	static MObject m_renderStampTextDefault; // default value for m_renderStampText

private:
	static void onAttributeChanged(MNodeMessage::AttributeMessage msg, MPlug &plug, MPlug &otherPlug, void *clientData);
	static void addAsGlobalAttribute(MFnAttribute& attr);

	static void createLegacyAttributes();
	static void setupProductionRayDepthParameters();
	static void setupRenderDevices();

	void syncGlobalAttributeValues();

private:
	// attributes which will be serialized via optionVar commands instead of usual node's attribute serialization
	typedef std::list<MObject> GlobalAttributesList;
	static GlobalAttributesList m_globalAttributesList;

	MCallbackId m_attributeChangedCallback;
};
