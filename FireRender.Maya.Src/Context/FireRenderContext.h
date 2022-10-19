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

#include <maya/M3dView.h>
#include <maya/MMessage.h>
#include <maya/MDGMessage.h>
#include <maya/MBoundingBox.h>
#include <maya/MFnTransform.h>
#include <maya/MCallbackIdArray.h>

#include "FireRenderObjects.h"
#include <string>
#include <map>
#include <time.h>

#include "frWrap.h"
#include "FireMaya.h"
#include "RenderRegion.h"
#include "FireRenderAOV.h"

#include <thread>
#include <functional>

#include <future>
#include <functional>

#include "FireRenderUtils.h"
#include "FireRenderContextIFace.h"

// Forward declarations.
class FireRenderViewport;
class ImageFilter;
struct RV_PIXEL;


// Turn on to track lock information
//#define DEBUG_LOCKS 1

// Image file description
// This class stores the information need by the image file writer
// during a non interactive render session, like file image extension
// or other flags
class ImageFileDescription
{
public:

	// Constructor
	ImageFileDescription():
		extension("exr"),
		flags(0)
	{}

	// Constructor
	ImageFileDescription(const std::string& ext):
		extension(ext),
		flags(0)
	{}

	// Copy constructor
	ImageFileDescription(const ImageFileDescription& other):
		extension(other.extension),
		flags(other.flags)
	{}

	// Assign operator
	ImageFileDescription& operator=(const ImageFileDescription& other)
	{
		extension = other.extension;
		flags = other.flags;
		return *this;
	}

public:

	// Image file extension
	std::string extension;

	// Flags
	int flags;
};


// used in sync progress callback
struct ContextWorkProgressData
{
	enum class ProgressType
	{
		Unknown = -1,
		SyncStarted = 1,
		ObjectPreSync,
		ObjectSyncComplete,
		SyncComplete,
		RenderStart,
		RenderPassStarted,
		RenderComplete
	};

	ProgressType progressType = ProgressType::Unknown;
	size_t currentIndex = 0;
	size_t totalCount = 0;
	std::string objectName;
	long long currentTimeInMiliseconds = 0;
	long long elapsed = 0;

	unsigned int GetPercentProgress() const { return (unsigned int)(100 * currentIndex / totalCount); }
};

using ProgressType = ContextWorkProgressData::ProgressType;

// FireRender Context
//
// This is the main class that wrap the main rpr_context
// Every renderer instance in Maya use this class,
// from the viewport renderer, to the swatch renderer etc...
// It also manage all the global callbacks connected to the current scene
// and the Maya session

class FireRenderContext : public IFireRenderContextInfo
{
public:
	typedef std::function<void(const ContextWorkProgressData&)> WorkProgressCallback;

	bool createContext(rpr_creation_flags createFlags, rpr_context& result, int* pOutRes = nullptr);

	enum RenderMode {
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

	enum StateEnum
	{
		StateExiting = 0,
		StatePaused = 1,
		StateRendering = 2,
		StateUpdating = 3,
	};


	// Constructor
	FireRenderContext();

	// Destructor
	virtual ~FireRenderContext();

	// Initialize the context
	// It create  the rpr_context, rpr_scene and rpr_material_system
	int initializeContext();

	// Build scene
	// Build the scene from the current Maya scene attaching all the callbacks needed
	void updateLimits(bool animation = false);
	void updateLimitsFromGlobalData(const FireRenderGlobalsData& globalData, bool animation = false, bool batch = false);

	bool buildScene(bool isViewport = false, bool glViewport = false, bool freshen = true);

	// Clean scene
	void cleanScene();

	// Run clean scene asynchronously
	void cleanSceneAsync(std::shared_ptr<FireRenderContext> refToKeepAlive);

	// Setup the motion blur
	void updateMotionBlurParameters(const FireRenderGlobalsData& globalData);
	void setMotionBlurParameters(const FireRenderGlobalsData& globalData);

	/** Return true if the render is interactive (Viewport or IPR). */
	bool isInteractive() const;

	// Build the scene for the swatch renderer
	// The scene is composed by a poly-sphere and a single light
	// \param shaderObj Shader used to render the sphere
	void initSwatchScene();

	void UpdateCompletionCriteriaForSwatch();

	// It resets frame buffer and reinitialize it if particular aov is enabled
	void resetAOV(int index, rpr_GLuint* glTexture);

	void enableAOVAndReset(int index, bool flag, rpr_GLuint* glTexture);

	// Sets the resolution and perform an initial render and frame buffer resolve.
	void resize(unsigned int w, unsigned int h, bool renderView, rpr_GLuint* glTexture = nullptr);

	// Setup denoiser if necessary
	bool TryCreateDenoiserImageFilters(bool useRAMBufer = false);

	bool TryCreateTonemapImageFilters(void);

	// Set the frame buffer resolution
	void setResolution(unsigned int w, unsigned int h, bool renderView, rpr_GLuint* glTexture = nullptr);

	void enableAOV(int aov, bool flag = true);
	bool isAOVEnabled(int aov) const;

	// Set camera
	void setCamera(MDagPath& cameraPath, bool useNonDefaultCameraType = false);

	//Get Camera
	FireRenderCamera& GetCamera() { return m_camera; }

	// Render function (optionally Locks)
	void render(bool lock = true);

	// Save the renderer image to a file
	void saveToFile(MString& filePath, const ImageFileDescription& imgDescription);

	// Get renderer image data
	std::vector<float> getRenderImageData();

	// Return the framebuffer width
	unsigned int width() const;

	// Return the framebuffer height
	unsigned int height() const;

	bool isRenderView() const;

	bool createContextEtc(rpr_creation_flags creation_flags, bool destroyMaterialSystemOnDelete = true, int* pOutRes = nullptr, bool createScene = true);

	// Return the context
	rpr_context context();

	// Return the material system
	rpr_material_system materialSystem();

	// Return the framebuffer
	rpr_framebuffer frameBufferAOV(int aov) const;
	rpr_framebuffer frameBufferAOV_Resolved(int aov);

	typedef void(*RenderUpdateCallback)(float, void*);

	virtual void SetRenderUpdateCallback(RenderUpdateCallback callback, void* data) {}
	virtual void SetSceneSyncFinCallback(RenderUpdateCallback callback, void* data) {}
	virtual void SetFirstIterationCallback(RenderUpdateCallback callback, void* data) {}
	virtual void SetRenderTimeCallback(RenderUpdateCallback callback, void* data) {}
	virtual void AbortRender() {}

	struct ReadFrameBufferRequestParams
	{
		RV_PIXEL* pixels;
		int aov;
		unsigned int width;
		unsigned int height;
		std::array<float, 3> shadowColor;
		float shadowTransp;
		float shadowWeight;
		const RenderRegion& region;
		bool mergeOpacity;
		bool mergeShadowCatcher;

		ReadFrameBufferRequestParams(const RenderRegion& _region)
			: pixels(nullptr)
			, aov(RPR_AOV_MAX)
			, width(0)
			, height(0)
			, shadowColor{ 0.0f, 0.0f, 0.0f }
			, shadowTransp(0.0f)
			, shadowWeight(1.0f)
			, region(_region)
			, mergeOpacity(false)
			, mergeShadowCatcher(false)
		{};

		unsigned int PixelCount(void) const { return (width*height); }
		bool UseTempData(void) const { return (region.getWidth() < width || region.getHeight() < height); }
	};

	// this is part of readFrameBuffer call and is only thing nneded for viewport
	RV_PIXEL* readFrameBufferSimple(ReadFrameBufferRequestParams& params);

	// Read frame buffer pixels and optionally normalize and flip the image.
	void readFrameBuffer(ReadFrameBufferRequestParams& params);

	// will process frame as shadow and/or reflection catcher
	bool ConsiderShadowReflectionCatcherOverride(const ReadFrameBufferRequestParams& params);

	// writes input aov frame bufer on disk (both resolved and not resolved)
	void DebugDumpAOV(int aov, char* pathToFile = nullptr) const;

	// runs denoiser, returns pixel array as float vector if denoiser runs successfully
	std::vector<float> GetDenoisedData(bool& result);

	// runs tonemappre, returns pixel arras as float vector if denoiser runs successfully
	std::vector<float> GetTonemappedData(bool& result);

	// reads aov directly into internal storage
	RV_PIXEL* GetAOVData(const ReadFrameBufferRequestParams& params);

	void ReadOpacityAOV(const ReadFrameBufferRequestParams& params);

	void CombineOpacity(int aov, RV_PIXEL* pixels, unsigned int area);

	// Composite image for Shadow Catcher, Reflection Catcher and Shadow+Reflection Catcher
	virtual void rifShadowCatcherOutput(const ReadFrameBufferRequestParams& params);
	virtual void rifReflectionCatcherOutput(const ReadFrameBufferRequestParams& params);
	virtual void rifReflectionShadowCatcherOutput(const ReadFrameBufferRequestParams& params);

	// Copy the frame buffer into temporary memory, if
	// required, or directly into the supplied pixel buffer.
	void doOutputFromComposites(const ReadFrameBufferRequestParams& params, size_t dataSize, const frw::FrameBuffer& frameBufferOut);

	// Copy pixels from the source buffer to the destination buffer.
	void copyPixels(RV_PIXEL* dest, RV_PIXEL* source,
		unsigned int sourceWidth, unsigned int sourceHeight,
		const RenderRegion& region) const;

	// Combine pixels (set alpha) with Opacity pixels
	void combineWithOpacity(RV_PIXEL* pixels, unsigned int size, RV_PIXEL *opacityPixels = NULL) const;

	// do action for each framebuffer matching filter
	void ForEachFramebuffer(std::function<void(int aovId)> actionFunc, std::function<bool(int aovId)> filter);

	std::vector<float> DenoiseAndUpscaleForViewport();

	// try running denoiser; result is saved into RAM buffer in context
	std::vector<float> DenoiseIntoRAM(void);

	// try running tonemapper; result is saved into RAM buffer in context
	bool TonemapIntoRAM(void);

	// runs denoiser, puts result in aov and applies render stamp
	void ProcessDenoise(FireRenderAOV& renderViewAOV, FireRenderAOV& colorAOV, unsigned int width, unsigned int height, const RenderRegion& region, std::function<void(RV_PIXEL* pData)> callbackFunc);
	
	// runs tonemapper, puts result in aov
	void ProcessTonemap(FireRenderAOV& renderViewAOV, FireRenderAOV& colorAOV, unsigned int width, unsigned int height, const RenderRegion& region, std::function<void(RV_PIXEL* pData)> callbackFunc);

	// try merge opacity from context to supplied buffer
	void ProcessMergeOpactityFromRAM(RV_PIXEL* data, int bufferWidth, int bufferHeight);

	// Resolve the framebuffer using the current tone mapping

	// Return the rpr_scene
	rpr_scene scene();

	// Return the render camera
	FireRenderCamera& camera();

	// Get a render object
	// \param name Uuid string of the object
	// With Maya 2015 there is no uuid attribute. In this case
	// the buildScene function when registers an object to the renderer
	// create a custom attribute called fruuid with a unique value for each node
	FireRenderObject* getRenderObject(const std::string& name);
	FireRenderObject* getRenderObject(const MDagPath& path);
	FireRenderObject* getRenderObject(const MObject& ob);

	void RemoveRenderObject(const MObject& ob);
	void RefreshInstances();

	template <class T>
	T* getRenderObject(const std::string& name) { return dynamic_cast<T*>(getRenderObject(name)); }

	template <class T>
	T* getRenderObject(const MDagPath& ob) { return dynamic_cast<T*>(getRenderObject(ob)); }

	template <class T>
	T* getRenderObject(const MObject& ob) { return dynamic_cast<T*>(getRenderObject(ob)); }

	bool AddSceneObject(FireRenderObject* ob);
	bool AddSceneObject(const MDagPath& ob);

	virtual FireRenderEnvLight* CreateEnvLight(const MDagPath& dagPath);
	virtual FireRenderSky* CreateSky(const MDagPath& dagPath);

	enum class NodeCachingOptions
	{
		AddPath,
		DontAddPath
	};

	template <typename T, NodeCachingOptions>
	struct CreateSceneObject_Impl
	{
		static T* call(const MDagPath& ob, FireRenderContext* context);
	};

	template <typename T, const NodeCachingOptions opt>
	T* CreateSceneObject(const MDagPath& ob)
	{
		return CreateSceneObject_Impl<T, opt>::call(ob, this);
	}

	template <typename T> struct CreateSceneObject_Impl<T, NodeCachingOptions::AddPath>
	{
		static T* call(const MDagPath& ob, FireRenderContext* context)
		{
			T* fr = new T(context, ob);

			if (context->AddSceneObject(fr))
			{
				// save node id and dagPath for future use
				context->AddNodePath(ob, fr->uuid());
			}
			else
			{
				DebugPrint("ERROR: Couldn't add object to scene.");
			}

			return fr;
		}
	};

	template <typename T> struct CreateSceneObject_Impl<T, NodeCachingOptions::DontAddPath>
	{
		static T* call(const MDagPath& ob, FireRenderContext* context)
		{
			T* fr = new T(context, ob);

			if (!context->AddSceneObject(fr))
			{
				DebugPrint("ERROR: Couldn't add object to scene.");
			}

			return fr;
		}
	};

	// Attach all the global callbacks
	// This function will remove current callbacks installed from current context and then attach new callbacks.
	void attachCallbacks();

	// Remove callbacks
	void removeCallbacks();

	// Removed node callback
	// Called when Maya delete a node
	static void removedNodeCallback(MObject &node, void *clientData);

	// Add node callback
	// Called when Maya add a node
	static void addedNodeCallback(MObject &node, void *clientData);

	// Called when an attribute on the FireRenderGlobals node change
	static void globalsChangedCallback(MNodeMessage::AttributeMessage msg, MPlug &plug, MPlug &otherPlug, void *clientData);

	// Called when a render layer change
	static void renderLayerManagerCallback(MNodeMessage::AttributeMessage msg, MPlug &plug, MPlug &otherPlug, void *clientData);

	// Set the context dirty
	// It forces to clear the framebuffer and the iterations ant the next render call
	void setDirty();

	void disableSetDirtyObjects(bool disable);
	void setDirtyObject(FireRenderObject* obj);

	// Check if the context is dirty
	bool isDirty();

	// refresh/rebuild anything we require
	bool Freshen(bool lock = true,
		std::function<bool()> cancelled = [] { return false; });

	HashValue GetStateHash();

	// Add a node to the scene.
	void addNode(const MObject& node);

	static bool DoesNodeAffectContextRefresh(const MObject &node);

	// Remove a node from the scene.
	void removeNode(MObject& node);

	// Update from globals.
	void updateFromGlobals(bool applyLock);

	// Update active render layers.
	void updateRenderLayers();

	// Set "Use render region" flag and returns if this flag was really set or not
	bool setUseRegion(bool value);

	// Check if render region is on
	bool useRegion();

	// Set the render region
	void setRenderRegion(const RenderRegion& region);

	// Return the render region
	RenderRegion renderRegion();

	// Disable the *creation* of callbacks (recommend refactoring this when looking at callbacks)
	void setCallbackCreationDisabled(bool value);

	// Check if the callbacks are disabled
	bool getCallbackCreationDisabled();

	// Check if render selected objects only
	bool renderSelectedObjectsOnly() const;

	// Check if motion blur is active
	bool motionBlur() const;
	bool cameraMotionBlur() const;

	// Getting camera exposure for motion blur
	float motionBlurCameraExposure() const;

	unsigned int motionSamples() const;

	// State flag of the renderer
	StateEnum GetState() const { return m_state; }
	void SetState(StateEnum newState);

	const char* lockedBy;

	// Camera attribute changed
	void setCameraAttributeChanged(bool value);

	// Check if render region is on
	bool cameraAttributeChanged() const { return m_cameraAttributeChanged; }

	void UpdateDefaultLights();

	void setRenderMode(RenderMode renderMode);

	void SetPreviewMode(int preview);

	bool hasTonemappingChanged() const { return m_tonemappingChanged; }

	// Returns true if context was recently Freshen and needs redraw
	bool needsRedraw(bool setNotUpdatedOnExit = true);

	bool IsDenoiserCreated(void) const { return m_denoiserFilter != nullptr; }

	bool IsDenoiserEnabled(void) const;

	virtual bool IsTonemappingEnabled(void) const;

	bool IsTileRender(void) const { return (m_globals.tileRenderingEnabled && !isInteractive()); }

	std::shared_ptr<ImageFilter> m_tonemap;

	frw::PostEffect m_normalization;
	frw::PostEffect white_balance;
	frw::PostEffect simple_tonemap;
	frw::PostEffect gamma_correction;

	FireRenderMeshCommon* GetMainMesh(const std::string& uuid) const
	{
		assert(!uuid.empty());

		auto it = m_mainMeshesDictionary.find(FireRenderObject::uuidWithoutInstanceNumberForString(uuid));

		if (it != m_mainMeshesDictionary.end())
		{
			return it->second;
		}

		return nullptr;
	}

	void AddMainMesh(FireRenderMeshCommon* mainMesh)
	{
		std::string uuid = mainMesh->uuidWithoutInstanceNumber();

		const FireRenderMeshCommon* alreadyHas = GetMainMesh(uuid);
		assert(!alreadyHas);

		m_mainMeshesDictionary[uuid] = mainMesh;
	}

	void AddMainMesh(FireRenderMeshCommon* mainMesh, std::string& suffix)
	{
		std::string uuid = mainMesh->uuidWithoutInstanceNumber();

		const FireRenderMeshCommon* alreadyHas = GetMainMesh(uuid + suffix);
		assert(!alreadyHas);

		m_mainMeshesDictionary[uuid + suffix] = mainMesh;
	}

	void RemoveMainMesh(const FireRenderMesh* mainMesh)
	{
		std::string uuid = mainMesh->uuidWithoutInstanceNumber();
		auto found = m_mainMeshesDictionary.find(uuid);

		if (found != m_mainMeshesDictionary.end())
		{
			m_mainMeshesDictionary.erase(found);
		}
	}

	bool GetNodePath(MDagPath& outPath, const std::string& uuid) const
	{
		auto it = m_nodePathCache.find(uuid);

		if (it != m_nodePathCache.end())
		{
			outPath = it->second;
			return true;
		}

		return false;
	}

	void AddNodePath(const MDagPath& path, const std::string& uuid)
	{
		MString tmpStr;
		tmpStr = path.fullPathName();

		MDagPath tmpPath;
		bool alreadyHas = GetNodePath(tmpPath, uuid);
		assert(!alreadyHas);

		m_nodePathCache[uuid] = path;
	}

	typedef std::map<std::string, std::shared_ptr<FireRenderObject> > FireRenderObjectMap;
	FireRenderObjectMap& GetSceneObjects() { return m_sceneObjects; }

	RenderType GetRenderType(void) const;
	void SetRenderType(RenderType renderType);

	// In case if we set render quality which is not supported by the current Renderer. i.e you set "Full" for Hybrid or "Low" for Tahoe
	bool DoesContextSupportCurrentSettings() const { return m_DoesContextSupportCurrentSettings; }
	void ResetContextSupportCurrentSettings() { m_DoesContextSupportCurrentSettings = true; }

	virtual bool ShouldResizeTexture(unsigned int& max_width, unsigned int& max_height) const;

	virtual rpr_int SetRenderQuality(RenderQuality quality) { return RPR_SUCCESS; }

	virtual void setupContextContourMode(const FireRenderGlobalsData& fireRenderGlobalsData, int createFlags, bool disableWhiteBalance = false) {}
	virtual void setupContextPostSceneCreation(const FireRenderGlobalsData& fireRenderGlobalsData, bool disableWhiteBalance = false) {}
	virtual void setupContextAirVolume(const FireRenderGlobalsData& fireRenderGlobalsData) {}
	virtual void setupContextCryptomatteSettings(const FireRenderGlobalsData& fireRenderGlobalsData) {}
	virtual bool IsAOVSupported(int aov) const { return true; }

	virtual bool IsRenderQualitySupported(RenderQuality quality) const override = 0;
	virtual bool IsRenderRegionSupported() const override { return true; }
	virtual bool IsDenoiserSupported() const override { return true; }
	virtual bool IsDisplacementSupported() const override { return true; }
	virtual bool IsHairSupported() const override { return true; }
	virtual bool IsVolumeSupported() const override { return true; }
	virtual bool IsNorthstarVolumeSupported() const { return false; }
	virtual bool ShouldForceRAMDenoiser() const override { return false; }

	virtual bool IsPhysicalLightTypeSupported(PLType lightType) const { return true; }

	virtual bool IsShaderSupported(frw::ShaderType type) const override { return true; }

	virtual bool IsShaderNodeSupported(FireMaya::ShaderNode* shaderNode) const override { return true; }
	virtual frw::Shader GetDefaultColorShader(frw::Value color) override;

	virtual bool MetalContextAvailable() const { return false; }

	virtual bool IsDeformationMotionBlurEnabled() const { return false; }

	virtual bool IsMaterialNodeIDSupported() const { return true; }
	virtual bool IsMeshObjectIDSupported() const { return true; }
	virtual bool IsContourModeSupported() const { return true; }
	virtual bool IsCameraSetExposureSupported() const { return true; }
	virtual bool IsShadowColorSupported() const { return true; }
	virtual bool IsUberReflectionDielectricSupported() const { return true; }
	virtual bool IsUberRefractionAbsorbtionColorSupported() const { return true; }
	virtual bool IsUberRefractionAbsorbtionDistanceSupported() const { return true; }
	virtual bool IsUberRefractionCausticsSupported() const { return true; }
	virtual bool IsUberSSSWeightSupported() const { return true; }
	virtual bool IsUberSheenWeightSupported() const { return true; }
	virtual bool IsUberBackscatterWeightSupported() const { return true; }
	virtual bool IsUberShlickApproximationSupported() const { return true; }
	virtual bool IsUberCoatingThicknessSupported() const { return true; }
	virtual bool IsUberCoatingTransmissionColorSupported() const { return true; }
	virtual bool IsUberReflectionNormalSupported() const { return true; }
	virtual bool IsUberScaleSupported() const { return true; }

	bool IsGLTFExport() const override { return m_bIsGLTFExport; }
	void SetGLTFExport(bool isGLTFExport) { m_bIsGLTFExport = isGLTFExport; }

	void SetWorkProgressCallback(WorkProgressCallback callback) { m_WorkProgressCallback = callback; }
	void SetIterationsPowerOf2Mode(bool flag) { m_IterationsPowerOf2Mode = flag; }

	int GetSamplesPerUpdate() const { return m_samplesPerUpdate; }

	void ResetRAMBuffers(void);
  
	const FireRenderGlobalsData& Globals(void) const { return m_globals; }

	bool setupUpscalerForViewport(RV_PIXEL* data);
	bool setupDenoiserForViewport();

	// used for toon shader light linking
	frw::Light LinkLightSceneObjectWithCurrentlyParsedMesh(const MObject& node);

protected:
	static int INCORRECT_PLUGIN_ID;

protected:
	virtual rpr_int CreateContextInternal(rpr_creation_flags createFlags, rpr_context* pContext) = 0;
	virtual void updateTonemapping(const FireRenderGlobalsData&, bool disableWhiteBalance = false) {}

	int getThreadCountToOverride() const;

	// Helper function showing if we need to use buffer or resolved buffer
	virtual bool needResolve() const { return false; }

	virtual bool IsGLInteropEnabled() const { return true; }

	void UpdateTimeAndTriggerProgressCallback(ContextWorkProgressData& syncProgressData, ProgressType progressType = ProgressType::Unknown);

	void TriggerProgressCallback(const ContextWorkProgressData& syncProgressData);

	virtual void OnPreRender() {}

	virtual int GetAOVMaxValue();

	void ReadDenoiserFrameBuffersIntoRAM(ReadFrameBufferRequestParams& params);

private:
	struct CallbacksAttachmentHelper
	{
	public:
		CallbacksAttachmentHelper(FireRenderContext *context)
		{
			mContext = context;
			mContext->attachCallbacks();
		}
		~CallbacksAttachmentHelper()
		{
			if (mContext)
				mContext->removeCallbacks();
		}
	private:
		FireRenderContext *mContext;
	};

	void initBuffersForAOV(frw::Context& context, int index, rpr_GLuint* glTexture = nullptr);

	void forceTurnOnAOVs(const std::vector<int>& aovsToAdd, bool allocBuffer = false);
	void turnOnAOVsForDenoiser(bool allocBuffer = false);
	void turnOnAOVsForContour(bool allocBuffer = false);
	bool CanCreateAiDenoiser() const;

	void setupDenoiserFB(void);
	void setupDenoiserRAM(void);
	void BuildLateinitObjects();

private:
	std::mutex m_rifLock;
	std::shared_ptr<ImageFilter> m_denoiserFilter;
	std::shared_ptr<ImageFilter> m_upscalerFilter;

	frw::DirectionalLight m_defaultLight;

	std::atomic<StateEnum> m_state;

	// Render camera
	FireRenderCamera m_camera;

	// map containing all the objects converted
	FireRenderObjectMap m_sceneObjects;

	// Main mutex
	std::mutex m_mutex;

	// these are all automatically destructing handles
	struct Handles
	{
		frw::FrameBuffer framebufferAOV[RPR_AOV_MAX];
		frw::FrameBuffer framebufferAOV_resolved[RPR_AOV_MAX];

		void Reset()
		{
			*this = Handles();
		}
	};
	Handles m;

	bool aovEnabled[RPR_AOV_MAX] = {0};

	// These two are to prevent constant allocs/dealloc during rendering and are used in readFrameBuffer:
	PixelBuffer m_tempData;
	PixelBuffer m_opacityData;
	PixelBuffer m_opacityTempData;

	FireMaya::Scope scope;

	// Render selected objects flag
	bool m_renderSelectedObjectsOnly;

	// Motion blur flag
	bool m_motionBlur;

	// Camera motion blur flag
	bool m_cameraMotionBlur;

	bool m_viewportMotionBlur;

	// motion blur only in velocity aov
	bool m_velocityAOVMotionBlur;

	// Motion blur camera exposure
	float m_motionBlurCameraExposure;

	// used for Deformation motion blur only for now
	unsigned int m_motionSamples;

	/** True if the render should be interactive. */
	bool m_interactive;

	/** A list of nodes that have been added since the last refresh. */
	std::vector<MObject> m_addedNodes;

	/** A list of nodes that have been removed since the last refresh. */
	std::vector<MObject> m_removedNodes;

	/** A list of objects which requires updating. Using weak_ptr to asynchronous allow removal of objects while they are waiting for update. */
	std::map<FireRenderObject*, std::weak_ptr<FireRenderObject> > m_dirtyObjects;

	/** Mutex used for disabling simultaneous access to dirty objects list. */
	std::mutex m_dirtyMutex;

	/** Holds current globals state obtained in previous refresh call. */
	FireRenderGlobalsData m_globals;

	/** True if globals have changed since the last refresh. */
	bool m_globalsChanged;

	/** Signals if tone-mapping options have changed */
	bool m_tonemappingChanged;

	/** Signals if denoiser options have changed */
	bool m_denoiserChanged;

	/** True if render layers have changed since the last refresh. */
	bool m_renderLayersChanged;

	bool m_inRefresh = false;

	/** Future object for async cleaning*/
	std::future<void> m_cleanSceneFuture;

	/** Returns valid context pointer only if we are in the mood to process callbacks */
	static FireRenderContext* GetCallbackContext(void *clientData)
	{
		if (auto p = reinterpret_cast<FireRenderContext*>(clientData))
		{
			if (!p->m_inRefresh)
				return p;
		}

		return nullptr;
	}

	/** map corresponds shape in Maya with main FireRenderMesh (used for instancing) **/
	std::map<std::string, FireRenderMeshCommon*> m_mainMeshesDictionary;

	/** map corresponding dag path of the node with the mode **/
	std::map<std::string, MDagPath> m_nodePathCache;

	std::atomic<int> m_samplesPerUpdate;

	// render type information
	RenderType m_RenderType;

	std::vector<MDagPath> m_LateinitMASHInstancers;
	bool m_DoesContextSupportCurrentSettings = true;

	// buffer to dump data from frame buffers, if needed
	AOVPixelBuffers m_pixelBuffers;

	bool m_bIsGLTFExport;

	WorkProgressCallback m_WorkProgressCallback;

	// Increasing iterations - 1, 2, 4, 8, etc up to 32 for now
	bool m_IterationsPowerOf2Mode;

	// Used for deformation motion blur feature. We need to disable dirtying object when perform deformation motion blur operations (switcihng current time which leads to dirty all objects)
	bool m_DisableSetDirtyObjects;

public:
	FireRenderEnvLight *iblLight = nullptr;
	MObject iblTransformObject = MObject();

	FireRenderSky *skyLight = nullptr;
	MObject skyTransformObject = MObject();

	FireMaya::Scope& GetScope() { return scope; }
	frw::Scene GetScene() { return scope.Scene(); }
	frw::Context GetContext() { return scope.Context(); }
	const frw::Context GetContext() const { return scope.Context(); }
	frw::MaterialSystem GetMaterialSystem() { return scope.MaterialSystem(); }
	frw::Shader GetShader(MObject ob, MObject shadingEngine = MObject(), const FireRenderMeshCommon* pMesh = nullptr, bool forceUpdate = false); // { return scope.GetShader(ob, forceUpdate); }
	frw::Shader GetVolumeShader(MObject ob, bool forceUpdate = false) { return scope.GetVolumeShader(ob, forceUpdate); }

	// Width of the framebuffer
	unsigned int m_width;

	// Height of the framebuffer
	unsigned int m_height;

	bool m_isRenderView = false;

	// remove node callback
	MCallbackId m_removedNodeCallback = 0;

	// Add node callback
	MCallbackId m_addedNodeCallback = 0;

	// render globals callback
	MCallbackId m_renderGlobalsCallback = 0;

	// render layer callback
	MCallbackId m_renderLayerCallback = 0;

	// Render region
	RenderRegion m_region;

	// Dirty flag
	std::atomic_bool m_dirty;
	std::atomic_bool m_cameraDirty;
	std::atomic_bool m_needRedraw;

	// Use region flag
	bool m_useRegion;

	// Callbacks disabled flag
	bool m_callbackCreationDisabled = false;

	// Camera changed flag
	bool m_cameraAttributeChanged;

	bool m_restartRender;

	// completion criteria sections:
	TimePoint m_renderStartTime;

	TimePoint m_workStartTime;

	CompletionCriteriaParams m_completionCriteriaParams;

	int	m_currentIteration;
	rpr_uint m_currentFrame;
	int	m_progress;
	std::chrono::time_point<std::chrono::system_clock> m_lastRenderStartTime;

	// shadow color and transparency (for shadow/reflection catcher)
	std::array<float, 3> m_shadowColor;
	float m_shadowTransparency;
	float m_shadowWeight;

	// data for render time reporting
	float m_syncTime;
	float m_firstFrameRenderTime;
	float m_lastRenderedFrameRenderTime;

	/* data for athena dumping */
	double m_secondsSpentOnLastRender;
	unsigned int m_polycountLastRender;
	enum RenderResultState
	{
		COMPLETED = 0,
		CANCELED,
		CRASHED,
		NOT_SET
	} m_lastRenderResultState;

	void setCompletionCriteria(const CompletionCriteriaParams& completionCriteriaParams);
	const CompletionCriteriaParams& getCompletionCriteria(void) const;

	bool isUnlimited();
	void setStartedRendering();
	bool keepRenderRunning();
	bool isFirstIterationAndShadersNOTCached();
	void updateProgress();
	int	getProgress();
	void setProgress(int percents);

	void setSamplesPerUpdate(int samplesPerUpdate);

	AOVPixelBuffers& PixelBuffers(void) { return m_pixelBuffers; }
    
	class Lock
	{
		FireRenderContext* context;
		int oldState;

		Lock(const Lock&) = delete;
	public:

        Lock(FireRenderContext* _context, StateEnum newState, const char* lockedBy)
			: context(_context)
		{
			if (context)
			{
#ifdef DEBUG_LOCKS
                addMapLock(context,lockedBy);
#endif
				context->m_mutex.lock();
				oldState = context->GetState();
				context->SetState(newState);
				context->lockedBy = lockedBy;
			}
		}

		Lock(FireRenderContext* _context, const char* lockedBy)
			: context(_context)
		{
			if (context)
			{
#ifdef DEBUG_LOCKS
                addMapLock(context,lockedBy);
#endif
				context->m_mutex.lock();
				oldState = -1;
				context->lockedBy = lockedBy;
			}
		}

		~Lock()
		{
			if (context)
			{
				if (oldState >= 0)
					context->SetState(StateEnum(oldState));
#ifdef DEBUG_LOCKS
                removeMapLock(context);
#endif
				context->m_mutex.unlock();
			}
		}
		operator bool() const { return true; }
	};

	friend class Lock;
};

class ContextSetDirtyObjectAutoLocker
{
public:
	ContextSetDirtyObjectAutoLocker(FireRenderContext& context) :
		m_context(context)
	{
		m_context.disableSetDirtyObjects(true);
	}
	~ContextSetDirtyObjectAutoLocker()
	{
		m_context.disableSetDirtyObjects(false);
	}
private:
	FireRenderContext& m_context;
};


typedef std::shared_ptr<FireRenderContext> FireRenderContextPtr;

#define STRINGIZE(x) STRINGIZE2(x)
#define STRINGIZE2(x) #x

// using these macros to make it easier to debug deadlocks
#define LOCKFORUPDATE(a) FireRenderContext::Lock lockForUpdate(a, FireRenderContext::StateUpdating, __FILE__ ":" STRINGIZE(__LINE__))
#define LOCKMUTEX(a) FireRenderContext::Lock lockMutex(a, __FILE__ ":" STRINGIZE(__LINE__))


