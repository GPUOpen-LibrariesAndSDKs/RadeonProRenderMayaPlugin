#pragma once
// Copyright (C) AMD
//
// File: FireRenderUtils.h
//
// Utility classes
//
// Created by Alan Stanzione.
//

#if MAYA_API_VERSION >= 20180000
#include <maya/MApiNamespace.h>
#endif
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
#include <../RprTools.h>
#include <algorithm>
#include <iterator>
#include <array>
#include <float.h>

// FireRenderGlobals
// Utility class used to read attributes form the render global node
// and configure the rpr_context

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
	void updateTonemapping(class FireRenderContext& frcontext, bool disableWhiteBalance = false);

	// Setup the rpr_context
	void setupContext(class FireRenderContext& frcontext, bool disableWhiteBalance = false);

	// Checks if name is one of the options are tonemapping options
	static bool isTonemapping(MString name);

	// Checks if name is one of the options are denoiser options
	static bool isDenoiser(MString name);

	static void getCPUThreadSetup(bool& overriden, int& cpuThreadCount, RenderType renderType);
	static int getThumbnailIterCount();
	static bool isExrMultichannelEnabled(void);

public:

	// Completion criteria.
	CompletionCriteriaParams completionCriteriaFinalRender;
	CompletionCriteriaParams completionCriteriaViewport;

	int adaptiveTileSize;
	float adaptiveThreshold;

	bool textureCompression;

	int viewportRenderMode;
	int renderMode;

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

	// Motion blur
	bool motionBlur;
	float motionBlurCameraExposure;
	float motionBlurScale;

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

	// Use Metal Performance Shaders for MacOS
	bool useMPS;

private:
	short getMaxRayDepth(const FireRenderContext& context) const;
	short getSamples(const FireRenderContext& context) const;

	void readDenoiserParameters(const MFnDependencyNode& frGlobalsNode);
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
bool isTransformWithInstancedShape(const MObject& node, MDagPath& nodeDagPath);

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

// Get render-able cameras
MDagPathArray getRenderableCameras();

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

template <class T> 
struct RampCtrlPoint // T is either MColor or float
{
	T ctrlPointData; // data point or Y input axis on graph
	InterpolationMethod method;
	unsigned int index;
	float position; // X input axis on graph

	RampCtrlPoint() : method(InterpolationMethod::kNone) {}
};

template <class MayaDataContainer, class valType>
bool SaveCtrlPoints(
	MayaDataContainer& dataVals,
	MFloatArray& positions,
	MIntArray& interps,
	MIntArray& indexes,
	std::vector<RampCtrlPoint<valType>>& out)
{
	// ensure correct input
	unsigned int length = dataVals.length();
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
		ctrlPointRef.ctrlPointData = dataVals[idx];
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

template <class MayaDataContainer, class valType>
bool GetValues(MPlug& plug, std::vector<RampCtrlPoint<valType>>& out)
{
	// get ramp from plug
	MRampAttribute valueRamp(plug);
	unsigned int countCtrlPoints = valueRamp.getNumEntries();

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

// common helper function
void setAttribProps(MFnAttribute& attr, const MObject& attrObj);

void CreateBoxGeometry(std::vector<float>& veritces, std::vector<float>& normals, std::vector<int>& vertexIndices, std::vector<int>& normalIndices);



void dumpFloatArrDbg(std::vector<float>& out, const MFloatArray& source);
