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
#include "Translators.h"
#include "../Context/FireRenderContext.h"

#include <maya/MPlug.h>
#include <maya/MDagPath.h>
#include <maya/MPlugArray.h>
#include <maya/MDistance.h>
#include <maya/MFnLight.h>
#include <maya/MFnSpotLight.h>
#include <maya/MRenderUtil.h>
#include <maya/MFloatMatrix.h>
#include <maya/M3dView.h>
#include <maya/MGlobal.h>
#include <maya/MImage.h>
#include <maya/MDGContext.h>
#include <maya/MTime.h>
#include <maya/MFileObject.h>
#include <maya/MTransformationMatrix.h>
#include <maya/MQuaternion.h>
#include <maya/MAnimControl.h>
#include <maya/MEulerRotation.h>
#include <maya/MFnMatrixData.h>


#include <functional>
#include <map>
#include <string>
#include <cfloat>

#include "FireMaya.h"
#include "FireRenderObjects.h"
#include "SkyBuilder.h"
#include "DependencyNode.h"
#include "maya/MTransformationMatrix.h"
#include "VRay.h"
#include "FireRenderUtils.h"
#include "FireRenderGlobals.h"
#include "FireRenderMath.h"
#include "FireRenderThread.h"

#include "PhysicalLightAttributes.h"
#include "PhysicalLightGeometryUtility.h"

namespace
{
	//	#define PROFILE 1			// profile mesh loading code

	// This code gives ~20% performance improvement in release, and ~10x boost for debug build.
	// Restrictions compared to original implementation:
	// - reserve() is required, must fit entire data
	// - works only with POD types.
	template <class T>
	class FastVector
	{
		size_t m_reserved = 0;
		size_t m_count = 0;
		T*     m_data = nullptr;

	public:
		~FastVector()
		{
			if (m_data) free(m_data);
		}

		void reserve(size_t n)
		{
			assert(!m_data);
			m_data = (T*)malloc(sizeof(T) * n);
			m_reserved = n;
			m_count = 0;
		}

		T& operator[](size_t index)
		{
			return *(m_data + index);
		}

		const T& operator[](size_t index) const
		{
			return *(m_data + index);
		}

		T* data() { return m_data; }
		const T* data() const { return m_data; }
		const size_t size() const { return m_count; }

		// returns index
		int Add(const T& v)
		{
			assert(m_data != nullptr && m_count < m_reserved);
			size_t index = m_count++;
			m_data[index] = v;
			return int(index);
		}
	};
}

namespace FireMaya
{
	bool translateCamera(frw::Camera& frw_camera, const MObject& camera, const MMatrix& matrix, bool isRenderView, float aspectRatio, bool useAspectRatio, int cameraType)
	{
		rpr_int frstatus;
		MStatus mstatus;
		MFnCamera fnCamera(camera);

		auto frcamera = frw_camera.Handle();

		bool showFilmGate = fnCamera.isDisplayFilmGate();
		float overscan = isRenderView ? 1.0f : float(fnCamera.overscan());

		DebugPrint("\tDisplay Film Gate: %d", showFilmGate);
		DebugPrint("\tOverscan: %f", overscan);

		auto cameraMode = FireRenderGlobals::getCameraModeForType(FireRenderGlobals::CameraType(cameraType), fnCamera.isOrtho());
		frw_camera.SetMode(cameraMode);

		switch (cameraMode)
		{
		case frw::CameraModeOrthographic:
		{
			float cs = static_cast<float>(fnCamera.cameraScale(&mstatus));
			assert(mstatus == MStatus::kSuccess);
			float ow = static_cast<float>(fnCamera.orthoWidth(&mstatus) * overscan);
			assert(mstatus == MStatus::kSuccess);
			float ps = static_cast<float>(fnCamera.preScale(&mstatus));
			assert(mstatus == MStatus::kSuccess);

			frstatus = rprCameraSetOrthoWidth(frcamera, float(CM_2_M * ps * ow) / cs);
			checkStatus(frstatus);

			frstatus = rprCameraSetOrthoHeight(frcamera, float(CM_2_M * ps * ow) / (cs * aspectRatio));
			checkStatus(frstatus);

		} break;

		case frw::CameraModePanorama: //break;
		case frw::CameraModePanoramaStereo: //break;
		case frw::CameraModeCubemap: //break;
		case frw::CameraModeCubemapStereo: //break;
		case frw::CameraModePerspective: // break
		default:
		{
			frstatus = rprCameraSetFocalLength(frcamera, static_cast<float>(fnCamera.focalLength(&mstatus)));
			checkStatus(frstatus);

			// convert fnCamera.focusDistance() in cm to m
			float focusDistance = static_cast<float>(fnCamera.focusDistance(&mstatus) * GetSceneUnitsConversionCoefficient());
			checkStatus(frstatus);

			frstatus = rprCameraSetFocusDistance(frcamera, focusDistance);
			checkStatus(frstatus);

		}	break;
		}

		// fStop == FLT_MAX is equivalent to disable DoF
		float fStop = FLT_MAX;
		if (fnCamera.isDepthOfField(&mstatus))
		{
			fStop = static_cast<float>(fnCamera.fStop(&mstatus));
			assert(mstatus == MStatus::kSuccess);
		}

		frstatus = rprCameraSetFStop(frcamera, fStop);
		checkStatus(frstatus);

		// convert from inch to mm
		float apertureWidth = static_cast<float>(fnCamera.horizontalFilmAperture(&mstatus)) * 25.4f;
		float apertureHeight = static_cast<float>(fnCamera.verticalFilmAperture(&mstatus)) * 25.4f;
		assert(mstatus == MStatus::kSuccess);

		apertureWidth *= overscan;
		apertureHeight *= overscan;

		if (useAspectRatio)
		{
			MFnCamera::FilmFit filmFit = fnCamera.filmFit();
			switch (filmFit)
			{
			case MFnCamera::kFillFilmFit:
			{
				if (aspectRatio < apertureWidth / apertureHeight)
				{
					apertureWidth = apertureHeight * aspectRatio;
					frstatus = rprCameraSetSensorSize(frcamera, apertureWidth, apertureHeight);
				}
				else
				{
					apertureHeight = apertureWidth / aspectRatio;
					frstatus = rprCameraSetSensorSize(frcamera, apertureWidth, apertureHeight);
				}
				break;
			}
			case MFnCamera::kHorizontalFilmFit:
			{
				apertureHeight = apertureWidth / aspectRatio;
				frstatus = rprCameraSetSensorSize(frcamera, apertureWidth, apertureHeight);
				break;
			}
			case MFnCamera::kVerticalFilmFit:
			{
				apertureWidth = apertureHeight * aspectRatio;
				frstatus = rprCameraSetSensorSize(frcamera, apertureWidth, apertureHeight);
				break;
			}
			default:
				float frApertureHeight = apertureWidth / aspectRatio;
				frstatus = rprCameraSetSensorSize(frcamera, apertureWidth, frApertureHeight);
			}
		}
		else
		{
			frstatus = rprCameraSetSensorSize(frcamera, apertureWidth, apertureHeight);
		}

		checkStatus(frstatus);

		SetCameraLookatForMatrix(frcamera, matrix);

		//Check aperture and blades
		MPlug bladesPlug = fnCamera.findPlug("fireRenderApertureBlades");
		if (!bladesPlug.isNull())
		{
			int num_blades = bladesPlug.asInt();
			frstatus = rprCameraSetApertureBlades(frcamera, num_blades);
			checkStatus(frstatus);
		}

		MPlug exposurePlug = fnCamera.findPlug("fireRenderExposure");
		if (!exposurePlug.isNull())
		{
			float exposure = exposurePlug.asFloat();
			frstatus = rprCameraSetExposure(frcamera, exposure);
			checkStatus(frstatus);
		}

		// Setting Near and Far clipping plane
		frstatus = rprCameraSetNearPlane(frcamera, (float)(fnCamera.nearClippingPlane() * GetSceneUnitsConversionCoefficient()));
		checkStatus(frstatus);

		frstatus = rprCameraSetFarPlane(frcamera, (float)(fnCamera.farClippingPlane() * GetSceneUnitsConversionCoefficient()));
		checkStatus(frstatus);
		
		float filmOffsetX = (float) fnCamera.horizontalFilmOffset(&mstatus);
		assert(mstatus == MStatus::kSuccess);

		float filmOffsetY = (float) fnCamera.verticalFilmOffset(&mstatus);
		assert(mstatus == MStatus::kSuccess);

		float inchesToMM = 25.4f;
		frstatus = rprCameraSetLensShift(frcamera, inchesToMM * filmOffsetX / apertureWidth, inchesToMM * filmOffsetY / apertureHeight);
		if (frstatus != RPR_ERROR_UNSUPPORTED)
		{
			checkStatus(frstatus);
		}

		// set rpr name
		frw_camera.SetName(fnCamera.name().asChar());
		
		return true;
	}

	void GetMatrixForTheNextFrame(const MFnDependencyNode& nodeFn, float matrixFloats[4][4], unsigned int dagPathIndex)
	{
		MTime nextTime = MAnimControl::currentTime();
		MTime maxTime = MAnimControl::maxTime();

		if (MAnimControl::currentTime() != maxTime)
		{
			nextTime++;
		}

		MDGContext dgcontext(nextTime);
		MObject val;
		MPlug matrixPlug = nodeFn.findPlug("worldMatrix");

		if (matrixPlug.isNull())
			return;

		matrixPlug = matrixPlug.elementByLogicalIndex(dagPathIndex);
		matrixPlug.getValue(val, dgcontext);
		MMatrix nextFrameMatrix = MFnMatrixData(val).matrix();

		ScaleMatrixFromCmToMFloats(nextFrameMatrix, matrixFloats);
	}

	void CalculateMotionBlurParams(const MFnDependencyNode& nodeFn, 
									const MMatrix& inMatrix, 
									MVector& outLinearMotion, 
									MVector& outAngularMotion, 
									double& outRotationAngle, 
									unsigned int dagPathIndex)
	{
		// convert Maya mesh in cm to m
		MMatrix scaleM;
		scaleM.setToIdentity();
		scaleM[0][0] = scaleM[1][1] = scaleM[2][2] = GetSceneUnitsConversionCoefficient();
		MMatrix matrix = inMatrix;
		matrix *= scaleM;

		MTime nextTime = MAnimControl::currentTime();
		MTime maxTime = MAnimControl::maxTime();

		if (MAnimControl::currentTime() != maxTime)
		{
			nextTime++;
			MDGContext dgcontext(nextTime);
			MObject val;
			MPlug matrixPlug = nodeFn.findPlug("worldMatrix");
			matrixPlug = matrixPlug.elementByLogicalIndex(dagPathIndex);
			matrixPlug.getValue(val, dgcontext);
			MMatrix nextFrameMatrix = MFnMatrixData(val).matrix();
			if (nextFrameMatrix != matrix)
			{
				MMatrix nextMatrix = nextFrameMatrix;
				nextMatrix *= scaleM;

				// get linear motion
				outLinearMotion = MVector(nextMatrix[3][0] - matrix[3][0], nextMatrix[3][1] - matrix[3][1], nextMatrix[3][2] - matrix[3][2]);

				MTransformationMatrix transformationMatrix(matrix);
				MQuaternion currentRotation = transformationMatrix.rotation();

				MTransformationMatrix transformationMatrixNext(nextMatrix);
				MQuaternion nextRotation = transformationMatrixNext.rotation();

				MQuaternion dispRotation = currentRotation.inverse() * nextRotation;

				dispRotation.getAxisAngle(outAngularMotion, outRotationAngle);

				if (outRotationAngle > PI)
				{
					outRotationAngle -= 2 * PI;
				}
			}
		}

	}

	bool translateLight(FrLight& frlight, Scope& scope, frw::Context frcontext, const MObject& object, const MMatrix& matrix, bool update)
	{
		rpr_int frstatus;
		MStatus mstatus;

		MFnDagNode dagNode(object);
		MDagPath dagPath;
		mstatus = dagNode.getPath(dagPath);
		assert(mstatus == MStatus::kSuccess);

		if (!update)
		{
			frlight.isAreaLight = false;
			if (frlight.areaLight)
				frlight.areaLight.SetAreaLightFlag(false);
		}

		PhysicalLightData lightData;
		FillLightData(lightData, object, scope);

		if (!lightData.enabled)
		{
			return true;
		}
	
		bool ret = true;
		if (lightData.lightType == PLTArea)
		{
			ret = translateAreaLightInternal(frlight, scope, frcontext, object, matrix, dagPath, lightData, update);
		}
		else
		{
			MColor color = lightData.GetChosenColor() * lightData.GetCalculatedIntensity();

			float mfloats[4][4];
			ScaleMatrixFromCmToMFloats(matrix, mfloats);

			switch (lightData.lightType)
			{
				case PLTSpot:
				{
					if (!update)
					{
						frlight.light = frcontext.CreateSpotLight();
					}
					frstatus = rprSpotLightSetConeShape(frlight.light.Handle(), lightData.spotInnerAngle, lightData.spotOuterFallOff);
					checkStatus(frstatus);

					frstatus = rprSpotLightSetRadiantPower3f(frlight.light.Handle(), color.r, color.g, color.b);
					checkStatus(frstatus);
					break;
				}

				case PLTDirectional:
				{
					if (!update)
					{
						frlight.light = frcontext.CreateDirectionalLight();
					}
					frstatus = rprDirectionalLightSetRadiantPower3f(frlight.light.Handle(), color.r, color.g, color.b);
					checkStatus(frstatus);

					float softnessAngle = toRadians(lightData.shadowsEnabled ? lightData.shadowsSoftnessAngle : 0.0f);

					frstatus = rprDirectionalLightSetShadowSoftnessAngle(frlight.light.Handle(), softnessAngle);
					checkStatus(frstatus);

					break;
				}

				case PLTPoint:
				{
					if (!update)
					{
						frlight.light = frcontext.CreatePointLight();
					}

					frstatus = rprPointLightSetRadiantPower3f(frlight.light.Handle(), color.r, color.g, color.b);
					checkStatus(frstatus);
					break;
				}
				case PLTSphere:
					if (!update)
					{
						frlight.light = frcontext.CreateSphereLight();
					}

					frstatus = rprSphereLightSetRadiantPower3f(frlight.light.Handle(), color.r, color.g, color.b);
					checkStatus(frstatus);

					frstatus = rprSphereLightSetRadius(frlight.light.Handle(), lightData.sphereRadius);
					checkStatus(frstatus);

					{
						// Maya let user to set only up to 3 digits after point in scale parameter of transform.
						// So we need to take only these 3 digits into account
						int revert_precision = 1000;
						
						long long scalex = (long long) (matrix[0][0] * revert_precision);
						long long scaley = (long long) (matrix[1][1] * revert_precision);
						long long scalez = (long long) (matrix[2][2] * revert_precision);

						if (scalex != scaley || scalex != scalez)
						{
							MGlobal::displayWarning("Non-uniform scaling for spehre light detected. Might lead to graphical artifacts!");
						}
					}
					break;

				case PLTDisk:
					if (!update)
					{
						frlight.light = frcontext.CreateDiskLight();
					}

					frstatus = rprDiskLightSetRadiantPower3f(frlight.light.Handle(), color.r, color.g, color.b);
					checkStatus(frstatus);

					frstatus = rprDiskLightSetRadius(frlight.light.Handle(), lightData.diskRadius);
					checkStatus(frstatus);

					frstatus = rprDiskLightSetAngle(frlight.light.Handle(), lightData.diskAngle);
					checkStatus(frstatus);
					break;

				case PLTUnknown:
					return false;
			}

			frlight.light.SetTransform((rpr_float*)mfloats);
		}

		const char* lightName = MFnDependencyNode(dagPath.transform()).name().asChar();
		if (frlight.light)
		{
			frlight.light.SetName(lightName);
		}
		else if (frlight.areaLight)
		{
			frlight.areaLight.SetName(lightName);
		}

		MPlug plug = dagNode.findPlug("RPRLightGroup");

		if (!plug.isNull())
		{
			const int maxLightGroupId = 3;
			int lightGroupId = plug.asInt();

			if (lightGroupId >= 0 && lightGroupId <= maxLightGroupId)
			{
				frlight.SetLightGroupId((rpr_uint) lightGroupId);
			}
		}

		return ret;
	}

	void FillLightData(PhysicalLightData& physicalLightData, const MObject& node, Scope& scope)
	{
		// Maya's light
		if (node.hasFn(MFn::kLight))
		{
			MStatus mstatus;
			MFnLight fnLight(node);

			physicalLightData.enabled = true;
			physicalLightData.colorMode = PLCColor;
			physicalLightData.intensityUnits = PLTIUWatts;
			physicalLightData.luminousEfficacy = PhysicalLightData::defaultLuminousEfficacy;
			physicalLightData.colorBase = fnLight.color();

			float mayaIntensity = fnLight.intensity(&mstatus);
			assert(mstatus == MStatus::kSuccess);

			if (node.apiType() == MFn::kAreaLight)
			{
				// For area light intensity would be used later
				// Formula was deduced manually to give render results closest to Maya Software render
				if (mayaIntensity <= std::numeric_limits<float>::epsilon())
				{
					physicalLightData.intensity = 0.0f;
				}
				else
				{
					physicalLightData.intensity = 0.00794f * mayaIntensity + 0.427324f;
				}
				physicalLightData.resultFrwColor = frw::Value(
					physicalLightData.colorBase.r, 
					physicalLightData.colorBase.g, 
					physicalLightData.colorBase.b
				);
			}
			else
			{
				physicalLightData.intensity = mayaIntensity * LIGHT_SCALE;
				physicalLightData.resultFrwColor = frw::Value(
					physicalLightData.colorBase.r, 
					physicalLightData.colorBase.g, 
					physicalLightData.colorBase.b
				) * physicalLightData.intensity;
			}

			physicalLightData.areaWidth = 1.0f;
			physicalLightData.areaLength = 1.0f;

			physicalLightData.shadowsEnabled = true;
			physicalLightData.shadowsSoftnessAngle = 0.0f;

			assert(mstatus == MStatus::kSuccess);

			switch (node.apiType())
			{
				case MFn::kAreaLight:
					physicalLightData.lightType = PLTArea;
					physicalLightData.areaLightShape = PLARectangle;
					break;

				case MFn::kSpotLight:
				{
					physicalLightData.lightType = PLTSpot;

					MFnSpotLight fnSpotLight(node);
					double coneAngle = fnSpotLight.coneAngle(&mstatus) * 0.5;
					assert(mstatus == MStatus::kSuccess);

					double penumbraAngle = coneAngle + fnSpotLight.penumbraAngle(&mstatus);
					assert(mstatus == MStatus::kSuccess);

					physicalLightData.spotInnerAngle = (float)coneAngle;
					physicalLightData.spotOuterFallOff = (float)penumbraAngle;
					break;
				}

				case MFn::kDirectionalLight:
					physicalLightData.lightType = PLTDirectional;
					break;

				case MFn::kPointLight:
					physicalLightData.lightType = PLTPoint;
					break;

				case MFn::kAmbientLight:
					physicalLightData.lightType = PLTUnknown;
					break;

				case MFn::kVolumeLight:
					physicalLightData.lightType = PLTUnknown;
					break;
			}
		}
		// RPR physical light
		else 
		{
			PhysicalLightAttributes::FillPhysicalLightData(physicalLightData, node, &scope);
		}
	}

	bool translateAreaLightInternal(FrLight& frlight, 
									Scope& scope,
									frw::Context frcontext, 
									const MObject& lightObject, 
									const MMatrix& matrix,
									const MDagPath& dagPath,
									const PhysicalLightData& areaLightData,
									bool update)
	{
		MFnDependencyNode depNode(lightObject);

		MTransformationMatrix trm;

		double scale[3]{ areaLightData.areaWidth, areaLightData.areaWidth, areaLightData.areaLength };
		trm.setScale(scale, MSpace::Space::kObject);

		MMatrix transformMatrix = trm.asMatrix() * matrix;

		float calculatedArea = 1.0f;
		if (update && !frlight.areaLight)
		{
			return false;
		}
		else
		{
            frw::MaterialSystem matSys = scope.MaterialSystem();
			if (!frlight.emissive)
				frlight.emissive = frw::EmissiveShader(matSys);

			if (!frlight.transparent)
				frlight.transparent = frw::EmissiveShader(matSys);

			if (areaLightData.areaLightShape != PLAMesh)
			{
				frlight.areaLight = PhysicalLightGeometryUtility::CreateShapeForAreaLight(areaLightData.areaLightShape, frcontext);
				calculatedArea = PhysicalLightGeometryUtility::GetAreaOfMeshPrimitive(areaLightData.areaLightShape, transformMatrix);
			}
			else
			{
				MDagPath shapeDagPath;
				if (!findMeshShapeForMeshPhysicalLight(dagPath, shapeDagPath))
				{
					return false; // no mesh found
				}

				std::vector<int> faceMaterialIndices;
				frw::Shape shape = MeshTranslator::TranslateMesh(frcontext, shapeDagPath.node(), faceMaterialIndices);
				if (shape)
				{
					frlight.areaLight = shape;
				}
				else
				{
					// bad node probably
					return false;
				}

				transformMatrix = trm.asMatrix() * shapeDagPath.inclusiveMatrix();
				calculatedArea = PhysicalLightGeometryUtility::GetAreaOfMesh(shapeDagPath.node(), transformMatrix);
			}

			frlight.areaLight.SetShader(frlight.emissive);
			frlight.areaLight.SetAreaLightFlag(true);
			frlight.isAreaLight = true;
		}

		if (frlight.areaLight)
		{
			float matrixfloats[4][4];
			ScaleMatrixFromCmToMFloats(transformMatrix, matrixfloats);

			frlight.areaLight.SetTransform((rpr_float*)matrixfloats);
			if (frlight.emissive)
			{
				frlight.emissive.SetValue(RPR_MATERIAL_INPUT_COLOR,
					areaLightData.resultFrwColor * areaLightData.GetCalculatedIntensity(calculatedArea));
			}

			bool shapeVisibility = false;
			MPlug shapeVisibilityPlug = depNode.findPlug("primaryVisibility");
			if (!shapeVisibilityPlug.isNull())
			{
				shapeVisibilityPlug.getValue(shapeVisibility);
			}

			shapeVisibility = shapeVisibility && areaLightData.areaLightVisible;

			frlight.areaLight.SetPrimaryVisibility(shapeVisibility);
			frlight.areaLight.SetShadowFlag(shapeVisibility && areaLightData.shadowsEnabled);
			frlight.areaLight.SetLightShapeVisibilityEx(shapeVisibility);
		}

		return true;
	}

	void ScaleMatrixFromCmToMFloats(const MMatrix& matrix, float floats[4][4])
	{
		MStatus mstatus;

		MMatrix m = matrix;
		// convert Maya mesh in cm to m

		ScaleMatrixFromCmToM(m);
		mstatus = m.get(floats);
		assert(mstatus == MStatus::kSuccess);
	}

	void ScaleMatrixFromCmToM(MMatrix& matrix)
	{
		// convert Maya mesh in cm to m
		MMatrix scaleM;
		scaleM.setToIdentity();
		scaleM[0][0] = scaleM[1][1] = scaleM[2][2] = CM_2_M;

		matrix *= scaleM;
	}

	bool getInputColorConnection(const MPlug& colorPlug, MPlug& connectedPlug)
	{
		MPlugArray connections;
		colorPlug.connectedTo(connections, true, false);
		if (connections.length() > 0)
		{
			if (connections[0].node().hasFn(MFn::kAnimCurve))
				return false;
			connectedPlug = connections[0];
			return true;
		}

		if (colorPlug.numChildren() == 0)
			return false;

		MPlug redPlug = colorPlug.child(0);
		redPlug.connectedTo(connections, true, false);
		if (connections.length() > 0)
		{
			if (connections[0].node().hasFn(MFn::kAnimCurve))
				return false;
			connectedPlug = connections[0];
			return true;
		}

		if (colorPlug.numChildren() <= 1)
			return false;

		MPlug greenPlug = colorPlug.child(1);
		greenPlug.connectedTo(connections, true, false);
		if (connections.length() > 0)
		{
			if (connections[0].node().hasFn(MFn::kAnimCurve))
				return false;
			connectedPlug = connections[0];
			return true;
		}

		if (colorPlug.numChildren() <= 2)
			return false;

		MPlug bluePlug = colorPlug.child(2);
		bluePlug.connectedTo(connections, true, false);
		if (connections.length() > 0)
		{
			if (connections[0].node().hasFn(MFn::kAnimCurve))
				return false;
			connectedPlug = connections[0];
			return true;
		}

		return false;
	}

	bool setEnvironmentIBL(
		frw::EnvironmentLight& frlight,
		frw::Context frcontext,
		frw::Image frImage,
		float intensity,
		const MMatrix& matrix,
		bool update)
	{
		if (!update)
			frlight = frcontext.CreateEnvironmentLight();

		if (frImage)
			frlight.SetImage(frImage);

		frlight.SetLightIntensityScale(intensity);

		MMatrix m = matrix;
		// convert Maya mesh in cm to m
		MMatrix scaleM;
		scaleM.setToIdentity();
		scaleM[0][0] = scaleM[1][1] = scaleM[2][2] = GetSceneUnitsConversionCoefficient();

		// We are going to flip IBL horizontally here by default.
		// To achieve that we need just make negative scale factor for X component
		if (!IsFlipIBL())
		{
			scaleM[0][0] = -scaleM[0][0];
		}

		m = scaleM * m;
		float mfloats[4][4];
		m.get(mfloats);
		frlight.SetTransform((rpr_float*)mfloats);

		return true;
	}

	bool translateFireRenderIBL(frw::EnvironmentLight& frlight, frw::Image frImage, frw::Context frcontext, const MObject& object, const MMatrix& matrix, bool update)
	{
		float intensity = 1.0;
		{
			MFnDependencyNode nodeFn(object);
			MPlug intensityPlug = nodeFn.findPlug("intensity");
			intensityPlug.getValue(intensity);

			MPlug portalPlug = nodeFn.findPlug("portal");
			MPlugArray portalConnections;
			portalPlug.connectedTo(portalConnections, true, false);
			for (int i = 0; i != portalConnections.length(); ++i) {
				MObject nodeObj = portalConnections[i].node();
			}
		}

		return setEnvironmentIBL(frlight, frcontext, frImage, intensity, matrix, update);
	}

	bool setEnvironmentAmbientLight(
		frw::EnvironmentLight& frlight,
		frw::Context frcontext,
		float intensity,
		MColor colorTemp,
		const MMatrix& matrix,
		bool update)
	{
		if (!update)
			frlight = frcontext.CreateEnvironmentLight();

		frlight.SetAmbientLight(true);

		rpr_image_desc imgDesc;
		imgDesc.image_width = 2;
		imgDesc.image_height = 2;
		imgDesc.image_depth = 0;
		imgDesc.image_row_pitch = imgDesc.image_width * sizeof(rpr_float) * 4;
		imgDesc.image_slice_pitch = 0;

		float imgData[16] = { 
			colorTemp.r, colorTemp.g, colorTemp.b, 1.0f, 
			colorTemp.r, colorTemp.g, colorTemp.b, 1.0f, 
			colorTemp.r, colorTemp.g, colorTemp.b, 1.0f, 
			colorTemp.r, colorTemp.g, colorTemp.b, 1.0f, 
		};

		frw::Image frImage(frcontext, { 4, RPR_COMPONENT_TYPE_FLOAT32 }, imgDesc, imgData);

		frlight.SetImage(frImage);
		frlight.SetLightIntensityScale(intensity);

		MMatrix m = matrix;
		// convert Maya mesh in cm to m
		MMatrix scaleM;
		scaleM.setToIdentity();
		scaleM[0][0] = scaleM[1][1] = scaleM[2][2] = GetSceneUnitsConversionCoefficient();
		m *= scaleM;
		float mfloats[4][4];
		m.get(mfloats);
		frlight.SetTransform((rpr_float*)mfloats);

		return true;
	}

	bool readFireRenderEnvironmentLight(frw::EnvironmentLight& frlight, frw::Context frcontext, const MObject& object, const MMatrix& matrix, bool update)
	{
		MColor color;
		float intensity = 1.0;
		bool display = true;

		{
			MFnDependencyNode nodeFn(object);
			intensity = findPlugTryGetValue(nodeFn, "intensity", 1.0f);
			display = findPlugTryGetValue(nodeFn, "display", true);

			MPlug colorPlug = nodeFn.findPlug("color");

			if (!colorPlug.isNull())
			{
				colorPlug.child(0).getValue(color.r);
				colorPlug.child(1).getValue(color.g);
				colorPlug.child(2).getValue(color.b);
			}
		}

		return setEnvironmentAmbientLight(frlight, frcontext, display ? intensity : 0.0f, color, matrix, update);
	}

	bool translateAmbientLight(frw::EnvironmentLight& frlight, frw::Context frcontext, const MObject& object, const MMatrix& matrix, bool update)
	{
		float intensity = 1.0;
		{
			MFnDependencyNode nodeFn(object);
			MPlug intensityPlug = nodeFn.findPlug("intensity");
			intensityPlug.getValue(intensity);
		}

		MFnLight fnLight(object);
		MColor colorTemp = fnLight.color() /** LIGHT_SCALE*/;

		return setEnvironmentAmbientLight(frlight, frcontext, intensity, colorTemp, matrix, update);
	}

	bool translateWhiteLight(frw::EnvironmentLight& frlight, frw::Context frcontext, const MObject& object, const MMatrix& matrix, bool update)
	{
		MFnDependencyNode nodeFn(object);

		const int numChannels = 4;

		float intensity = 1.0;
		MPlug intensityPlug = nodeFn.findPlug("intensity", false);
		intensityPlug.getValue(intensity);

		MPlug colorPlug = nodeFn.findPlug("color", false);

		RV_PIXEL color = { 1.0f, 1.0f, 1.0f, 1.0f };

		if (!colorPlug.isNull())
		{
			// this color plug has 3 components that's why -1 is applied
			colorPlug.child(0).getValue(color.r);
			colorPlug.child(1).getValue(color.g);
			colorPlug.child(2).getValue(color.b);
		}

		if (!update)
		{
			frlight = frcontext.CreateEnvironmentLight();
		}

		rpr_image_desc imgDesc;
		rpr_uint channels = numChannels;
		imgDesc.image_width = 64;
		imgDesc.image_height = 64;
		imgDesc.image_depth = 0;
		imgDesc.image_row_pitch = imgDesc.image_width * sizeof(rpr_float) * channels;
		imgDesc.image_slice_pitch = 0;

		std::vector<RV_PIXEL> imgData(imgDesc.image_width * imgDesc.image_height * channels, color);

		frw::Image frImage(frcontext, { channels, RPR_COMPONENT_TYPE_FLOAT32 }, imgDesc, imgData.data());

		frlight.SetImage(frImage);
		frlight.SetLightIntensityScale(intensity);

		MMatrix m = matrix;
		// convert Maya mesh in cm to m
		MMatrix scaleM;
		scaleM.setToIdentity();
		scaleM[0][0] = scaleM[1][1] = scaleM[2][2] = GetSceneUnitsConversionCoefficient();
		m *= scaleM;
		float mfloats[4][4];
		m.get(mfloats);
		frlight.SetTransform((rpr_float*)mfloats);

		return true;
	}

	bool translateEnvLight(
		frw::EnvironmentLight& frlight,
		frw::Image& frImage,
		frw::Context frcontext,
		FireMaya::Scope & scope,
		const MObject& object,
		const MMatrix& matrix,
		bool update)
	{
		MFnDependencyNode nodeFn(object);
		if (nodeFn.typeId() == FireMaya::TypeId::FireRenderIBL)
		{
			if (frImage)
				return translateFireRenderIBL(frlight, frImage, frcontext, object, matrix, update);
			else
				return translateWhiteLight(frlight, frcontext, object, matrix, update);
		}
		else if (nodeFn.typeName() == "ambientLight")
		{
			return translateAmbientLight(frlight, frcontext, object, matrix, update);
		}
		else if (nodeFn.typeId() == FireMaya::TypeId::FireRenderEnvironmentLight)
		{
			return readFireRenderEnvironmentLight(frlight, frcontext, object, matrix, update);
		}
		else if (nodeFn.typeName() == FireMaya::VRay::TypeStr::VRayLightDomeShape)
		{
			return FireMaya::VRay::translateVRayLightDomeShape(frlight, frcontext, scope, nodeFn, matrix, update);
		}

		return false;
	}


	bool translateVrayLight(FrLight& frlight, frw::MaterialSystem frMatsys, frw::Context frcontext, const MObject& object, const MMatrix& matrix, bool update)
	{
		using namespace FireMaya::VRay;

		MStatus mstatus;

		MFnDagNode dagNode(object);
		MDagPath dagPath;
		mstatus = dagNode.getPath(dagPath);
		assert(mstatus == MStatus::kSuccess);

		MMatrix m2 = dagPath.inclusiveMatrix(&mstatus);
		MMatrix m = matrix;

		// convert Maya mesh in cm to m
		MMatrix scaleM;
		scaleM.setToIdentity();
		scaleM[0][0] = scaleM[1][1] = scaleM[2][2] = GetSceneUnitsConversionCoefficient();
		m *= scaleM;

		MFnDependencyNode fnLight(object);

		MColor color = readColorOfVRayLight(fnLight);

		if (!update)
			frlight.isAreaLight = false;

		auto typeName = fnLight.typeName();

		if (typeName == TypeStr::VRayLightRectShape)
		{
			if (!translateVRayRectLightShape(frlight, frMatsys, frcontext, fnLight, scaleM, update))
				return false;
		}
		else if (typeName == TypeStr::VRayLightSphereShape)
		{
			if (!translateVRayLightSphereShape(frlight, frMatsys, frcontext, fnLight, scaleM, update))
				return false;
		}
		else if (typeName == TypeStr::VRayLightIESShape || fnLight.typeId() == TypeId::FireRenderIESLightLocator)
		{
			MEulerRotation rotation = MEulerRotation(M_PI / 2.0f, 0.0, 0.0);

			if (!translateVRayLightIESShape(frlight, frMatsys, frcontext, fnLight, scaleM, rotation, update))
				return false;
		}

		return true;
	}

	bool translateSky(
		frw::EnvironmentLight& envLight,
		frw::DirectionalLight& sunLight,
		frw::Image& frImage,
		SkyBuilder& skyBuilder,
		frw::Context frcontext,
		const MObject& object,
		const MMatrix& matrix,
		bool update)
	{
		using namespace FireMaya::VRay;

		// Check that the node is the correct type.
		MFnDependencyNode node(object);
		if (node.typeName() == VRay::TypeStr::VRayGeoSun)
			return translateVRaySunShape(envLight, sunLight, frImage, frcontext, node, matrix, update);
		else if (node.typeId() != TypeId::FireRenderSkyLocator)
			return false;

		// Create lights.
		if (!update)
		{
#ifdef USE_DIRECTIONAL_SKY_LIGHT
			sunLight = frcontext.CreateDirectionalLight();
#endif
			envLight = frcontext.CreateEnvironmentLight();
		}

		// Update the sky image.
		if (skyBuilder.refresh())
		{
			skyBuilder.updateImage(frcontext, frImage);

			// Update the environment light image.
			envLight.SetImage(frImage);
		}

		// Scale lights to RPR space.
		float m[4][4];

		// Transform (rotate) generated IBL image
		MTransformationMatrix t;
		double r1[3] = { 0, -skyBuilder.getSunAzimuth() + M_PI / 2.0, 0 };
		t.setRotation(r1, MTransformationMatrix::RotationOrder::kXYZ);
		MMatrix m2 = t.asMatrix() * matrix;
		m2.get(m);
		envLight.SetTransform(reinterpret_cast<rpr_float*>(m));

		// IBL's intensity
		envLight.SetLightIntensityScale(skyBuilder.getSkyIntensity());

#ifdef USE_DIRECTIONAL_SKY_LIGHT
		// Transform the sun light to the sun direction.
		double r2[3] = { -skyBuilder.getSunAltitude(), -skyBuilder.getSunAzimuth(), 0 };
		t.setRotation(r2, MTransformationMatrix::RotationOrder::kXYZ);
		m2 = t.asMatrix() * matrix;
		m2.get(m);
		sunLight.SetTransform(reinterpret_cast<fr_float*>(m));

		// Update the sun light color.
		MColor& c = skyBuilder.getSunLightColor() * skyBuilder.getSkyIntensity();
		sunLight.SetRadiantPower(c.r, c.g, c.b);
#endif

		return true;
	}
}
