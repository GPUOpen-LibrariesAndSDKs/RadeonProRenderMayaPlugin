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
#include "MeshTranslator.h"

#include <maya/MObject.h>
#include <maya/MFnCamera.h>
#include <maya/MMatrix.h>
#include <maya/MColor.h>
#include <maya/MObjectArray.h>
#include <maya/MFnNurbsSurface.h>
#include <cassert>
#include <vector>

#define CM_2_M 0.01

#define LIGHT_SCALE 2.5f

// Forward declarations
class SkyBuilder;
struct PhysicalLightData;

struct FrElement
{
	frw::Shape	shape;
	frw::Shader shader;
	frw::Shader volumeShader;
	MObject		shadingEngine;

	std::array<float, 16> TM; // extra transformation matrix
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

	void SetLightGroupId(rpr_uint id)
	{
		if (isAreaLight)
		{
			areaLight.SetLightGroupId(id);
		}
		else
		{
			light.SetLightGroupId(id);
		}
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

	bool translateCamera(frw::Camera& frcamera, const MObject& camera, const MMatrix& matrix, bool isRenderView, float aspectRatio = 0.0, bool useAspectRatio = false, int cameraType = 0);

	bool translateLight(FrLight& frlight, Scope& scope, frw::Context frcontext, const MObject& object, const MMatrix& matrix, bool update = false);

	bool translateAreaLightInternal(FrLight& frlight, Scope& scope, frw::Context frcontext, const MObject& object,
											const MMatrix& matrix, const MDagPath& dagPath, const PhysicalLightData& areaLightData, bool update);

	bool translateEnvLight(frw::EnvironmentLight& frlight, frw::Image& frImage, frw::Context frcontext, FireMaya::Scope &scope, const MObject& object, const MMatrix& matrix, bool update = false);

	bool translateSky(frw::EnvironmentLight& envLight, frw::DirectionalLight& sunLight, frw::Image& frImage, SkyBuilder& skyBuilder, frw::Context frcontext, const MObject& object, const MMatrix& matrix, bool update = false);

	bool translateVrayLight(FrLight& frlight, frw::MaterialSystem frMatsys, frw::Context frcontext, const MObject& object, const MMatrix& matrix, bool update = false);

	void ScaleMatrixFromCmToMFloats(const MMatrix& matrix, float floats[4][4]);
	void ScaleMatrixFromCmToM(MMatrix& matrix);

	void FillLightData(PhysicalLightData& physicalLightData, const MObject& object, Scope& scope);

	void GetMatrixForTheNextFrame(const MFnDependencyNode& nodeFn, float matrixFloats[4][4]);
	void CalculateMotionBlurParams(const MFnDependencyNode& nodeFn, const MMatrix& inMatrix, MVector& outLinearMotion, MVector& outAngularMotion, double& outRotationAngle);

	template<typename T,
		// Enable this function for floating point types only
		class = std::enable_if_t<std::is_floating_point<T>::value>>
	inline T deg2rad(T degrees)
	{
		return (static_cast<T>(M_PI) / static_cast<T>(180)) * degrees;
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
