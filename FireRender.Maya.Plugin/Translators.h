#pragma once
// Copyright (C) AMD
//
// File: Translators.h
//
// Translator functions
//
// Created by Alan Stanzione.
//
#include "frWrap.h"
#include <maya/MObject.h>
#include <maya/MFnCamera.h>
#include <maya/MMatrix.h>
#include <maya/MColor.h>
#include <maya/MObjectArray.h>
#include <maya/MFnNurbsSurface.h>
#include "FireMaya.h"
#include <cassert>
#include <vector>

#define CM_2_M 0.01

#define LIGHT_SCALE 2.5

// Forward declarations
class SkyBuilder;

struct FrElement
{
	frw::Shape	shape;
	frw::Shader shader;
	frw::Shader volumeShader;
	MObject		shadingEngine;
};

class FrLight
{
public:
	FrLight() :
		scaleX(1.0f),
		scaleY(1.0f),
		scaleZ(1.0f),
		isAreaLight(false)
	{}

	FrLight(const FrLight& other)
	{
		isAreaLight = other.isAreaLight;
		if (isAreaLight)
			areaLight = other.areaLight;
		else
			light = other.light;
		emissive = other.emissive;
		transparent = other.transparent;
		scaleX = other.scaleX;
		scaleY = other.scaleY;
		scaleZ = other.scaleZ;
	}

	FrLight& operator=(const FrLight& other)
	{
		isAreaLight = other.isAreaLight;
		if (isAreaLight)
			areaLight = other.areaLight;
		else
			light = other.light;
		emissive = other.emissive;
		transparent = other.transparent;
		scaleX = other.scaleX;
		scaleY = other.scaleY;
		scaleZ = other.scaleZ;
		return *this;
	}

	frw::Image image;
	frw::Light light;
	frw::Shape areaLight;
	frw::Shader emissive;
	frw::Shader transparent;

	float scaleX;

	float scaleY;

	float scaleZ;

	bool isAreaLight;
};

namespace FireMaya
{
	bool getInputColorConnection(const MPlug& colorPlug, MPlug& connectedPlug);

	/** Tessellate a NURBS surface and return the resulting mesh object. */
	MObject tessellateNurbsSurface(const MObject& object, const MObject& parent, MStatus& status);

	bool translateCamera(frw::Camera& frcamera, const MObject& camera, const MMatrix& matrix, bool isRenderView, float aspectRatio = 0.0, bool useAspectRatio = false, int cameraType = 0);

	std::vector<frw::Shape> TranslateMesh(frw::Context context, const MObject& originalObject);

	bool translateLight(FrLight& frlight, frw::MaterialSystem frMatsys, frw::Context frcontext, const MObject& object, const MMatrix& matrix, bool update = false);

	bool translateEnvLight(frw::EnvironmentLight& frlight, frw::Image& frImage, frw::Context frcontext, FireMaya::Scope &scope, const MObject& object, const MMatrix& matrix, bool update = false);

	bool translateSky(frw::EnvironmentLight& envLight, frw::DirectionalLight& sunLight, frw::Image& frImage, SkyBuilder& skyBuilder, frw::Context frcontext, const MObject& object, const MMatrix& matrix, bool update = false);

	bool translateVrayLight(FrLight& frlight, frw::MaterialSystem frMatsys, frw::Context frcontext, const MObject& object, const MMatrix& matrix, bool update = false);

	inline double rad2deg(double radians)
	{
		return (180.0 / M_PI)*radians;
	}

	inline double deg2rad(double degrees)
	{
		return (M_PI / 180.0)*degrees;
	}

	inline double limit_degrees180pm(double degrees)
	{
		double limited;

		degrees /= 360.0;
		limited = 360.0*(degrees - floor(degrees));
		if (limited < -180.0) limited += 360.0;
		else if (limited >  180.0) limited -= 360.0;

		return limited;
	}

	bool setEnvironmentIBL(frw::EnvironmentLight& frlight, frw::Context frcontext, frw::Image frImage, float intensity, const MMatrix& matrix, bool update);
	bool setEnvironmentAmbientLight(frw::EnvironmentLight& frlight, frw::Context frcontext, float intensity, MColor colorTemp, const MMatrix& matrix, bool update);
}
