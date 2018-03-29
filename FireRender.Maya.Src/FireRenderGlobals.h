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

#include <maya/MPxNode.h>
#include "frWrap.h"
#include "FireMaya.h"

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

	virtual MStatus compute(const MPlug& plug, MDataBlock& data);

	static MTypeId FRTypeID() { return FireMaya::TypeId::FireRenderGlobals; }

	static void* creator();

	static MStatus initialize();

	static void createDenoiserAttributes();

	/** Return the FR camera mode that matches the given camera type. */
	static frw::CameraMode getCameraModeForType(CameraType type, bool defaultIsOrtho = false);
	// Ground
	static MObject m_useGround;
	static MObject m_groundHeight;
	static MObject m_groundRadius;
	static MObject m_groundShadows;
	static MObject m_groundReflections;
	static MObject m_groundStrength;
	static MObject m_groundRoughness;

	// Render stamp
	static MObject m_useRenderStamp;
	static MObject m_renderStampText;
	static MObject m_renderStampTextDefault; // default value for m_renderStampText
};
