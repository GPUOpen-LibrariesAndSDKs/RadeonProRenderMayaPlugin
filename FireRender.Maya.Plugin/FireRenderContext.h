#pragma once
//
// Copyright (C) AMD
//
// File: FireRenderContext.h
//
// FireRender context wrapper class
//
// Created by Alan Stanzione.
//

#include <maya/M3dView.h>
#include <maya/MMessage.h>
#include <maya/MDGMessage.h>
#include <maya/MMutexLock.h>
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

// Forward declarations.
class FireRenderViewport;
class FireRenderGlobalsData;
struct RV_PIXEL;

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


// FireRender Context
//
// This is the main class that wrap the main rpr_context
// Every renderer instance in Maya use this class,
// from the viewport renderer, to the swatch renderer etc...
// It also manage all the global callbacks connected to the current scene
// and the Maya session
class FireRenderContext
{
public:
	static bool createContext(rpr_creation_flags createFlags, rpr_context& result);

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
	~FireRenderContext();

	// Initialize the context
	// It create  the rpr_context, rpr_scene and rpr_material_system
	void initializeContext();

	// Build scene
	// Build the scene from the current Maya scene attaching all the callbacks needed
	void updateLimits(bool animation = false);
	void updateLimitsFromGlobalData(const FireRenderGlobalsData& globalData, bool animation = false, bool batch = false);
	bool buildScene(bool animation = false, bool isViewport = false, bool glViewport = false, bool freshen = true);

	// Clean scene
	void cleanScene();

	// Setup the motion blur
	void setMotionBlur(bool doBlur);

	/** Set to true for interactive renders (Viewport or IPR). */
	void setInteractive(bool interactive);

	/** Return true if the render is interactive (Viewport or IPR). */
	bool isInteractive() const;

	/** Return true if OpenGL interop is active. */
	bool isGLInteropActive() const;

	// Build the scene for the swatch renderer
	// The scene is composed by a poly-sphere and a single light
	// \param shaderObj Shader used to render the sphere
	void initSwatchScene();
	void updateSwatchSceneRenderLimits(const MObject& shaderObj);

	// Sets the resolution and perform an initial render and frame buffer resolve.
	void resize(unsigned int w, unsigned int h, bool renderView, rpr_GLuint* glTexture = nullptr);

	// Set the frame buffer resolution
	void setResolution(unsigned int w, unsigned int h, bool renderView, rpr_GLuint* glTexture = nullptr);

	void enableAOV(int aov){
		aovEnabled[aov] = true;
	}
	bool isAOVEnabled(int aov){
		return aovEnabled[aov];
	}

	/**
	 * Enable FireRender gamma correction
	 * by applying the current global value.
	 */
	void enableDisplayGammaCorrection(const FireRenderGlobalsData& globals);

	// Set camera
	void setCamera(MDagPath& cameraPath, bool useVRCamera = false);

	// Render function (optionally Locks)
	void render(bool lock = true);

	// Save the renderer image to a file
	void saveToFile(MString& filePath, const ImageFileDescription& imgDescription);

	// Get renderer image data
	std::vector<float> getRenderImageData();

	// Return the framebuffer width
	unsigned int width();

	// Return the framebuffer height
	unsigned int height();

	bool isRenderView() const;

	bool createContextEtc(rpr_context_type context_type, rpr_creation_flags creation_flags, bool destroyMaterialSystemOnDelete = true, bool glViewport = false);

	// Return the context
	rpr_context context();

	// Return the material system
	rpr_material_system materialSystem();

	// Return the framebuffer
	rpr_framebuffer framebufferAOV(int aov) const;
	rpr_framebuffer frameBufferAOV_Resolved(int aov);

	// Read frame buffer pixels and optionally normalize and flip the image.
	void readFrameBuffer(RV_PIXEL* pixels, rpr_framebuffer frameBuffer,
		unsigned int width, unsigned int height, const RenderRegion& region,
		bool flip, bool isColor, rpr_framebuffer opacityFrameBuffer = NULL);

	// Copy pixels from the source buffer to the destination buffer.
	void copyPixels(RV_PIXEL* dest, RV_PIXEL* source,
		unsigned int sourceWidth, unsigned int sourceHeight,
		const RenderRegion& region, bool flip, bool isColor) const;

	// Normalize the specified pixel buffer.
	void normalizePixels(RV_PIXEL* pixels, unsigned int size, RV_PIXEL *opacityPixels = NULL) const;

	// Combine pixels (set alpha) with Opacity pixels
	void combineWithOpacity(RV_PIXEL* pixels, unsigned int size, RV_PIXEL *opacityPixels = NULL) const;


	// Return a transparent shader
	// Used by the portals
	frw::Shader transparentShader();

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

	template <class T>
	T* CreateSceneObject(const MDagPath& ob)
	{
		T* fr = new T(this, ob);
		if (!AddSceneObject(fr))
			DebugPrint("ERROR: Couldn't add object to scene.");
		return fr;
	}


	// Attach all the global callbacks
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

	void setDirtyObject(FireRenderObject* obj);

	// Check if the context is dirty
	bool isDirty();

	// refresh/rebuild anything we require
	bool Freshen(bool lock = true, std::function<bool()> cancelled = [] { return false; });
	HashValue GetStateHash();

	// Add a node to the scene.
	void addNode(MObject& node);

	// Remove a node from the scene.
	void removeNode(MObject& node);

	// Update from globals.
	void updateFromGlobals();

	// Update active render layers.
	void updateRenderLayers();

	// Use render region
	void setUseRegion(bool value);

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

	// Check if motion blur is active
	bool motionBlur();

	// State flag of the renderer
	tbb::atomic<StateEnum> state;
	const char* lockedBy;

	// Camera attribute changed
	void setCameraAttributeChanged(bool value);

	// Check if render region is on
	bool cameraAttributeChanged() const { return m_cameraAttributeChanged; }

	void UpdateDefaultLights();

	void setRenderMode(RenderMode renderMode);

	void UpdateGround(const FireRenderGlobalsData& data);

	bool hasTonemappingChanged() const { return m_tonemappingChanged; }

	// Returns true if context was recently Freshen and needs redraw
	bool needsRedraw(bool setNotUpdatedOnExit = true);

	frw::PostEffect white_balance;
	frw::PostEffect simple_tonemap;			// TODO: not used
	frw::PostEffect tonemap;
	frw::PostEffect normalization;
	frw::PostEffect gamma_correction;

private:
	// Helper function showing if we need to use buffer or resolved buffer
	bool needResolve() const
	{
		return bool(white_balance) || bool(simple_tonemap) || bool(tonemap) || bool(normalization) || bool(gamma_correction);
	}

	// Tweaks precision of the ray-cast system, based on scene bounding box size:
	void CheckSetRayCastEpsilon();

	float m_lastRayCastEpsilon;

	frw::DirectionalLight m_defaultLight;

	// Render camera
	FireRenderCamera m_camera;

	// map containing all the objects converted
	std::map<std::string, std::shared_ptr<FireRenderObject> > m_sceneObjects;

	// Main mutex
	MMutexLock m_mutex;

	// these are all automatically destructing handles
	struct Handles
	{
		frw::FrameBuffer framebufferAOV[RPR_AOV_MAX];
		frw::FrameBuffer framebufferAOV_resolved[RPR_AOV_MAX];

		struct
		{
			frw::EnvironmentLight	light;
			frw::Shape				shadows;
			frw::Shape				reflections;
		} ground;

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

	// Motion blur flag
	bool m_motionBlur;

	/** True if the render should be interactive. */
	bool m_interactive;

	/** True if OpenGL interop is enabled. */
	bool m_glInteropActive;

	/** A list of nodes that have been added since the last refresh. */
	std::vector<MObject> m_addedNodes;

	/** A list of nodes that have been removed since the last refresh. */
	std::vector<MObject> m_removedNodes;

	/** A list of objects which requires updating. Using weak_ptr to asynchronous allow removal of objects while they are waiting for update. */
	std::vector<std::weak_ptr<FireRenderObject> > m_dirtyObjects;

	/** Mutex used for disabling simultaneous access to dirty objects list. */
	MMutexLock m_dirtyMutex;

	/** True if globals have changed since the last refresh. */
	bool m_globalsChanged;

	/** Signals if tone-mapping options have changed */
	bool m_tonemappingChanged;

	/** True if render layers have changed since the last refresh. */
	bool m_renderLayersChanged;

	bool m_inRefresh = false;

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


public:
	FireRenderEnvLight *iblLight = nullptr;
	MObject iblTransformObject = MObject();

	FireRenderSky *skyLight = nullptr;
	MObject skyTransformObject = MObject();

	FireMaya::Scope& GetScope() { return scope; }
	frw::Scene GetScene() { return scope.Scene(); }
	frw::Context GetContext() { return scope.Context(); }
	frw::MaterialSystem GetMaterialSystem() { return scope.MaterialSystem(); }
	frw::Shader GetShader(MObject ob, bool forceUpdate = false) { return scope.GetShader(ob, forceUpdate); }
	frw::Shader GetVolumeShader(MObject ob, bool forceUpdate = false) { return scope.GetVolumeShader(ob, forceUpdate); }


	// Transparent material
	frw::Shader m_transparent;

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


	//completion criteria sections:
	clock_t		m_startTime;
	short		m_completionType;
	long		m_completionIterations;
	int			m_completionTime;
	int			m_currentIteration;
	int			m_progress;

	double		m_timeIntervalForOutputUpdate;//in sec, TODO: check for Linux/Mac
	clock_t		m_lastIterationTime;

	void setCompletionCriteria(short type, long seconds, int iterations);
	bool isUnlimited();
	void setStartedRendering();
	bool keepRenderRunning();
	bool isFirstIterationAndShadersNOTCached();
	void updateProgress();
	int	getProgress();
	bool updateOutput();

	class Lock
	{
		FireRenderContext* context;
		int oldState;

		Lock(const Lock&) = delete;
	public:
		Lock(FireRenderContext* _context, StateEnum newState = StateUpdating, const char* lockedBy = "?")
			: context(_context)
		{
			if (context)
			{
				context->m_mutex.lock();
				oldState = context->state;
				context->state = newState;
				context->lockedBy = lockedBy;
			}
		}

		Lock(FireRenderContext* _context, const char* lockedBy)
			: context(_context)
		{
			if (context)
			{
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
					context->state = StateEnum(oldState);
				context->m_mutex.unlock();
			}
		}
		operator bool() const { return true; }
	};

	friend class Lock;
};

extern rpr_int g_tahoePluginID;

#define STRINGIZE(x) STRINGIZE2(x)
#define STRINGIZE2(x) #x

// using these macros to make it easier to debug deadlocks
#define LOCKFORUPDATE(a) FireRenderContext::Lock lockForUpdate(a, FireRenderContext::StateUpdating, __FILE__ ":" STRINGIZE(__LINE__))
#define LOCKMUTEX(a) FireRenderContext::Lock lockMutex(a, __FILE__ ":" STRINGIZE(__LINE__))


