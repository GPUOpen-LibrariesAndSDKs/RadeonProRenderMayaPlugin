#pragma once

#include <string>
#include <memory>
#include <Translators.h>

#if MAYA_API_VERSION < 20180000
class MFnDagNode;
#endif

namespace FireMaya
{
	namespace VRay
	{
		enum VrayTypeIDs
		{
			// Lights:
			VRayLightSphereShape = 0xb4006,
			VRayLightDomeShape = 0xb4007,
			VRayLightRectShape = 0xb4008,
			VRayLightIESShape = 0x1104a0,
			VRaySunShape = 0xb4003,
			VRaySunTarget = 0xb4004,
			// Materials:
			VRayBumpMtl = 0x1104af,
			VRayAlSurface = 0x1104d6,
			VRayBlendMtl = 0x110498,
			VRayCarPaintMtl = 0x1104b6,
			VRayFastSSS2 = 0x110497,
			VRayFlakesMtl = 0x1104bf,
			VRayLightMtl = 0x110494,
			VRayMeshMaterial = 0x1104a3,
			VRayMtl = 0xb4010,
			VRayMtl2Sided = 0x110493,
			VRayMtlHair3 = 0x1104c1,
			VRayMtlOSL = 0x1104da,
			VRayMtlRenderStats = 0x1104cd,
			VRayMtlWrapper = 0x110495,
			VRayPointParticleMtl = 0x1104d9,
			VRayScannedMtl = 0x1104cb,
			VRaySimbiont = 0x110492,
			VRaySkinMtl = 0x1104d4,
			VRayStochasticFlakesMtl = 0x1104df,
			VRaySwitchMtl = 0x1104d0,
			VRayVRmatMtl = 0x1104c6,
			// Volume:
			VRayAerialPerspective = 0x1104c3,
			VRayEnvironmentFog = 0x1104ad,
			VRayScatterFog = 0x11049a,
			VRaySimpleFog = 0x110499,
			VRaySphereFadeVolume = 0x1104ab,
		};

		enum class VRayLightUnits
		{
			Default = 0,
			Lumens = 1,
			LumensPerSqrMetersPerSteradian = 2,
			Watts = 3,
			WattsPerSqrMetersPerSteradian = 4,
		};

		namespace TypeStr
		{
			const char * const VRayLightRectShape = "VRayLightRectShape";
			const char * const VRayLightSphereShape = "VRayLightSphereShape";
			const char * const VRayLightIESShape = "VRayLightIESShape";
			const char * const VRayLightDomeShape = "VRayLightDomeShape";
			const char * const VRayGeoSun = "VRayGeoSun";
			const char * const VRaySunShape = "VRaySunShape";
			const char * const VRaySunTarget = "VRaySunTarget";
		}

		bool isNonEnvironmentLight(const MFnDagNode& node);
		bool isEnvironmentLight(const MFnDagNode& node);
		bool isSkyLight(const MFnDagNode& node);

		bool isTexture(const MFnDependencyNode& node);

		bool isVRayObject(const MFnDagNode& node);

		struct VRayLighSphereData
		{
			MMatrix matrix;
			float radius;
			float intensity;
			VRayLightUnits units;
			MColor color;
			bool invisible;
			bool enabled;
		};

		struct VRayLighRectData
		{
			MMatrix matrix;
			MColor color;
			float uSize, vSize;
			float intensity;
			VRayLightUnits units;
			bool invisible;
			bool enabled;
		};

		struct VRayIESLightData
		{
			MString filePath;
			MMatrix matrix;
			MColor color;
			MColor filterColor;
			float intensity;
			bool visible;
		};

		struct VRayLightDomeData
		{
			MMatrix matrix;
			MColor color;
			MString texutre;	// domeTex
			MColor textureColor; // domeTexR domeTexG domeTexB domeTexA
			float intensity;
			bool useDomeTexture; // useDomeTex
			bool multiplyByLightColor; // multiplyByTheLightColor
			bool domeSpherical;	// domeSpherical
			bool visible;
		};

		struct VRayLightSunShapeData
		{
			MString nameOfVRaySunShape;
			MString nameOfVRayGeoSun;
			MString nameOfVRaySunTarget;
			MMatrix matrix;
			MColor color;
			MVector upVector;
			MVector position;
			MColor filterColor;

			float azimuthDeg;
			float altitudeDeg;

			float turbidity;
			float ozone;
			float sizeMultiplier;

			float intensity;
			VRayLightUnits units;

			bool manualPosition;
			bool manualPositionUp;
		};

		typedef std::shared_ptr<VRayLightSunShapeData> VRayLightSunShapeDataPtr;

		float translateVrayLightIntensity(float intensisty, VRayLightUnits units, float area);
		MColor translateColorOfVRayLight(const MColor color, float intensity, VRayLightUnits units, float area);

		MColor readColorOfVRayLight(const MFnDependencyNode &fnLight);

		VRayLighSphereData readVRayLightSphereData(MObject vrayLight);
		VRayLighRectData readVRayLightRectData(MObject vrayLight);
		VRayIESLightData readVRayIESLightData(MObject vrayLight);
		VRayLightDomeData readVRayLightDomeShapeData(MObject vrayLight);
		VRayLightSunShapeDataPtr readVRayLightSunShapeData(MObject vrayLight);

		VRayLightSunShapeDataPtr translateManualVRaySunPoisiton(VRayLightSunShapeDataPtr data);
		bool translateVRayRectLightShape(FrLight& frlight, frw::MaterialSystem frMatsys, frw::Context frcontext, const MFnDependencyNode & fnLight, const MMatrix scaleM, bool update);
		bool translateVRayLightSphereShape(FrLight& frlight, frw::MaterialSystem frMatsys, frw::Context frcontext, const MFnDependencyNode & fnLight, const MMatrix scaleM, bool update);
		bool translateVRayLightIESShape(FrLight& frlight, frw::MaterialSystem frMatsys, frw::Context frcontext, MFnDependencyNode & fnVRayLight, const MMatrix scaleM, const MEulerRotation rotation, bool update);
		bool translateVRayLightDomeShape(frw::EnvironmentLight& frlight, frw::Context frcontext, FireMaya::Scope & scope, MFnDependencyNode & nodeFn, MMatrix matrix, bool update);
		VRayLightSunShapeDataPtr translateManualVRaySunPoisiton(VRayLightSunShapeDataPtr data);
		bool translateVRaySunShape(frw::EnvironmentLight& envLight, frw::DirectionalLight& sunLight, frw::Image& frImage, frw::Context frcontext, MFnDependencyNode & node, const MMatrix& matrix, bool update);
	}
}
