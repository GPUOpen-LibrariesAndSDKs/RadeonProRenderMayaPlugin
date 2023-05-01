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

#include <maya/MApiNamespace.h>

#include <maya/MFnDagNode.h>
#include <maya/MObjectArray.h>
#include <maya/MIntArray.h>
#include <maya/MFnMesh.h>
#include <maya/MDagPath.h>
#include <maya/MGlobal.h>
#include <maya/MFnNurbsSurface.h>
#include <maya/MRenderView.h>
#include <maya/MDagPathArray.h>
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MRampAttribute.h>
#include "FireRenderAOVs.h"
#include "FireRenderError.h"
#include "FireRenderGlobals.h"
#include "Logger.h"
#include <chrono>
#include "RprTools.h"
#include <algorithm>
#include <iterator>
#include <array>
#include <functional>
#include <float.h>

#ifndef PI
#define PI 3.14159265358979323846
#endif

typedef std::chrono::time_point<std::chrono::steady_clock> TimePoint;

// FireRenderGlobals
// Utility class used to read attributes form the render global node
// and configure the rpr_context

struct AirVolumeSettings
{
	AirVolumeSettings()
		: airVolumeEnabled(false)
		, fogEnabled(false)
		, fogColor(1.0f, 1.0f, 1.0f, 0.0f)
		, fogDistance(5000)
		, fogHeight(1.5f)
		, airVolumeDensity(0.8f)
		, airVolumeColor(1.0f, 1.0f, 1.0f, 0.0f)
		, airVolumeClamp(0.1f)
	{}

	bool airVolumeEnabled;
	bool fogEnabled;
	MColor fogColor;
	float fogDistance;
	float fogHeight;
	float airVolumeDensity;
	MColor airVolumeColor;
	float airVolumeClamp;
};

struct DenoiserSettings
{
	DenoiserSettings()
	{
		enabled = false;
		type = FireRenderGlobals::kBilateral;
		radius = 0;
		samples = 0;
		filterRadius = 0;
		bandwidth = 0.0f;
		color = 0.0f;
		depth = 0.0f;
		normal = 0.0f;
		trans = 0.0f;
		colorOnly = false;
		enable16bitCompute = false;

		viewportDenoiseUpscaleEnabled = false;
	}

	bool enabled;
	FireRenderGlobals::DenoiserType type;
	int radius;
	int samples;
	int filterRadius;
	float bandwidth;
	float color;
	float depth;
	float normal;
	float trans;
	bool colorOnly;
	bool enable16bitCompute;

	bool viewportDenoiseUpscaleEnabled;
};

struct CompletionCriteriaParams
{
	CompletionCriteriaParams()
	{
		makeInfiniteTime();

		completionCriteriaMaxIterations = 0;
		completionCriteriaMinIterations = 1; // should not be zero
	}

	int getTotalSecondsCount()
	{
		return completionCriteriaHours * 3600 + completionCriteriaMinutes * 60 + completionCriteriaSeconds;
	}

	void makeInfiniteTime()
	{
		completionCriteriaHours = 0;
		completionCriteriaMinutes = 0;
		completionCriteriaSeconds = 0;
	}

	bool isUnlimitedTime() const
	{
		return completionCriteriaHours == 0 && completionCriteriaMinutes == 0 && completionCriteriaSeconds == 0;
	}

	bool isUnlimitedIterations() const
	{
		return completionCriteriaMaxIterations == 0;
	}

	bool isUnlimited() const
	{
		return isUnlimitedTime() && isUnlimitedIterations();
	}

	//short completionCriteriaType;
	int completionCriteriaHours;
	int completionCriteriaMinutes;
	int completionCriteriaSeconds;

	int completionCriteriaMaxIterations;
	int completionCriteriaMinIterations;
};

enum class RenderType
{
	Undefined = 0,
	ProductionRender,
	IPR,
	ViewportRender,
	Thumbnail // swatch
};

enum class RenderQuality
{
	RenderQualityFull = 0,
	RenderQualityHigh,
	RenderQualityMedium,
	RenderQualityLow,
	RenderQualityNorthStar,
	RenderQualityHybridPro
};


class FireRenderGlobalsData
{
public:

	// Constructor
	FireRenderGlobalsData();

	//Destructor
	~FireRenderGlobalsData();

	//Read data from the FireRenderGLobals/FireRenderViewportGlobals node in the scene
	void readFromCurrentScene();

	// Update the tonemapping in rpr_context
	//void updateTonemapping(class FireRenderContext& frcontext, bool disableWhiteBalance = false);

	// Setup the rpr_context
	//void setupContext(class FireRenderContext& frcontext);

	// Checks if name is one of the options are tonemapping options
	static bool isTonemapping(MString name);

	// Checks if name is one of the options are denoiser options
	static bool isDenoiser(MString name);

	static bool IsMotionBlur(MString name);

	static bool IsAirVolume(MString name);

	static void getCPUThreadSetup(bool& overriden, int& cpuThreadCount, RenderType renderType);
	static int getThumbnailIterCount(bool* pSwatchesEnabled = nullptr);
	static bool isExrMultichannelEnabled(void);

public:

	// Completion criteria.
	CompletionCriteriaParams completionCriteriaFinalRender;
	CompletionCriteriaParams completionCriteriaViewport;

	int adaptiveTileSize;
	float adaptiveThreshold;
	float adaptiveThresholdViewport;

	bool textureCompression;

	int viewportRenderMode;
	int renderMode;

	MString textureCachePath;

	// Global Illumination
	bool giClampIrradiance;
	float giClampIrradianceValue;

	// (iterations, legacy name "aasamples")
	int samplesPerUpdate;

	// Filter type
	short filterType;

	// Filter size
	short filterSize;

	// Max ray depth
	int maxRayDepth;
	int maxRayDepthDiffuse;
	int maxRayDepthGlossy;
	int maxRayDepthRefraction;
	int maxRayDepthGlossyRefraction;
	int maxRayDepthShadow;

	// for viewport
	int viewportMaxRayDepth;
	int viewportMaxDiffuseRayDepth;
	int viewportMaxReflectionRayDepth;

	// Command port
	int commandPort;

	// True if FireRender gamma correction should be applied to
	// the frame data that gets sent to Maya views. Defaults to
	// false because Maya uses it's own gamma correction by default.
	bool applyGammaToMayaViews = false;

	// Display gamma.
	float displayGamma = 1;

	// Texture gamma.
	float textureGamma = 1;

	// Render stamp
	bool useRenderStamp;
	MString renderStampText;

	//tone mapping
	short toneMappingType;

	bool toneMappingWhiteBalanceEnabled = false;
	float toneMappingWhiteBalanceValue = 3200;

	// Tone mapping linear scale
	float toneMappingLinearScale;

	// Tone mapping photo-linear sensitivity
	float toneMappingPhotolinearSensitivity;

	// Tone mapping photo-linear exposure
	float toneMappingPhotolinearExposure;

	// Tone mapping photo-linear fstop
	float toneMappingPhotolinearFstop;

	// Tone mapping Reinhard02 pre scale
	float toneMappingReinhard02Prescale;

	// Tone mapping Reinhard02 post scale
	float toneMappingReinhard02Postscale;

	// Tone mapping Reinhard02 burn
	float toneMappingReinhard02Burn;

	// Tone mapping simple exposure
	float toneMappingSimpleExposure;

	// Tone mapping simple contrast
	float toneMappingSimpleContrast;

	// Tone mapping simple tonemap
	bool toneMappingSimpleTonemap;

	// Motion blur
	bool motionBlur;
	bool cameraMotionBlur;
	bool viewportMotionBlur;
	bool velocityAOVMotionBlur;
	float motionBlurCameraExposure;
	unsigned int motionSamples;

	// Contour
	bool contourIsEnabled;

	bool contourUseObjectID;
	bool contourUseMaterialID;
	bool contourUseShadingNormal;
	bool contourUseUV;

	float contourLineWidthObjectID;
	float contourLineWidthMaterialID;
	float contourLineWidthShadingNormal;
	float contourLineWidthUV;

	float contourNormalThreshold;
	float contourUVThreshold;
	float contourAntialiasing;

	bool contourIsDebugEnabled;

	// Camera type.
	short cameraType;

	bool tileRenderingEnabled;
	int tileSizeX;
	int tileSizeY;

	// AOVs.
	FireRenderAOVs aovs;

	// Advanced settings
	float raycastEpsilon;

	// - out of core textures
	bool enableOOC;
	unsigned int oocTexCache;

	DenoiserSettings denoiserSettings;
	AirVolumeSettings airVolumeSettings;

	// Use Metal Performance Shaders for MacOS
	bool useMPS;

	bool useDetailedContextWorkLog;

	//Deep EXR
	float deepEXRMergeZThreshold;

	// Cryptomatte settings
	bool cryptomatteExtendedMode;
	bool cryptomatteSplitIndirect;

	// SC and RC settins
	bool shadowCatcherEnabled;
	bool reflectionCatcherEnabled;

	// use legacy OpenCL context
	bool useOpenCLContext;

private:
	short getMaxRayDepth(const FireRenderContext& context) const;
	short getSamples(const FireRenderContext& context) const;

	void readDenoiserParameters(const MFnDependencyNode& frGlobalsNode);
	void readAirVolumeParameters(const MFnDependencyNode& frGlobalsNode);
};

namespace FireMaya
{
	class Options
	{
	public:
		static int GetContextDeviceFlags(RenderType renderType = RenderType::ProductionRender);
	};
}

MString GetPropertyNameFromPlugName(const MString& name);

// Check if the object is visible
bool isVisible(MFnDagNode & fnDag, MFn::Type type);
bool isGeometry(const MObject& node);
bool isLight(const MObject& node);
bool isTransformWithInstancedShape(const MObject& node, MDagPath& nodeDagPath, bool& isGPUCacheNode);

double rad2deg(double radians);
double deg2rad(double degrees);

bool findMeshShapeForMeshPhysicalLight(const MDagPath&  physicalLightPath, MDagPath& shapeDagPath);

// Get visible objects
MStatus getVisibleObjects(MDagPathArray& dagArray, MFn::Type type, bool checkVisibility = true);

// Get connected shaders
MObjectArray getConnectedShaders(MDagPath& meshPath);

// Get connected meshes to the shader
MObjectArray getConnectedMeshes(MObject& shaderNode);

// Get node with this uuid
MObject getNodeFromUUid(const std::string& uuid);

// Get the node uuid
std::string getNodeUUid(const MObject& node);
std::string getNodeUUid(const MDagPath& node);

// Get environment light object
MDagPath getEnvLightObject(MStatus& status);

// Get sky light object
MDagPath getSkyObject(MStatus& status);

// Get shader cache path
MString getShaderCachePath();

//Get if shaders have been cached (Shader System)
int areShadersCached();

// Get the log folder.
MString getLogFolder();

// Get the path to the project source images.
MString getSourceImagesPath();

// Get surface shader
MObject getSurfaceShader(MObject& shadingEngine);

// Get volume shader
MObject getVolumeShader( MObject& shadingEngine );

//
MObject getDisplacementShader(MObject& shadingEngine);
// Get shading engines
MObjectArray GetShadingEngines(MFnDagNode& node, uint instance);
int GetFaceMaterials(MFnMesh& mesh, MIntArray& faceList);

// Get All Cameras
// - renderableOnly - Get only render-able cameras
MDagPathArray GetSceneCameras(bool renderableOnly = false);

// get shape name by dag path
MString getNameByDagPath(const MDagPath& cameraPath);

// Get camera that is used by production render if no camera is selected
MDagPath getDefaultCamera();

// Get defaultRenderGlobals node
MStatus GetDefaultRenderGlobals(MObject& outGlobalsNode);

// Get RadeonProRenderGlobals node
MStatus GetRadeonProRenderGlobals(MObject& outGlobalsNode);

// Get Plug from RadeonProRenderGlobals
MPlug GetRadeonProRenderGlobalsPlug(const char* name, MStatus* status = nullptr);

// Is IBL image fliping switched on
bool IsFlipIBL();

// Get Render Size from Common Tab
void GetResolutionFromCommonTab(unsigned int& width, unsigned int& height);

class HardwareResources
{
	HardwareResources();
	static const HardwareResources& GetInstance();
public:
	struct Device
	{
		std::string name;
		rpr_creation_flags creationFlag;
		RPR_TOOLS_COMPATIBILITY compatibility;
		bool isDriverCompatible = true;

		bool isCertified() const {
			return compatibility == RPRTC_COMPATIBLE;
		}
		bool isCpu() const {
			return !!(creationFlag & RPR_CREATION_FLAGS_ENABLE_CPU);
		}
	};

	struct Driver
	{
		std::wstring name;
		std::wstring deviceName;
		bool isAMD = false;
		bool isNVIDIA = false;
		bool compatible = true;
	};

	static std::vector<Device> GetCompatibleCPUs();
	static std::vector<Device> GetCompatibleGPUs(bool onlyCertified = false);
	static std::vector<Device> GetAllDevices();

private:

	std::vector<Device> _devices;
	std::vector<Driver> _drivers;

	void initializeDrivers();
	void populateDrivers();
	void updateDriverCompatibility();
	bool isDriverCompatible(std::string deviceName);
	bool compareDeviceNames(std::wstring& a, std::wstring& b);
	bool parseNVIDIADriver(const std::wstring & rawDriver, int& publicVersionOut);
};

class Timer
{
	std::string m_name;
	clock_t m_ticks;
public:
	Timer(const char* scopeName = "")
	{
		m_name = scopeName;
		m_ticks = clock();
	}
	~Timer()
	{
		if (!m_name.empty())
		{
			LogPrint("%s timing: %f seconds", m_name.c_str(), Seconds());
		}
	}
	void Restart()
	{
		m_ticks = clock();
	}
	double Seconds()
	{
		return double(clock() - m_ticks) / CLOCKS_PER_SEC;
	}
};

template<typename C, typename T>
class MayaCIterator : public std::iterator<std::forward_iterator_tag, T>
{
	const C& m_container;
	unsigned int m_i;
public:
	MayaCIterator(const C& c, unsigned int i = 0u)
		: m_container(c), m_i(i)
	{
	}

	const T operator*() const
	{
		return m_container[m_i];
	}

	MayaCIterator<C, T>& operator++()
	{
		++m_i;
		return *this;
	}

	MayaCIterator<C, T> operator++(int)
	{
		auto&& tmp = *this;
		operator++();
		return tmp;
	}

	bool operator==(MayaCIterator<C, T> const& other) const
	{
		return m_i == other.m_i;
	}

	bool operator!=(MayaCIterator<C, T> const& other) const
	{
		return !(*this == other);
	}
};

template<typename C, typename T>
class MayaIterator : public std::iterator<std::forward_iterator_tag, T>
{
	C& m_container;
	unsigned int m_i;
public:
	MayaIterator(C& c, unsigned int i = 0u)
		: m_container(c), m_i(i)
	{
	}

	T& operator*() const
	{
		return m_container[m_i];
	}

	MayaIterator<C, T>& operator++()
	{
		++m_i;
		return *this;
	}

	MayaIterator<C, T> operator++(int)
	{
		auto&& tmp = *this;
		operator++();
		return tmp;
	}

	bool operator==(MayaIterator<C, T> const& other) const
	{
		return m_i == other.m_i;
	}

	bool operator!=(MayaIterator<C, T> const& other) const
	{
		return !(*this == other);
	}
};

#if MAYA_API_VERSION >= 20180000
OPENMAYA_MAJOR_NAMESPACE_OPEN
#endif

inline MayaIterator<MDagPathArray, MDagPath> begin(MDagPathArray &arr)
{
	return MayaIterator<MDagPathArray, MDagPath>(arr);
}

inline MayaIterator<MDagPathArray, MDagPath> end(MDagPathArray &arr)
{
	return MayaIterator<MDagPathArray, MDagPath>(arr, arr.length());
}

inline MayaIterator<MObjectArray, MObject> begin(MObjectArray &arr)
{
	return MayaIterator<MObjectArray, MObject>(arr);
}

inline MayaIterator<MObjectArray, MObject> end(MObjectArray &arr)
{
	return MayaIterator<MObjectArray, MObject>(arr, arr.length());
}

inline MayaIterator<MPlugArray, MPlug> begin(MPlugArray &arr)
{
	return MayaIterator<MPlugArray, MPlug>(arr);
}

inline MayaIterator<MPlugArray, MPlug> end(MPlugArray &arr)
{
	return MayaIterator<MPlugArray, MPlug>(arr, arr.length());
}

inline MayaIterator<MIntArray, int> begin(MIntArray &arr)
{
	return MayaIterator<MIntArray, int>(arr);
}

inline MayaIterator<MIntArray, int> end(MIntArray &arr)
{
	return MayaIterator<MIntArray, int>(arr, arr.length());
}

inline MayaCIterator<MDagPathArray, MDagPath> begin(const MDagPathArray &arr)
{
	return MayaCIterator<MDagPathArray, MDagPath>(arr);
}

inline MayaCIterator<MDagPathArray, MDagPath> end(const MDagPathArray &arr)
{
	return MayaCIterator<MDagPathArray, MDagPath>(arr, arr.length());
}

inline MayaCIterator<MStringArray, MString> begin(const MStringArray &arr)
{
	return MayaCIterator<MStringArray, MString>(arr);
}

inline MayaCIterator<MStringArray, MString> end(const MStringArray &arr)
{
	return MayaCIterator<MStringArray, MString>(arr, arr.length());
}

#if MAYA_API_VERSION >= 20180000
OPENMAYA_NAMESPACE_CLOSE
#endif

template<typename T>
inline T findPlugTryGetValue(const MFnDependencyNode & mfnDepNode, const MString& plugName, T defaultValue, bool failOnNotFound = true)
{
	auto plug = mfnDepNode.findPlug(plugName);

	T retValue = defaultValue;
	if (!plug.isNull())
	{
		auto mstatus = plug.getValue(retValue);
        if (failOnNotFound && (!mstatus))
            assert(false);
	}
	else
	{
		auto name = mfnDepNode.name();

		DebugPrint("Failed to find the plug: %s in %s", plugName.asUTF8(), name.asUTF8());

		if(failOnNotFound)
			assert(false);
	}

	return retValue;
}

template<typename T>
inline T findPlugTryGetValue(const MFnDependencyNode& mfnDepNode, const MObject& attr, T defaultValue, bool failOnNotFound = true)
{
	auto plug = mfnDepNode.findPlug(attr);

	T result = defaultValue;

	if (plug.isNull())
	{
		DebugPrint("Failed to find an attribute in %s", mfnDepNode.name().asUTF8());

		if (failOnNotFound)
		{
			assert(false);
		}
	}
	else
	{
		auto status = plug.getValue(result);

		if (!status)
		{
			DebugPrint("Failed to find an attribute in %s", mfnDepNode.name().asUTF8());

			if (failOnNotFound)
			{
				assert(false);
			}
		}
	}

	return result;
}

inline MColor getColorAttribute(const MFnDependencyNode& node, const MObject& attribute)
{
    MPlug plug = node.findPlug(attribute);

    assert(!plug.isNull());

    if (!plug.isNull())
    {
        float r, g, b;

        plug.child(0).getValue(r);
        plug.child(1).getValue(g);
        plug.child(2).getValue(b);

        return MColor(r, g, b);
    }

    return MColor::kOpaqueBlack;
}

inline MString operator "" _ms(const char * str, std::size_t len)
{
	return MString(str, static_cast<int>(len));
}

template<typename T>
inline MString toMString(const T & data)
{
	return ""_ms + data;
}

template<>
inline MString toMString(const bool & data)
{
	return data ? "1"_ms : "0"_ms;
}

class MStringComparison
{
public:
	// Less then comparison
	bool operator() (const MString & first, const MString & second) const
	{
		auto ret = strcmp(first.asUTF8(), second.asUTF8());

		return ret < 0;
	}
};

struct Float3
{
	float x, y, z;

	Float3()
		: x (0.0f)
		, y (0.0f)
		, z (0.0f)
	{}

	Float3(const MFloatPoint& v)
	{
		x = v.x;
		y = v.y;
		z = v.z;
	}
	Float3(const MFloatVector& v)
	{
		x = v.x;
		y = v.y;
		z = v.z;
	}
	bool operator<(const Float3& b) const
	{
		if (x < b.x)
			return true;
		if (x > b.x)
			return false;
		if (y < b.y)
			return true;
		if (y > b.y)
			return false;
		return z < b.z;
	}
};

struct Float2
{
	float x, y;
	bool operator<(const Float2& b) const
	{
		if (x < b.x)
			return true;
		if (x > b.x)
			return false;
		return y < b.y;
	}

	Float2()
		: x (0.0f)
		, y (0.0f)
	{}

	Float2(float _x, float _y)
		: x (_x)
		, y (_y)
	{}
};

MObject findDependNode(MString name);

// Same environment variable used in the Blender addon
//
bool isMetalOn();

// Maya always returns all lengths in centimeters despite the settings in Preferences (detected experimentally)
inline float GetSceneUnitsConversionCoefficient(void)
{
	const static float cmToMCoefficient = 0.01f;
	return cmToMCoefficient;
}

/**
* Disconnect everything connected to Plug
* Returns true if successfull
*/
bool DisconnectFromPlug(MPlug& plug);

void SetCameraLookatForMatrix(rpr_camera camera, const MMatrix& matrix);

/*
* Auxiliary function and struct for extracting data from UI Ramp 
*/
enum class InterpolationMethod
{
	kNone,
	kLinear,
	kSpline,
	kSmooth,
};

// representation of Control Point on Ramp in Maya UI
template <class T> 
struct RampCtrlPoint // T is either MColor or float
{
	T ctrlPointData; // data point or Y input axis on graph
	InterpolationMethod method;
	unsigned int index;
	float position; // X input axis on graph

	using CtrlPointDataContainerT = T;

	RampCtrlPoint() : method(InterpolationMethod::kNone), index(UINT_MAX) {}
};

// function to get value between prev and next point on ramp corresponding to positionOnRamp using Linear Interpolation
template <typename valType>
auto RampLerp(const RampCtrlPoint<valType>& prev, const RampCtrlPoint<valType>& next, float positionOnRamp)
{
	float coef = ((positionOnRamp - prev.position) / (next.position - prev.position));

	valType prevValue = prev.ctrlPointData;
	valType nextValue = next.ctrlPointData;
	valType calcRes = nextValue - prevValue;

	calcRes = coef * calcRes;
	calcRes = calcRes + prevValue;

	return calcRes;
}

// function to get array of values of type valType that are calculated by ramp control points values in inputControlPoints
// is used to get values from maya ramp that can be passed to RPR
template <typename valType>
void RemapRampControlPoints(
	size_t countOutputPoints,
	std::vector<valType>& output,
	const std::vector<RampCtrlPoint<valType>>& inputControlPoints)
{
	// 0 elements
	if (inputControlPoints.size() == 0)
	{
		return;
	}
	output.clear();
	output.reserve(countOutputPoints);

	auto itCurr = inputControlPoints.begin();
	auto itNext = inputControlPoints.begin(); ++itNext;
	// 1 element
	if (itNext == inputControlPoints.end())
	{
		output.push_back(itCurr->ctrlPointData);
		return;
	}

	// many elements
	for (size_t idx = 0; idx < countOutputPoints; ++idx)
	{
		float positionOnRamp = (1.0f / (countOutputPoints - 1)) * idx;

		if (positionOnRamp > itNext->position)
		{
			itNext++;
			itCurr++;
		}

		if (itNext == inputControlPoints.end())
		{
			output.push_back(itCurr->ctrlPointData);
			continue;
		}

		valType remappedValue = RampLerp(*itCurr, *itNext, positionOnRamp); // only linear interpolation is currently supported

		output.push_back(remappedValue);
	}
}

template <typename valType>
void RemapRampControlPointsNoInterpolation(
	size_t countOutputPoints,
	std::vector<valType>& output,
	const std::vector<RampCtrlPoint<valType>>& inputControlPoints)
{
	output.clear();
	output.reserve(countOutputPoints);

	auto itCurr = inputControlPoints.begin();
	auto itNext = inputControlPoints.begin(); ++itNext;

	for (size_t idx = 0; idx < countOutputPoints; ++idx)
	{
		float positionOnRamp = (1.0f / (countOutputPoints - 1)) * idx;

		if (positionOnRamp > itNext->position)
		{
			itNext++;
			itCurr++;
		}

		output.push_back(itCurr->ctrlPointData);
	}
}

template <typename MayaDataContainer, typename valType>
void AssignCtrlPointValue(RampCtrlPoint<valType>& ctrlPoint, const MayaDataContainer& dataVals, unsigned int idx)
{
	ctrlPoint.ctrlPointData = dataVals[idx];
}

template <typename MayaDataContainer, typename... List> // tuple for compound ctrl point value
void AssignCtrlPointValue(RampCtrlPoint<std::tuple<List...>>& ctrlPoint, const MayaDataContainer& dataVals, unsigned int idx)
{
	using MayaElementT = decltype(
		std::declval<MayaDataContainer&>()[std::declval<unsigned int>()]
		);

	std::get<typename std::remove_reference<MayaElementT>::type>(ctrlPoint.ctrlPointData) = dataVals[idx];
}

// internal function to grab ramp control points from maya Ramp
// shouldn't be used directly
template <class MayaDataContainer, class valType>
bool SaveCtrlPoints(
	MayaDataContainer& dataVals,
	MFloatArray& positions,
	MIntArray& interps,
	MIntArray& indexes,
	std::vector<RampCtrlPoint<valType>>& out)
{
	out.clear();

	// ensure correct input
	unsigned int length = dataVals.length();
	if (length == 0)
	{
		return false;
	}
	if ((length != positions.length()) || 
		(length != interps.length()) || 
		(length != indexes.length())
		)
		return false;

	// copy data values
	for (unsigned int idx = 0; idx < length; ++idx)
	{
		out.emplace_back();
		auto& ctrlPointRef = out.back();
		AssignCtrlPointValue(ctrlPointRef, dataVals, idx);
		ctrlPointRef.method = (InterpolationMethod) interps[idx];
		ctrlPointRef.position = positions[idx];
		ctrlPointRef.index = indexes[idx];
	}

	// sort values by position (maya returns control point in random order)
	std::sort(out.begin(), out.end(), [](auto first, auto second)->bool { 
		return (first.position < second.position); });

	// add new control point for position == 1.0 
	// last position on graph; 
	// control point for this position is not generated by maya, 
	// but is very convinient for further calculations)
	if ((1.0f - out.back().position) > FLT_EPSILON)
	{
		out.emplace_back();
		const auto& last = out.end() - 2;
		auto& ctrlPointRef = out.back();
		ctrlPointRef.ctrlPointData = last->ctrlPointData;
		ctrlPointRef.method = last->method;
		ctrlPointRef.position = 1.0f;
		ctrlPointRef.index = 99; // this value is irrelevant
	}

	return true;
}

// function to grab ramp control points from maya Ramp
// use this to get control points array from MPlug
template <class MayaDataContainer, class valType>
bool GetRampValues(MPlug& plug, std::vector<RampCtrlPoint<valType>>& out)
{
	// get ramp from plug
	MRampAttribute valueRamp(plug);

	// get data from plug
	MIntArray indexes;
	MFloatArray positions;
	MayaDataContainer dataValues;
	MIntArray interps;
	MStatus returnStatus;

	valueRamp.getEntries(
		indexes,
		positions,
		dataValues,
		interps,
		&returnStatus
	);

	if (returnStatus != MS::kSuccess)
		return false;

	// save data
	return SaveCtrlPoints<MayaDataContainer, valType>(dataValues, positions, interps, indexes, out);
}

template <typename MayaDataContainer>
bool SetRampValues(MPlug& plug, const MayaDataContainer& values)
{
	// get ramp from plug
	MRampAttribute valueRamp(plug);

	unsigned int len = values.length();

	MIntArray interps(len, MRampAttribute::kLinear);

	MFloatArray positions(len, 0.0f);
	if (len > 1)
	{
		for (unsigned int idx = 0; idx < len; ++idx)
		{
			positions[idx] = idx * 1.0f / (len - 1);
		}
	} else {
		for (unsigned int idx = 0; idx < len; ++idx)
		{
			positions[idx] = 0;
		}
	}
	MStatus status;
	valueRamp.setRamp(values, positions, interps);
	if (status != MS::kSuccess)
		return false;

	return true;
}

// function to grab ramp control points from maya Ramp
// takes MObject rampObject and MString rampKey 
// use this to get control points array 
// - notice that in maya calls to ramp plug usually looks like: rampPlugName[0].SetSomething(...)
// - the plug name in this case will be "rampPlugName"
template<typename MayaDataContainer, typename valType>
bool GetValuesFromUIRamp(MObject rampObject, const MString& rampKey, std::vector<RampCtrlPoint<valType>>& outRampCtrlPoints)
{
	MFnDependencyNode shaderNode(rampObject);
	MPlug rampPlug = shaderNode.findPlug(rampKey);

	bool found = !rampPlug.isNull();
	if (!found)
		return false;

	bool res = GetRampValues<MayaDataContainer>(rampPlug, outRampCtrlPoints);
	if (!res)
		return false;

	return true;
}

template<typename valType>
frw::BufferNode CreateRPRRampNode(std::vector<RampCtrlPoint<valType>>& rampCtrlPoints, const FireMaya::Scope& scope, const unsigned int bufferSize)
{
	// ensure correct input
	if (rampCtrlPoints.size() == 0)
		return frw::BufferNode(scope.MaterialSystem());

	// create buffer desc
	rpr_buffer_desc bufferDesc;
	bufferDesc.nb_element = bufferSize;
	bufferDesc.element_type = RPR_BUFFER_ELEMENT_TYPE_FLOAT32;
	bufferDesc.element_channel_size = 4;

	// convert control points into continious vector of data
	std::vector<valType> remapedRampValue(bufferSize, valType());
	RemapRampControlPoints(remapedRampValue.size(), remapedRampValue, rampCtrlPoints);

	// create buffer
	frw::DataBuffer dataBuffer(scope.Context(), bufferDesc, &remapedRampValue[0][0]);

	// create buffer node
	frw::BufferNode bufferNode(scope.MaterialSystem());
	bufferNode.SetBuffer(dataBuffer);

	return bufferNode;
}

// get path to hipbin folder from Maya enviromental variable
std::string GetPathToHipbinFolder(void);

// wrapper for maya call to Python
static std::function<int(std::string)> pythonCallWrap = [](std::string arg)->int
{
	MStatus res = MGlobal::executePythonCommandOnIdle(MString(arg.c_str()));

	if (res != MStatus::kSuccess)
		return 1;

	return 0;
};

// common helper function
void setAttribProps(MFnAttribute& attr, const MObject& attrObj);

void CreateBoxGeometry(std::vector<float>& veritces, std::vector<float>& normals, std::vector<int>& vertexIndices, std::vector<int>& normalIndices);

template <typename OutT, typename MayaArrayT>
void WriteMayaArrayTo(std::vector<OutT>& out, const MayaArrayT& source)
{
	using MayaElementT = decltype(
		std::declval<MayaArrayT&>()[std::declval<unsigned int>()]
	);
	static_assert(std::is_same<MayaElementT, OutT&>::value, "array type mismatch!");

	int length = source.length();
	out.clear();
	out.resize(length, OutT());
	source.get(out.data());
}

std::vector<MString> dumpAttributeNamesDbg(MObject node);

RenderQuality GetRenderQualityFromPlug(const char* plugName);
RenderQuality GetRenderQualityForRenderType(RenderType renderType);

bool CheckIsInteractivePossible();


template <class T>
std::vector<T> splitString(const T& s, typename T::traits_type::char_type delim)
{
	std::vector<T> elems;
	for (size_t p = 0, q = 0; p != s.npos; p = q)
	{
		elems.push_back(s.substr(p + (p != 0), (q = s.find(delim, p + 1)) - p - (p != 0)));
	}

	return elems;
}

// Backdoor to enable different AOVs from Render Settings in IPR and Viewport
void EnableAOVsFromRSIfEnvVarSet(FireRenderContext& context, FireRenderAOVs& aovs);

template<typename ... Args>
std::string string_format(const std::string& format, Args ... args)
{
	size_t size = snprintf(nullptr, 0, format.c_str(), args ...) + 1; // Extra space for '\0'
	if (size <= 0) 
	{ 
		throw std::runtime_error("Error during formatting."); 
	}

	std::unique_ptr<char[]> buf(new char[size]);
	snprintf(buf.get(), size, format.c_str(), args ...);
	return std::string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
}

void GetUINameFrameExtPattern(std::wstring& nameOut, std::wstring& extOut);

#ifdef __APPLE__ // https://stackoverflow.com/questions/31346887/header-for-environ-on-mac
extern char **environ;
#endif

template<typename T>
T* GetEnviron(void);

bool IsUnicodeSystem(void);

template <typename T>
std::pair<T, T> GetPathDelimiter(void);

// replaces environment variables in file path string into actual paths
template <typename T, typename C>
T ProcessEnvVarsInFilePath(const C* in);

template <typename T>
size_t FindDelimiter(T& tmp);

template <typename T>
class EnvironmentVarsWrapper
{
	using xstring = std::basic_string<T, std::char_traits<T>, std::allocator<T> >;

public:
	static const std::map<xstring, xstring>& GetEnvVarsTable(void)
	{
		static EnvironmentVarsWrapper instance;

		return instance.m_eVars;
	}

	EnvironmentVarsWrapper(EnvironmentVarsWrapper const&) = delete;
	void operator=(EnvironmentVarsWrapper const&) = delete;

private:
	EnvironmentVarsWrapper(void)
	{
		T* pEnvVarPair = *GetEnviron<T*>();
		for (int idx = 1; pEnvVarPair; idx++)
		{
			xstring tmp(pEnvVarPair);
			size_t delimiter = FindDelimiter(tmp);
			xstring varName = tmp.substr(0, delimiter);
			xstring varValue = tmp.substr(delimiter + 1, tmp.length());

			m_eVars[varName] = varValue;
			pEnvVarPair = *(GetEnviron<T*>() + idx);
		};
	}

private:
	std::map<xstring, xstring> m_eVars;
};

template <typename T>
std::tuple<T, T> ProcessEVarSchema(const T& eVar);

template <typename T, typename C>
T ProcessEnvVarsInFilePath(const C* in)
{
	T out(in);

	const std::map<T, T>& eVars = EnvironmentVarsWrapper<C>::GetEnvVarsTable();

	// find environment variables in the string
	// and replace them with real path
	for (auto& eVar : eVars)
	{
		auto processedEVar = ProcessEVarSchema(eVar.first);

		T tmpVar = std::get<0>(processedEVar);
		size_t found = out.find(tmpVar);

		if (found == T::npos)
		{
			tmpVar = std::get<1>(processedEVar);
			found = out.find(tmpVar);
		}

		if (found == T::npos)
			continue;

		out.replace(found, tmpVar.length(), eVar.second);
	}

	// replace "\\" with "/"
	static const std::pair<T, T> toBeReplaced = GetPathDelimiter<T>();
	while (out.find(toBeReplaced.first) != T::npos)
	{
		out.replace(out.find(toBeReplaced.first), toBeReplaced.first.size(), toBeReplaced.second);
	}

	return out;
}

TimePoint GetCurrentChronoTime();

template <typename T>
long TimeDiffChrono(TimePoint currTime, TimePoint startTime)
{
	return (long)std::chrono::duration_cast<T>(currTime - startTime).count();
}


