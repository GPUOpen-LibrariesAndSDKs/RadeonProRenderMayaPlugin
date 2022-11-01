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
#include "Translators/Translators.h"
#include <maya/MNodeMessage.h>
#include <maya/MDagMessage.h>
#include <maya/MPlug.h>
#include <maya/MFnFluid.h>
#include <string>
#include <atomic>
#include "FireMaya.h"

#include "PhysicalLightData.h"

// Forward declarations
class FireRenderContext;
class SkyBuilder;

class HashValue
{
	const static size_t BigDumbPrime = 0x1fffffffffffffff;
	size_t value = 0;

	template<class T>
	size_t HashItems(const T* v, int count, size_t ret)
	{
		auto n = sizeof(T) * count;
		auto p = reinterpret_cast<const unsigned char*>(v);

		if (!p)
			return (ret >> 17 | ret << 47) ^ ((n + ret) * BigDumbPrime);

		for (int i = 0; i < n; i++)
			ret = (ret >> 17 | ret << 47) ^ ((p[i] + i + 1 + ret) * BigDumbPrime);

		return ret;
	}

public:
	HashValue(size_t v = 0) : value(v) {}

	bool operator==(const HashValue& h) const { return value == h.value; }
	bool operator!=(const HashValue& h) const { return value != h.value; }

	template <class T>
	HashValue& operator<<(const T& v)
	{
		value = HashItems(&v, 1, value);
		return *this;
	}

	template <class T>
	void Append(const T* v, int count)
	{
		value = HashItems(v, count, value);
	}

	operator size_t() const { return value; }
	operator int() const
	{
		return  int((value >> 32) ^ value);
	}
};

// FireRenderObject
// Base class for each translated object
class FireRenderObject
{

protected:

	struct
	{
		// Plug dirty flag
		// this is true when Maya is evaluating the plug dirty callback
		// used to avoid race conditions between the plug dirty and the attribute changed callbacks
		std::list<MCallbackId> callbackId;
		FireRenderContext* context = nullptr;
		std::string uuid;
		MObject		object;
		HashValue	hash;
		unsigned int instance = 0;
	} m;
public:

	// Constructor
	FireRenderObject(FireRenderContext* context, const MObject& object = MObject::kNullObj);

	// Copy constuctor with custom uuid
	FireRenderObject(const FireRenderObject& rhs, const std::string& uuid);

	// Destructor
	virtual ~FireRenderObject();

	// Clear
	virtual void clear();

	// uuid
	std::string uuid() const;
	std::string uuidWithoutInstanceNumber() const;

	// Set dirty
	void setDirty();

	static void Dump(const MObject& ob, int depth = 0, int maxDepth = 4);
	static HashValue GetHash(const MObject& ob);

	static std::string uuidWithoutInstanceNumberForString(const std::string& uuid);

	// update fire render objects using Maya objects, then marks as clean
	virtual void Freshen(bool shouldCalculateHash);

	virtual bool IsMesh(void) const { return false; }
	virtual bool ReloadMesh(unsigned int sampleIdx = 0) { return false; }
	virtual bool ShouldForceReload(void) const { return false; }

	// hash is generated during Freshen call
	HashValue GetStateHash() { return m.hash; }

	// Return the render context
	FireRenderContext* context() { return m.context; }
	const FireRenderContext* context() const { return m.context; }

	frw::Scene		Scene();
	frw::Context	Context();
	FireMaya::Scope Scope();

	const MObject& Object() const { return m.object; }
	void SetObject(const MObject& ob);

	void AddCallback(MCallbackId id);
	void ClearCallbacks();
	virtual void RegisterCallbacks();

	template <class T>
	static T GetPlugValue(const MObject& ob, const char* name, T defaultValue)
	{
		MStatus mStatus;
		MFnDependencyNode node(ob, &mStatus);
		if (mStatus == MStatus::kSuccess)
		{
			auto displayPlug = node.findPlug(name);
			if (!displayPlug.isNull())
				displayPlug.getValue(defaultValue);
		}
		return defaultValue;
	}

	template <class T>
	T GetPlugValue(const char* name, T defaultValue)
	{
		return GetPlugValue(Object(), name, defaultValue);
	}

protected:
	// node dirty
	virtual void OnNodeDirty();
	static void NodeDirtyCallback(MObject& node, void* clientData);

	// attribute changed
	virtual void attributeChanged(MNodeMessage::AttributeMessage msg, MPlug &plug, MPlug &otherPlug) {}
	static void attributeChanged_callback(MNodeMessage::AttributeMessage msg, MPlug& plug, MPlug& otherPlug, void* clientData);
	static void attributeAddedOrRemoved_callback(MNodeMessage::AttributeMessage msg, MPlug& plug, void* clientData);

	// Plug dirty
	virtual void OnPlugDirty(MObject& node, MPlug& plug);
	static void plugDirty_callback(MObject& node, MPlug& plug, void* clientData);
	void SetAllChildrenDirty();

	virtual HashValue CalculateHash();

public:
	bool m_isPortal_IBL;
	bool m_isPortal_SKY;
};

class FireRenderNode : public FireRenderObject
{
public:
	// Constructor
	FireRenderNode(FireRenderContext* context, const MDagPath& dagPath);

	// Copy constructor with custom uuid.
	// In the future should be deleted, because FireRenderMesh should inherit from FireRenderObject, so there no need in this constructor with custom uuid.
	FireRenderNode(const FireRenderNode& rhs, const std::string& uuid);

	virtual ~FireRenderNode();

	// Detach from the fire render scene
	virtual void detachFromScene() {};

	// Attach to the fire render scene
	virtual void attachToScene() {};

	virtual bool IsEmissive() { return false; }

	// transform change callback
	// Plug dirty
	virtual void OnPlugDirty(MObject& node, MPlug &plug) override;
	virtual HashValue CalculateHash() override;

	virtual void OnWorldMatrixChanged();
	static void WorldMatrixChangedCallback(MObject& transformNode, MDagMessage::MatrixModifiedFlags& modified, void* clientData);

	virtual void RegisterCallbacks() override;

	bool IsVisible() const { return m_isVisible; }

	std::vector<frw::Shape> GetVisiblePortals();

	MDagPath DagPath();
	unsigned int Instance() const { return m.instance; }

	virtual MMatrix GetSelfTransform();

	// Record the state of a portal
	// node before it is used as a portal.
	void RecordPortalState(MObject node);

	// Restore portal nodes to their original
	// states before being used as portals.
	void RestorePortalStates();

	// Record the state of a portal
	// node before it is used as a portal.
	void RecordPortalState(MObject node, bool iblPortals);

	// Restore portal nodes to their original
	// states before being used as portals.
	void RestorePortalStates(bool iblPortals);

	// The list of currently visible portal
	// nodes and whether they were originally
	//shadow casters before being used as a portal.
	std::vector<std::pair<MObject, bool>> m_visiblePortals;

	std::vector<std::pair<MObject, bool>> m_visiblePortals_IBL;
	std::vector<std::pair<MObject, bool>> m_visiblePortals_SKY;

protected:
	bool m_isVisible = false;
	bool m_bIsTransformChanged = false;

protected:
	virtual void UpdateTransform(const MMatrix& matrix) {}

	void MarkDirtyTransformRecursive(const MFnTransform& transform);
	void MarkDirtyAllDirectChildren(const MFnTransform& transform);
};

// Common class for mesh and gpuCache
class FireRenderMeshCommon : public FireRenderNode
{
public:
	// Constructor
	FireRenderMeshCommon(FireRenderContext* context, const MDagPath& dagPath);

	// Copy constructor with custom uuid
	FireRenderMeshCommon(const FireRenderMeshCommon& rhs, const std::string& uuid);

	// Destructor
	virtual ~FireRenderMeshCommon();

	// for UVs (a bit of a hack; see belo for more info)
	unsigned int GetAssignedUVMapIdx(const MString& textureFile) const;

	void AddForceShaderDirtyDependOnOtherObjectCallback(MObject dependency);
	static void ForceShaderDirtyCallback(MObject& node, void* clientData);

	// Mesh bits (each one may have separate shading engine)
	std::vector<FrElement>& Elements() { return m.elements; }
	const std::vector<FrElement>& Elements() const { return m.elements; }
	FrElement& Element(int i) { return m.elements[i]; }

	bool IsMainInstance() const { return m.isMainInstance; }

	bool IsNotInitialized() const { return m.isPreProcessed; }

	// utility functions
	void setRenderStats(MDagPath dagPath);
	void setVisibility(bool visibility);
	virtual void setReflectionVisibility(bool reflectionVisibility);
	virtual void setRefractionVisibility(bool refractionVisibility);
	virtual void setCastShadows(bool castShadow);
	void setReceiveShadows(bool recieveShadow);
	virtual void setPrimaryVisibility(bool primaryVisibility);
	virtual void setContourVisibility(bool contourVisibility);

	virtual bool IsMesh(void) const { return false; }

	virtual bool InitializeMaterials() { return false; }
	virtual bool ReloadMesh(unsigned int sampleIdx = 0) { return false; }

	// translate mesh
	virtual bool TranslateMeshWrapped(const MDagPath& dagPath, frw::Shape& outShape) { return false; }

	virtual bool IsMeshVisible(const MDagPath& meshPath, const FireRenderContext* context) const = 0;

protected:
	// Detach from the scene
	virtual void detachFromScene() override;

	// Attach to the scene
	virtual void attachToScene() override;


	// materials
	const std::vector<int>& GetFaceMaterialIndices(void) const;

	// utility functions
	virtual void AssignShadingEngines(const MObjectArray& shadingEngines);
	virtual void ProcessMotionBlur(const MFnDagNode& meshFn);

	bool IsMotionBlurEnabled(const MFnDagNode& meshFn);


protected:

	struct
	{
		std::vector<FrElement> elements;
		std::vector<int> faceMaterialIndices;
		bool isEmissive = false;
		bool isMainInstance = false;

		// pre-processed is a state when some mesh data is read from maya but rpr object is not created yet
		// - is re-set to false after rpr object is created
		// - TODO: replace bool with bit field
		bool isPreProcessed = false; 

		struct
		{
			bool mesh = false;
			bool transform = false;
			bool shader = false;
		} changed;
	} m;

	// only one uv coordinates set can be attached to texture file
	// this is the limitation of Maya's relationship editor
	// thus, it is correct to match filename with UV map index
	std::unordered_map<std::string /*texture file name*/, unsigned int /*UV map index*/ > m_uvSetCachedMappingData;
};

// Fire render mesh
// Bridge class between a Maya mesh node and a fr_shape
class FireRenderMesh : public FireRenderMeshCommon
{
public:

	// Constructor
	FireRenderMesh(FireRenderContext* context, const MDagPath& dagPath);

	// Copy constructor with custom uuid
	FireRenderMesh(const FireRenderMesh& rhs, const std::string& uuid);

	// Destructor
	virtual ~FireRenderMesh();

	// Clear
	virtual void clear() override;
	// Register the callback
	virtual void RegisterCallbacks() override;

	// transform attribute changed callback
	virtual void OnNodeDirty() override;

	// Plug dirty
	virtual void OnPlugDirty(MObject& node, MPlug& plug);

	// node dirty
	virtual void OnShaderDirty();

	virtual void attributeChanged(MNodeMessage::AttributeMessage msg, MPlug &plug, MPlug &otherPlug) override;

	static void ShaderDirtyCallback(MObject& node, void* clientData);


	virtual void Freshen(bool shouldCalculateHash) override;

	virtual bool IsMesh(void) const override { return true; }

	virtual bool InitializeMaterials() override;
	virtual bool ReloadMesh(unsigned int sampleIdx = 0) override;
	virtual bool TranslateMeshWrapped(const MDagPath& dagPath, frw::Shape& outShape) override;

	// build a sphere
	void buildSphere();

	virtual bool IsEmissive() override { return m.isEmissive; }

	bool setupDisplacement(std::vector<MObject>& shadingEngines, frw::Shape shape);
	virtual void Rebuild(void);
	void ReinitializeMesh(const MDagPath& meshPath);
	void ProcessMesh(const MDagPath& meshPath);
	void ProcessIBLLight(void);
	void ProcessSkyLight(void);
	void RebuildTransforms(void);

	// used to safely skip pre-processing
	void SetReloadedSafe();

	bool IsInitialized(void) const { return m_meshData.IsInitialized(); }

	virtual bool IsMeshVisible(const MDagPath& meshPath, const FireRenderContext* context) const;

protected:
	void SaveUsedUV(const MObject& meshNode);

	void SetupObjectId(MObject parentTransform);

	void SetupShadowColor();

	bool IsRebuildNeeded();

protected:
	FireMaya::MeshTranslator::MeshPolygonData m_meshData;

private:
	void GetShapes(frw::Shape& outShape);
	
	bool IsSelected(const MDagPath& dagPath) const;

	// A mesh in Maya can have multiple shaders
	// in fr it must be split in multiple shapes
	// so this return the list of all the fr_shapes created for this Maya mesh

	virtual HashValue CalculateHash() override;
	void ProccessSmoothCallbackWorkaroundIfNeeds();

private:
	unsigned int m_SkipCallbackCounter;
};

// Fire render light
// Bridge class between a Maya light node and a frw::Light
class FireRenderLight : public FireRenderNode
{
public:

	// Constructor
	FireRenderLight(FireRenderContext* context, const MDagPath& dagPath);

	// Destructor
	virtual ~FireRenderLight();

	// Return the fr light
	const FrLight& data() { return m_light; }
	FrLight& GetFrLight() { return m_light; }

	// clear
	virtual void clear() override;

	// detach from the scene
	void detachFromScene() override;

	// attach to the scene
	void attachToScene() override;

	virtual bool IsEmissive() override { return true; }

	virtual void Freshen(bool shouldCalculateHash) override;

	// build light for swatch renderer
	void buildSwatchLight();

	// set portal
	void setPortal(bool value);

	// return portal
	bool portal();

protected:
	void UpdateTransform(const MMatrix& matrix) override;

	virtual bool ShouldUpdateTransformOnly() const;

protected:

	// Transform matrix
	MMatrix m_matrix;

	// Fr light
	FrLight m_light;

	// portal flag
	bool m_portal;
};

class FireRenderPhysLight : public FireRenderLight
{
public:
	// Constructor
	FireRenderPhysLight(FireRenderContext* context, const MDagPath& dagPath);

	static PLType GetPhysLightType(MObject dagPath);

protected:
	virtual bool ShouldUpdateTransformOnly() const;
};

// Fire render environment light
// Bridge class between a Maya environment light node and a frw::Light
class FireRenderEnvLight : public FireRenderNode
{
public:

	// Constructor
	FireRenderEnvLight(FireRenderContext* context, const MDagPath& dagPath);

	// Destructor
	virtual ~FireRenderEnvLight();

	// return the frw::Light
	const frw::Light& data();

	// clear
	virtual void clear() override;

	// detach from the scene
	virtual void detachFromScene() override;

	// attach to the scene
	virtual void attachToScene() override;

	virtual void Freshen(bool shouldCalculateHash) override;

	virtual bool IsEmissive() override { return true; }

	inline frw::EnvironmentLight getLight() { return m.light; }

protected:
	virtual void attachToSceneInternal();
	virtual void detachFromSceneInternal();

private:

	// Transform matrix
	MMatrix m_matrix;

public:
	struct
	{
		frw::EnvironmentLight light;
		frw::EnvironmentLight bgOverride;

		frw::Image image;
	} m;
};

class FireRenderEnvLightHybrid : public FireRenderEnvLight
{
public:
	FireRenderEnvLightHybrid(FireRenderContext* context, const MDagPath& dagPath):
		FireRenderEnvLight(context, dagPath) {}

protected:
	void attachToSceneInternal() override;
	void detachFromSceneInternal() override;
};

// Fire render camera
// Bridge class between a Maya camera node and a frw::Camera
class FireRenderCamera : public FireRenderNode
{
public:

	// Constructor
	FireRenderCamera(FireRenderContext* context, const MDagPath& dagPath);

	// Destructor
	virtual ~FireRenderCamera();

	// return the frw::Camera
	const frw::Camera& data();

	// clear
	virtual void clear() override;
	virtual void Freshen(bool shouldCalculateHash) override;

	void TranslateCameraExplicit(int viewWidth, int viewHeight);

	// build camera for swatch render
	void buildSwatchCamera();

	// Set the camera type (default or a VR camera).
	void setType(short type);

	bool GetAlphaMask() const;

	virtual void RegisterCallbacks() override;

	bool isCameraTypeDefault() const;
	bool isDefaultPerspective() const;
	bool isDefaultOrtho() const;

private:

	// transform matrix
	MMatrix m_matrix;
	MObject m_imagePlane;

	// Camera type (default or a VR camera).
	short m_type;

	//Camera alpha mask
	bool m_alphaMask;

	// Camera
	frw::Camera m_camera;
};


// Fire render display layer
// Maya display layer wrapper
class FireRenderDisplayLayer : public FireRenderObject
{
public:

	// Constructor
	FireRenderDisplayLayer(FireRenderContext* context, const MObject& object);

	// Destructor
	virtual ~FireRenderDisplayLayer();

	// clear
    virtual void clear() override;

	// attribute changed callback
	virtual void attributeChanged(MNodeMessage::AttributeMessage msg, MPlug &plug, MPlug &otherPlug) override;

};

// Fire render sky
// Bridge class between a Maya sky node and a frw::Light
class FireRenderSky : public FireRenderNode
{
public:

	// Constructor
	FireRenderSky() = delete;
	FireRenderSky(FireRenderContext* context, const MDagPath& dagPath);

	// Destructor
	virtual ~FireRenderSky();

	// Refresh the sky.
	virtual void Freshen(bool shouldCalculateHash) override;

	// clear
	virtual void clear() override;

	// detach from the scene
	virtual void detachFromScene() override;

	// attach to the scene
	virtual void attachToScene() override;

	// The sky system is emissive.
	virtual bool IsEmissive() override { return true; }

	// Attach portals.
	void attachPortals();

	// Detach portals.
	void detachPortals();

public:
	// The environment light.
	frw::EnvironmentLight m_envLight;

protected:
	virtual void attachToSceneInternal();
	virtual void detachFromSceneInternal();

private:
	// transform matrix
	MMatrix m_matrix;

	// The sun light.
	frw::DirectionalLight m_sunLight;

	// The environment image.
	frw::Image m_image;

	// Sky builder.
	SkyBuilder* m_skyBuilder;

	// This list of active portals.
	std::vector<frw::Shape> m_portals;

	// True once the sky has been initialized.
	bool m_initialized = false;
};


class FireRenderSkyHybrid : public FireRenderSky
{
public:
	FireRenderSkyHybrid(FireRenderContext* context, const MDagPath& dagPath) :
		FireRenderSky(context, dagPath) {}

protected:
	void attachToSceneInternal() override;
	void detachFromSceneInternal() override;
};

// Fire render volume
class FireRenderCommonVolume : public FireRenderNode
{
public:
	// Constructor
	FireRenderCommonVolume(FireRenderContext* context, const MDagPath& dagPath);

	// Destructor
	virtual ~FireRenderCommonVolume();

	// Refresh the curves
	virtual void Freshen(bool shouldCalculateHash) override;

	// clear
	virtual void clear() override;

	// detach from the scene
	virtual void detachFromScene() override;

	// attach to the scene
	virtual void attachToScene() override;

	// Register the callback
	virtual void RegisterCallbacks(void) override;

	// node dirty
	virtual void OnShaderDirty(void);

protected:
	// create volume from maya fluid node
	virtual bool TranslateVolume(void) = 0;

	// applies transform to node
	virtual void ApplyTransform(void);

	// create shape for bounding box
	virtual bool SetBBox(double Xdim, double Ydim, double Zdim);

protected:
	// transform matrix
	MMatrix m_matrix;
	MMatrix m_bboxScale;

	// volume
	frw::VolumeGrid m_densityGrid;
	frw::VolumeGrid m_albedoGrid;
	frw::VolumeGrid m_emissionGrid;
	frw::Volume m_volume;

	// fake mesh
	frw::Shape m_boundingBoxMesh;
};

// Bridge class between a Maya Volume node and frw::Volume
struct VolumeData; // forward declaration of RPR data wrapper class
class FireRenderFluidVolume : public FireRenderCommonVolume
{
public:
	// Constructor
	FireRenderFluidVolume(FireRenderContext* context, const MDagPath& dagPath);

	// Destructor
	virtual ~FireRenderFluidVolume();

protected:
	// create volume from maya fluid node
	virtual bool TranslateVolume(void);

	// read data from fluid object and/or volume shader to rpr volume data container
	bool TranslateGeneralVolumeData(VolumeData* pVolumeData, MFnFluid& fnFluid);
	bool TranslateDensity(VolumeData* pVolumeData, MFnFluid& fnFluid, MFnDependencyNode& shaderNode);
	bool TranslateAlbedo(VolumeData* pVolumeData, MFnFluid& fnFluid, MFnDependencyNode& shaderNode);
	bool TranslateEmission(VolumeData* pVolumeData, MFnFluid& fnFluid, MFnDependencyNode& shaderNode);
	bool ReadDensityIntoArray(MFnFluid& fnFluid, std::vector<float>& outputValues);
	bool ReadTemperatureIntoArray(MFnFluid& fnFluid, std::vector<float>& outputValues);
	bool ReadFuelIntoArray(MFnFluid& fnFluid, std::vector<float>& outputValues);
	bool ReadPressureIntoArray(MFnFluid& fnFluid, std::vector<float>& outputValues);
	bool ReadSpeedIntoArray(MFnFluid& fnFluid, std::vector<float>& outputValues);
	bool ProcessInputField(int inputField, std::vector<float>& outData, unsigned int Xres, unsigned int Yres, unsigned int Zres, MFnFluid& fnFluid);

	// apply noise to channel
	bool ApplyNoise(std::vector<float>& channelValues);
};

// Bridge class between RPR Volume node and frw::Volume
class FireRenderRPRVolume : public FireRenderCommonVolume
{
public:
	// Constructor
	FireRenderRPRVolume(FireRenderContext* context, const MDagPath& dagPath);

	// Destructor
	virtual ~FireRenderRPRVolume();

protected:
	// create volume from maya fluid node
	virtual bool TranslateVolume(void) override;
};

// Implementation of RPR volumes in RPR 2
class NorthstarRPRVolume : public FireRenderRPRVolume
{
public:
	// Constructor
	NorthstarRPRVolume(FireRenderContext* context, const MDagPath& dagPath);

	// Destructor
	virtual ~NorthstarRPRVolume();

	// detach from the scene
	virtual void detachFromScene() override;

	// attach to the scene
	virtual void attachToScene() override;


protected:
	virtual bool TranslateVolume(void) override;
	virtual void ApplyTransform(void) override;
};

// Implementation of Fluid volumes in RPR 2
class NorthstarFluidVolume : public FireRenderFluidVolume
{
public:
	// Constructor
	NorthstarFluidVolume(FireRenderContext* context, const MDagPath& dagPath);

	// Destructor
	virtual ~NorthstarFluidVolume();

	// detach from the scene
	virtual void detachFromScene() override;

	// attach to the scene
	virtual void attachToScene() override;

protected:
	virtual bool TranslateVolume(void) override;
	virtual void ApplyTransform(void) override;
};

// Fire render hair
// Bridge class between a Maya hair physical shader node and a frw::Curve
class FireRenderHair : public FireRenderNode
{
public:	
	// Constructor
	FireRenderHair(FireRenderContext* context, const MDagPath& dagPath);

	// Destructor
	virtual ~FireRenderHair();

	// Refresh the curves
	virtual void Freshen(bool shouldCalculateHash) override;

	// clear
	virtual void clear(void) override;

	// detach from the scene
	virtual void detachFromScene(void) override;

	// attach to the scene
	virtual void attachToScene(void) override;

	// Register the callback
	virtual void RegisterCallbacks(void) override;
	
	// node dirty
	virtual void OnShaderDirty(void);

	// visibility flags
	virtual void setRenderStats(MDagPath dagPath);
	void setPrimaryVisibility(bool primaryVisibility);
	void setReflectionVisibility(bool reflectionVisibility);
	void setRefractionVisibility(bool refractionVisibility);
	void setCastShadows(bool castShadow);
	void setReceiveShadows(bool recieveShadow);

protected:
	// applies transform to node
	void ApplyTransform(void);

	// applies material connected to hair shader to Curves
	// returns false if no such material was found
	bool ApplyMaterial(void);

	virtual frw::Shader ParseNodeAttributes(MObject hairObject, const FireMaya::Scope& scope) { return frw::Shader(); }

	// set albedo
	// NIY

	// set emission
	// NIY

	// set filter
	// NIY
	// tries to load curves data and creates rpr curve (batch) if succesfull
	// returns false if failed to create curves
	virtual bool CreateCurves(void) = 0;

	// transform matrix
	MMatrix m_matrix;

	// curves
	std::vector<frw::Curve> m_Curves;
};

class FireRenderHairXGenGrooming : public FireRenderHair
{
public:
	// Constructor
	FireRenderHairXGenGrooming(FireRenderContext* context, const MDagPath& dagPath);

	// Destructor
	virtual ~FireRenderHairXGenGrooming();

	// visibility flags
	virtual void setRenderStats(MDagPath dagPath);

protected:
	virtual bool CreateCurves(void);
};

class FireRenderHairOrnatrix : public FireRenderHair
{
public:
	// Constructor
	FireRenderHairOrnatrix(FireRenderContext* context, const MDagPath& dagPath);

	// Destructor
	virtual ~FireRenderHairOrnatrix();

	// visibility flags
	virtual void setRenderStats(MDagPath dagPath);

protected:
	virtual bool CreateCurves(void);
};

class FireRenderHairNHair : public FireRenderHair
{
public:
	// Constructor
	FireRenderHairNHair(FireRenderContext* context, const MDagPath& dagPath);

	// Destructor
	virtual ~FireRenderHairNHair();

	// Register the callback
	virtual void RegisterCallbacks(void) override;

	// visibility flags
	virtual void setRenderStats(MDagPath dagPath);

protected:
	virtual bool CreateCurves(void);

	virtual frw::Shader ParseNodeAttributes(MObject hairObject, const FireMaya::Scope& scope);
};

class FireRenderCustomEmitter : public FireRenderLight
{
public:
	FireRenderCustomEmitter(FireRenderContext* context, const MDagPath& dagPath);

	void Freshen(bool shouldCalculateHash) override;
};

bool IsUberEmissive(frw::Shader shader);
