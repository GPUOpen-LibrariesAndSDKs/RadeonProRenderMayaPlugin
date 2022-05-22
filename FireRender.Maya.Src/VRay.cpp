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
#include "FireMaya.h"
#include "frWrap.h"
#include "maya/MFnDagNode.h"
#include "VRay.h"
#include "FireRenderUtils.h"
#include "maya/MEulerRotation.h"
#include "base_mesh.h"
#include "FireRenderMath.h"
#include "Translators/Translators.h"
#include <functional>

#include "IESLight/IESprocessor.h"
#include "IESLight/IESLightRepresentationCalc.h"


namespace FireMaya
{
	namespace VRay
	{
		bool isNonEnvironmentLight(const MFnDagNode& node)
		{
			auto typeStr = node.typeName();

			return typeStr == TypeStr::VRayLightSphereShape
				|| typeStr == TypeStr::VRayLightRectShape
				|| typeStr == TypeStr::VRayLightIESShape;
		}

		bool isEnvironmentLight(const MFnDagNode& node)
		{
			auto typeStr = node.typeName();

			return typeStr == TypeStr::VRayLightDomeShape;
		}

		bool isSkyLight(const MFnDagNode& node)
		{
			auto typeStr = node.typeName();

			return typeStr == TypeStr::VRayGeoSun;
		}

		bool isSunShape(const MFnDagNode& node)
		{
			auto typeStr = node.typeName();

			return typeStr == TypeStr::VRayGeoSun ||
				typeStr == TypeStr::VRaySunShape ||
				typeStr == TypeStr::VRaySunTarget;
		}

		bool isTexture(const MFnDependencyNode& node)
		{
			MTypeId typeId = node.typeId();
			auto typeIdUI = typeId.id();

			switch (typeIdUI)
			{
			case VRayBlendMtl:
			case VRayAlSurface:
			case VRayBumpMtl:
			case VRayCarPaintMtl:
			case VRayFastSSS2:
			case VRayFlakesMtl:
			case VRayLightMtl:
			case VRayMeshMaterial:
			case VRayMtl:
			case VRayMtl2Sided:
			case VRayMtlHair3:
			case VRayMtlOSL:
			case VRayMtlWrapper:
				return true;

			default:
				return false;
			}
		}

		bool isVRayObject(const MFnDagNode& node)
		{
			return isNonEnvironmentLight(node) ||
				isEnvironmentLight(node) ||
				isSkyLight(node) ||
				isSunShape(node);
		}

		MColor translateColorToRadiantPower(MColor color)
		{
			// we are passing a simple scale factor (1 Lm to 1 W) however we are missing geometrical
			// information for an accurate conversion. Specifically we would need the light's Apex angle.
			// Perhaps the rendering engine could return it as a light query option. Or perhaps it could
			// handle the conversion altogether, so the rprIESLightSetRadiantPower3f would actually be used
			// to pass an optional dimming/scaling factor and coloration.
			float watts = 1.f / 683.f;

			return MColor(
				color.r * color.r * watts,
				color.g * color.g * watts,
				color.b * color.b * watts);
		}

		MColor blackBodyColor(float temp)
		{
			float x = temp / 1000.0f;
			float x2 = x * x;
			float x3 = x2 * x;
			float x4 = x3 * x;
			float x5 = x4 * x;

			float R, G, B = 0.0f;

			// red
			if (temp <= 6600)
				R = 1.0f;
			else
				R = 0.0002889f * x5 - 0.01258f * x4 + 0.2148f * x3 - 1.776f * x2 + 6.907f * x - 8.723f;

			// green
			if (temp <= 6600)
				G = -4.593e-05f * x5 + 0.001424f * x4 - 0.01489f * x3 + 0.0498f * x2 + 0.1669f * x - 0.1653f;
			else
				G = -1.308e-07f * x5 + 1.745e-05f * x4 - 0.0009116f * x3 + 0.02348f * x2 - 0.3048f * x + 2.159f;

			// blue
			if (temp <= 2000.0f)
				B = 0.0f;
			else if (temp < 6600.0f)
				B = 1.764e-05f * x5 + 0.0003575f * x4 - 0.01554f * x3 + 0.1549f * x2 - 0.3682f * x + 0.2386f;
			else
				B = 1.0f;

			return MColor(R, G, B);
		}

		bool translateVRayRectLightShape(
			FrLight& frlight,
			frw::MaterialSystem frMatsys,
			frw::Context frcontext,
			const MFnDependencyNode & fnLight,
			const MMatrix scaleM,
			bool update)
		{
			auto data = readVRayLightRectData(fnLight.object());
			auto color = translateColorOfVRayLight(data.color, data.intensity, data.units, 2.f * data.uSize * data.vSize);

			float mfloats[4][4];
			MStatus mstatus = data.matrix.get(mfloats);
			assert(mstatus == MStatus::kSuccess);

			auto primaryVisibility = !data.invisible;

			auto convertedUnits = data.matrix * scaleM;

			MMatrix scaleMatrix2;
			scaleMatrix2.setToIdentity();
			scaleMatrix2[0][0] = data.uSize;
			scaleMatrix2[1][1] = data.vSize;
			scaleMatrix2[2][2] = 1.0;
			auto m = scaleMatrix2 * convertedUnits;
			mstatus = m.get(mfloats);

			frlight.scaleX = data.uSize;
			frlight.scaleY = data.vSize;
			frlight.scaleZ = 1.0f;

			// Fire Render's area light is implemented as a mesh with emissive shader
			if (update)
			{
				if (!frlight.areaLight)
					return false;

				frlight.areaLight.SetTransform((rpr_float*)mfloats);

				if (frlight.emissive)
					frlight.emissive.SetValue(RPR_MATERIAL_INPUT_COLOR, frw::Value(color.r, color.g, color.b));
			}
			else
			{
				if (!frlight.emissive)
					frlight.emissive = frw::EmissiveShader(frMatsys);

				if (!frlight.transparent)
				{
					frlight.transparent = frw::TransparentShader(frMatsys);
					frlight.transparent.SetValue(RPR_MATERIAL_INPUT_COLOR, 1.0f);
				}

				//create transparent shader for portals

				struct vertex
				{
					rpr_float pos[3];
					rpr_float norm[3];
					rpr_float tex[2];
				};

				static const vertex quad[] =
				{
					{ -1.f, -1.f, 0.f,		0.f, 0.f, -1.f,			0.f, 0.f },
					{ 1.f, -1.f, 0.f,		0.f, 0.f, -1.f,			1.f, 0.f },
					{ 1.f,  1.f, 0.f,		0.f, 0.f, -1.f,			1.f, 1.f },
					{ -1.f,  1.f, 0.f,		0.f, 0.f, -1.f,			0.f, 1.f }
				};

				static const rpr_int indices[] =
				{
					0, 2, 1,
					0, 3, 2
				};

				static const rpr_int num_face_vertices[] =
				{
					3, 3
				};

				frlight.areaLight = frcontext.CreateMesh(
					(rpr_float const*)&quad[0], 24, sizeof(vertex),
					(rpr_float const*)((char*)&quad[0] + sizeof(rpr_float) * 3), 24, sizeof(vertex),
					(rpr_float const*)((char*)&quad[0] + sizeof(rpr_float) * 6), 24, sizeof(vertex),
					(rpr_int const*)indices, sizeof(rpr_int),
					(rpr_int const*)indices, sizeof(rpr_int),
					(rpr_int const*)indices, sizeof(rpr_int),
					num_face_vertices, 2);
				frlight.areaLight.SetTransform((rpr_float*)mfloats);
				frlight.areaLight.SetShader(frlight.emissive);
				frlight.areaLight.SetShadowCatcherFlag(false);
				frlight.areaLight.SetShadowFlag(false);
				frlight.emissive.SetValue(RPR_MATERIAL_INPUT_COLOR, frw::Value(color.r, color.g, color.b));
				frlight.isAreaLight = true;
			}

			if (frlight.areaLight)
				frlight.areaLight.SetPrimaryVisibility(primaryVisibility);

			return true;
		}


		MColor readColorOfVRayLight(const MFnDependencyNode &fnLight)
		{
			MColor color;

			MPlug colorPlug = fnLight.findPlug("color");

			if (!colorPlug.isNull())
			{
				int colorMode = findPlugTryGetValue<int>(fnLight, "colorMode", 0, false);

				if (colorMode == 0)
				{
					colorPlug.child(0).getValue(color.r);
					colorPlug.child(1).getValue(color.g);
					colorPlug.child(2).getValue(color.b);
				}
				else
				{
					float temperature = findPlugTryGetValue(fnLight, "temperature", 6300.0f);
					color = blackBodyColor(temperature);
				}
			}

			return color;
		}

		float translateVrayLightIntensity(float intensisty, VRayLightUnits units, float area)
		{
			const float lumensToWatts = 1 / 683.f;

			area /= 100 * 100;	// cm2 to m2
			switch (units)
			{
			case FireMaya::VRay::VRayLightUnits::Default:
				return intensisty;
			case FireMaya::VRay::VRayLightUnits::LumensPerSqrMetersPerSteradian:
				return (float) (intensisty * lumensToWatts * M_PI);
			case FireMaya::VRay::VRayLightUnits::Lumens:
				return (float) (intensisty * lumensToWatts / area);
			case FireMaya::VRay::VRayLightUnits::WattsPerSqrMetersPerSteradian:
				return (float) (intensisty * M_PI);
			case FireMaya::VRay::VRayLightUnits::Watts:
				return intensisty / area;
			default:
				throw std::logic_error("Unsupported VRay light unit specified");
			}
		}

		MColor translateColorOfVRayLight(const MColor color, float intensisty, VRayLightUnits units, float area)
		{
			intensisty = translateVrayLightIntensity(intensisty, units, area);

			return color * intensisty;
		}

		VRayLighSphereData readVRayLightSphereData(MObject vrayLight)
		{
			MStatus status;
			VRayLighSphereData ret;
			MFnDependencyNode depNodeVRayLight(vrayLight);

			ret.color = readColorOfVRayLight(depNodeVRayLight);
			ret.intensity = findPlugTryGetValue(depNodeVRayLight, "intensity", 1.0f);
			ret.units = static_cast<VRayLightUnits>(findPlugTryGetValue(depNodeVRayLight, "units", 0));

			{
				MFnDagNode dagNode(vrayLight);
				MDagPath dagPath;
				status = dagNode.getPath(dagPath);
				assert(status == MStatus::kSuccess);

				ret.matrix = dagPath.inclusiveMatrix(&status);
			}

			ret.radius = findPlugTryGetValue(depNodeVRayLight, "radius", 1.0f);
			ret.invisible = findPlugTryGetValue(depNodeVRayLight, "invisible", false);
			ret.enabled = findPlugTryGetValue(depNodeVRayLight, "enabled", true);

			return ret;
		}

		VRayLighRectData readVRayLightRectData(MObject vrayLight)
		{
			MStatus status;
			VRayLighRectData ret;
			MFnDependencyNode depNodeVRayLight(vrayLight);

			ret.color = readColorOfVRayLight(depNodeVRayLight);
			ret.intensity = findPlugTryGetValue(depNodeVRayLight, "intensity", 1.0f);
			ret.units = static_cast<VRayLightUnits>(findPlugTryGetValue(depNodeVRayLight, "units", 0));

			{
				MFnDagNode dagNode(vrayLight);
				MDagPath dagPath;
				status = dagNode.getPath(dagPath);
				assert(status == MStatus::kSuccess);

				ret.matrix = dagPath.inclusiveMatrix(&status);
			}

			ret.uSize = findPlugTryGetValue(depNodeVRayLight, "uSize", 1.0f);
			ret.vSize = findPlugTryGetValue(depNodeVRayLight, "vSize", 1.0f);

			ret.invisible = findPlugTryGetValue(depNodeVRayLight, "invisible", false);
			ret.enabled = findPlugTryGetValue(depNodeVRayLight, "enabled", true);

			return ret;
		}

		VRayIESLightData readVRayIESLightData(MObject vrayLight)
		{
			MStatus status;
			VRayIESLightData ret;
			MFnDependencyNode depNodeVRayLight(vrayLight);

			ret.color = readColorOfVRayLight(depNodeVRayLight);
			{
				MPlug filterColorPlug = depNodeVRayLight.findPlug("filterColor");
				if (!filterColorPlug.isNull())
				{
					filterColorPlug.child(0).getValue(ret.filterColor.r);
					filterColorPlug.child(1).getValue(ret.filterColor.g);
					filterColorPlug.child(2).getValue(ret.filterColor.b);
				}
			}

			ret.intensity = findPlugTryGetValue(depNodeVRayLight, "intensity", 1.0f);

			{
				MFnDagNode dagNode(vrayLight);
				MDagPath dagPath;
				status = dagNode.getPath(dagPath);
				assert(status == MStatus::kSuccess);

				ret.matrix = dagPath.inclusiveMatrix(&status);
			}

			ret.filePath = findPlugTryGetValue(depNodeVRayLight, "iesFile", ""_ms);

			ret.visible = true;

			return ret;
		}

		VRayLightDomeData readVRayLightDomeShapeData(MObject vrayLight)
		{
			MStatus status;
			VRayLightDomeData ret;
			MFnDependencyNode depNodeVRayLight(vrayLight);

			ret.color = readColorOfVRayLight(depNodeVRayLight);
			ret.intensity = findPlugTryGetValue(depNodeVRayLight, "intensity", 1.0f);

			{
				MFnDagNode dagNode(vrayLight);
				MDagPath dagPath;
				status = dagNode.getPath(dagPath);
				assert(status == MStatus::kSuccess);

				ret.matrix = dagPath.inclusiveMatrix(&status);
			}

			ret.domeSpherical = findPlugTryGetValue(depNodeVRayLight, "domeSpherical", false);

			ret.useDomeTexture = findPlugTryGetValue(depNodeVRayLight, "useDomeTex", false);

			MPlug domeTexPlug = depNodeVRayLight.findPlug("domeTex");

			MPlugArray domeTextures;
			domeTexPlug.connectedTo(domeTextures, true, false);
			auto length = domeTextures.length();
			for (auto i = 0u; i < length; i++)
			{
				MObject nodeObj = domeTextures[i].node();
				if (nodeObj.hasFn(MFn::kFileTexture))
				{
					MFnDependencyNode imageDepNode(nodeObj);
					ret.texutre = findPlugTryGetValue<MString>(imageDepNode, "computedFileTextureNamePattern", "");
				}
			}

			ret.textureColor.r = findPlugTryGetValue(depNodeVRayLight, "domeTexR", .0f);
			ret.textureColor.g = findPlugTryGetValue(depNodeVRayLight, "domeTexG", .0f);
			ret.textureColor.b = findPlugTryGetValue(depNodeVRayLight, "domeTexB", .0f);
			ret.textureColor.a = findPlugTryGetValue(depNodeVRayLight, "domeTexA", .0f);

			ret.multiplyByLightColor = findPlugTryGetValue(depNodeVRayLight, "multiplyByTheLightColor", false);

			ret.visible = findPlugTryGetValue(depNodeVRayLight, "enabled", true);

			return ret;
		}

		MObject tryFindSiblingOrParentOfType(MObject object, const char* typeName)
		{
			{	// check if current node has that name:
				auto nodeTypeName = MFnDagNode(object).typeName();

				if (nodeTypeName == typeName)
				{
					return object;
				}
			}

			// Check children:
			auto dagPath = MDagPath::getAPathTo(object);

			MStatus status = MStatus::kSuccess;
			while (status == MStatus::kSuccess)
			{
				status = dagPath.pop();
				auto cc = dagPath.childCount();
				for (auto ci = 0u; ci < cc; ci++)
				{
					auto child = dagPath.child(ci);
					MFnDagNode dagNode(child);
					auto nodeTypeName = dagNode.typeName();

					if (nodeTypeName == typeName)
					{
						return dagNode.object();
					}
				}
			}

			return MObject();
		}

		MObject tryFindChildOfType(MObject object, const char* typeName)
		{
			{	// check if current node has that name:
				auto nodeTypeName = MFnDagNode(object).typeName();

				if (nodeTypeName == typeName)
				{
					return object;
				}
			}

			// Check children:
			auto dagPath = MDagPath::getAPathTo(object);

			auto cc = dagPath.childCount();
			for (auto ci = 0u; ci < cc; ci++)
			{
				auto child = dagPath.child(ci);
				MFnDagNode dagNode(child);
				auto nodeTypeName = dagNode.typeName();

				if (nodeTypeName == typeName)
				{
					return dagNode.object();
				}
			}

			return MObject();
		}

		VRayLightSunShapeDataPtr readVRayLightSunShapeData(MObject vrayLight)
		{
			MStatus status;
			VRayLightSunShapeData ret;

			MObject sunGeo = tryFindSiblingOrParentOfType(vrayLight, VRay::TypeStr::VRayGeoSun);

			if (!sunGeo.isNull())
			{
				///////////////////////////////////////////////////////////////////////
				// Read from GeoSun:
				{
					MFnDagNode dagNode(sunGeo);
					ret.nameOfVRayGeoSun = dagNode.name();
				}

				MFnDependencyNode depNodeSunGeo(sunGeo);

				ret.color = readColorOfVRayLight(depNodeSunGeo);

				MVector position;
				position.x = findPlugTryGetValue(depNodeSunGeo, "translateX", 0.0f);
				position.y = findPlugTryGetValue(depNodeSunGeo, "translateY", 0.0f);
				position.z = findPlugTryGetValue(depNodeSunGeo, "translateZ", 0.0f);

				ret.position = position;

				ret.manualPosition = findPlugTryGetValue(sunGeo, "gmanual", true);
				ret.manualPositionUp = findPlugTryGetValue(sunGeo, "gmanualup", true);

				{
					ret.upVector.x = findPlugTryGetValue(sunGeo, "gupvector0", 0.0f);
					ret.upVector.y = findPlugTryGetValue(sunGeo, "gupvector1", 0.0f);
					ret.upVector.z = findPlugTryGetValue(sunGeo, "gupvector2", 0.0f);
				}

				ret.azimuthDeg = findPlugTryGetValue(sunGeo, "azimuth", 0.0f);
				ret.altitudeDeg = findPlugTryGetValue(sunGeo, "altitude", 0.0f);

				///////////////////////////////////////////////////////////////////////
				// Read from SunShape:
				MObject objSunShape = tryFindChildOfType(depNodeSunGeo.object(), FireMaya::VRay::TypeStr::VRaySunShape);
				if (!objSunShape.isNull())
				{
					MFnDependencyNode sunShapeDepNode(objSunShape);

					{
						MFnDagNode dagNode(vrayLight);
						MDagPath dagPath;
						status = dagNode.getPath(dagPath);
						assert(status == MStatus::kSuccess);

						ret.matrix = dagPath.inclusiveMatrix(&status);
						ret.nameOfVRaySunShape = dagNode.name();
					}

					ret.intensity = findPlugTryGetValue(sunShapeDepNode, "intensity", 1.0f);
					ret.turbidity = findPlugTryGetValue(sunShapeDepNode, "turbidity", 2.5f);
					ret.ozone = findPlugTryGetValue(sunShapeDepNode, "ozone", 0.35f);
					ret.sizeMultiplier = findPlugTryGetValue(sunShapeDepNode, "sizeMultiplier", 1.0f);

					{
						float r = findPlugTryGetValue(sunShapeDepNode, "colorR", 1.0f);
						float g = findPlugTryGetValue(sunShapeDepNode, "colorG", 1.0f);
						float b = findPlugTryGetValue(sunShapeDepNode, "colorB", 1.0f);
						ret.filterColor = MColor(r, g, b);
					}
				}

				///////////////////////////////////////////////////////////////////////
				// Read from SunTarget:
				MObject objSunTarget = tryFindSiblingOrParentOfType(vrayLight, FireMaya::VRay::TypeStr::VRaySunTarget);
				if (!objSunTarget.isNull())
				{
					MFnDagNode dagNode(objSunTarget);
					ret.nameOfVRaySunTarget = dagNode.name();
				}

				return std::make_unique<VRayLightSunShapeData>(ret);
			}

			return nullptr;
		}

		bool translateVRayLightSphereShape(
			FrLight& frlight,
			frw::MaterialSystem frMatsys,
			frw::Context frcontext,
			const MFnDependencyNode & fnLight,
			const MMatrix scaleM,
			bool update)
		{
			auto object = fnLight.object();
			auto lightData = readVRayLightSphereData(object);

			float mfloats[4][4];
			MStatus mstatus = lightData.matrix.get(mfloats);
			assert(mstatus == MStatus::kSuccess);

			frlight.scaleX = frlight.scaleY = frlight.scaleZ = lightData.radius;

			MMatrix scaled;
			scaled.setToIdentity();
			scaled[0][0] = scaled[1][1] = scaled[2][2] = lightData.radius;
			scaled[3][0] = (1 - lightData.radius) * lightData.matrix[3][0];
			scaled[3][1] = (1 - lightData.radius) * lightData.matrix[3][1];
			scaled[3][2] = (1 - lightData.radius) * lightData.matrix[3][2];
			scaled[3][3] = 1.f;
			MMatrix m = lightData.matrix * scaled * scaleM;
			mstatus = m.get(mfloats);

			auto color = translateColorOfVRayLight(lightData.color, lightData.intensity, lightData.units, (float) (4 * M_PI * powf(lightData.radius, 2.f)));

			// Fire Render's area light is implemented as a mesh with emissive shader
			if (update)
			{
				if (!frlight.areaLight)
					return false;

				frlight.areaLight.SetTransform((rpr_float*)mfloats);

				if (frlight.emissive)
					frlight.emissive.SetValue(RPR_MATERIAL_INPUT_COLOR, frw::Value(color.r, color.g, color.b));
			}
			else
			{
				if (!frlight.emissive)
					frlight.emissive = frw::EmissiveShader(frMatsys);
				if (!frlight.transparent)
				{
					frlight.transparent = frw::TransparentShader(frMatsys);
					frlight.transparent.SetValue(RPR_MATERIAL_INPUT_COLOR, 1.0f);
				}

				frlight.areaLight = frcontext.CreateMesh(
					vertices, 382, sizeof(float) * 3,
					normals, 382, sizeof(float) * 3,
					texcoords, 439, sizeof(float) * 2,
					vertexIndices, sizeof(int),
					normalIndices, sizeof(int),
					texcoord_indices, sizeof(int),
					polyCountArray, 400);

				frlight.areaLight.SetTransform((rpr_float*)mfloats);
				frlight.areaLight.SetShader(frlight.emissive);
				frlight.areaLight.SetShadowCatcherFlag(false);
				frlight.areaLight.SetShadowFlag(false);
				frlight.emissive.SetValue(RPR_MATERIAL_INPUT_COLOR, frw::Value(color.r, color.g, color.b));
				frlight.isAreaLight = true;
			}

			if (frlight.areaLight)
			{
				frlight.areaLight.SetVisibility(lightData.enabled);
				frlight.areaLight.SetPrimaryVisibility(lightData.enabled && !lightData.invisible);
			}

			return true;
		}

		bool translateVRayLightIESShape(
			FrLight& frlight,
			frw::MaterialSystem frMatsys,
			frw::Context frcontext,
			MFnDependencyNode & fnVRayLight,
			const MMatrix scaleM,
			const MEulerRotation,
			bool update)
		{
			auto data = readVRayIESLightData(fnVRayLight.object());

			frlight.scaleX = frlight.scaleY = frlight.scaleZ = 0.01f;

			float mfloats[4][4];
			MStatus mstatus = data.matrix.get(mfloats);
			assert(mstatus == MStatus::kSuccess);

			auto primaryVisibility = data.visible;

			MMatrix m = data.matrix;
			m = scaleM * m;
			m.get(mfloats);
			mfloats[3][0] *= 0.01f;
			mfloats[3][1] *= 0.01f;
			mfloats[3][2] *= 0.01f;

			auto color = data.color /** data.filterColor */ * data.intensity;// *PI_F;

			// Fire Render's area light is implemented as a mesh with emissive shader
			if (update)
			{
				if (!frlight.light)
					return false;

				frlight.light.SetTransform((rpr_float*)mfloats);

				frw::IESLight iesLight(frlight.light.Handle(), frcontext);

				auto rp = translateColorToRadiantPower(color);
				iesLight.SetRadiantPower(rp.r, rp.g, rp.b);
			}
			else
			{
				auto iesFile = data.filePath;
				IESProcessor processor;
				IESLightRepresentationParams params;

				if (iesFile.length() && 
					processor.Parse(params.data, iesFile.asWChar()) == IESProcessor::ErrorCode::SUCCESS)
				{
					auto iesLight = frcontext.CreateIESLight();

					rpr_int res = iesLight.SetIESData(processor.ToString(params.data).c_str(), 256, 256);
					assert(res == RPR_SUCCESS);

					if (res == RPR_SUCCESS)
					{
						auto rp = translateColorToRadiantPower(color);
						iesLight.SetRadiantPower(rp.r, rp.g, rp.b);

						iesLight.SetTransform(reinterpret_cast<const rpr_float*>(mfloats));

						frlight.isAreaLight = false;

						frlight.light = iesLight;
					}
					else
					{
						MString str = MString("RPR error: Failed to load IES Data: code = ") + res;
						MGlobal::displayError(str);
						return false;
					}
				}
			}

			if (frlight.areaLight)
			{
				frlight.areaLight.SetVisibility(primaryVisibility);
			}

			return true;
		}

		bool translateVRayLightDomeShape(
			frw::EnvironmentLight& frlight,
			frw::Context frcontext,
			FireMaya::Scope & scope,
			MFnDependencyNode & nodeFn,
			MMatrix matrix,
			bool update)
		{
			MStatus mstatus;

			auto data = readVRayLightDomeShapeData(nodeFn.object());

			if (data.visible)
			{
				float intensity = data.intensity;

				MColor color = data.color;

				frw::Image frImage;
				bool useDomeTexture = data.useDomeTexture;
				if (useDomeTexture)
				{
					MColor texColor = data.textureColor;

					bool multiplyByTheLightColor = data.multiplyByLightColor;

					if (multiplyByTheLightColor)
						color = texColor * color;
					else
						color = texColor;

					if (data.useDomeTexture)
					{
						MString filePath = data.texutre;
						if (filePath.length())
							frImage = scope.GetImage(filePath.asUTF8(), "", nodeFn.name());
					}
				}

				if (frImage)
					setEnvironmentIBL(frlight, frcontext, frImage, intensity, matrix, update);
				else
					setEnvironmentAmbientLight(frlight, frcontext, /*intensity already applied to color*/1.0f, color, matrix, update);
			}

			return true;
		}

		VRayLightSunShapeDataPtr translateManualVRaySunPoisiton(VRayLightSunShapeDataPtr data)
		{
			if (!data)
				return nullptr;

			if (data->manualPosition)
			{
				DebugPrint("Position: %f, %f, %f\n", data->position.x, data->position.y, data->position.z);

				auto azimuth = static_cast<float>(atan2(-data->position.x, data->position.z));

				MVector proj(data->position.x, 0, data->position.z);
				std::function<MVector(MVector)> normalize = [](MVector in) -> MVector { return MVector(in.x / in.length(), in.y / in.length(), in.z / in.length()); };
				std::function<double(MVector, MVector)> dot = [](MVector in1, MVector in2) -> double { return in1.x * in2.x + in1.y * in2.y + in1.z * in2.z; };

				auto altitude = static_cast<float>(acos(dot(normalize(data->position), normalize(proj))));

				if (data->position.y < 0)
					altitude = -altitude;

				data->azimuthDeg = static_cast<float>(limit_degrees180pm(rad2deg(azimuth) + 180));
				data->altitudeDeg = static_cast<float>(rad2deg(altitude));

				DebugPrint("Azimuth: %f, Altitude: %f\n", data->azimuthDeg, data->altitudeDeg);

				data->manualPosition = false;
			}

			return data;
		}

		bool translateVRaySunShape(
			frw::EnvironmentLight& envLight,
			frw::DirectionalLight& sunLight,
			frw::Image& frImage,
			frw::Context frcontext,
			MFnDependencyNode & node,
			const MMatrix& matrix,
			bool update)
		{
			auto data = readVRayLightSunShapeData(node.object());

			if (data)
			{
				// Create lights.
				if (!update)
					sunLight = frcontext.CreateDirectionalLight();

				sunLight.SetRadiantPower(
					sqrt(data->filterColor.r) * 64,
					sqrt(data->filterColor.g) * 64,
					sqrt(data->filterColor.b) * 64);

				// Scale lights to RPR space.
				MTransformationMatrix t;
				float m[4][4];

				if (data->manualPosition)
					data = translateManualVRaySunPoisiton(data);

				// Transform the sun light to the sun direction.
				DebugPrint("Using Azimuth: %f and Altitude: %f\n", data->azimuthDeg, data->altitudeDeg);
				double r[3] = { -deg2rad(data->altitudeDeg), -deg2rad(limit_degrees180pm(data->azimuthDeg - 180)), 0 };
				t.setRotation(r, MTransformationMatrix::RotationOrder::kXYZ);
				t.asRotateMatrix().get(m);

				DebugPrint("%f, %f, %f, %f\n", m[0][0], m[0][1], m[0][2], m[0][3]);
				DebugPrint("%f, %f, %f, %f\n", m[1][0], m[1][1], m[1][2], m[1][3]);
				DebugPrint("%f, %f, %f, %f\n", m[2][0], m[2][1], m[2][2], m[2][3]);
				DebugPrint("%f, %f, %f, %f\n", m[3][0], m[3][1], m[3][2], m[3][3]);

				sunLight.SetTransform(reinterpret_cast<rpr_float*>(m));

				return true;
			}
			else
				return false;
		}
	}
}
