#pragma once
//
// Copyright (C) AMD
//
// File: FireRenderGlobals.h
//
// FireRender globals node
//
// Created by Alan Stanzione.
//

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
		kReinhard02
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

	enum DenoiserType {
		kBilateral,
		kLWR,
		kEAW
	};

	FireRenderGlobals();

	virtual ~FireRenderGlobals();

	void postConstructor() override;

	virtual MStatus compute(const MPlug& plug, MDataBlock& data);

	static MTypeId FRTypeID() { return FireMaya::TypeId::FireRenderGlobals; }

	static void* creator();

	static MStatus initialize();

	static void createDenoiserAttributes();

    static void createFinalRenderAttributes();
	static void createViewportAttributes();
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
