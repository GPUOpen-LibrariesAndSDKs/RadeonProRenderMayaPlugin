#pragma once

#include <vector>
#include "frWrap.h"

#include <maya/MCallbackIdArray.h>
#include <maya/MString.h>
#include <maya/MViewport2Renderer.h>
#include <maya/MDrawContext.h>
#include <maya/MFrameContext.h>
#include <maya/M3dView.h>
#include <maya/MUiMessage.h>
#include <maya/MShaderManager.h>
#include "FireRenderContext.h"
#include "FireRenderTextureCache.h"
#include <maya/MThreadAsync.h>
#include <maya/MTextureManager.h>

#include "RenderCacheWarningDialog.h"

/**
 * A viewport is responsible for rendering to a texture
 * that is then rendered to a Maya viewport panel.
 */
class FireRenderViewport
{

public:

	// Life Cycle
	// -----------------------------------------------------------------------------

	FireRenderViewport(const MString& panelName);
	~FireRenderViewport();


	// Public Methods
	// -----------------------------------------------------------------------------

	/** Set up the viewport for rendering to Maya. */
	MStatus setup();
	MStatus doSetup();

	/** Perform the threaded render operation. */
	bool RunOnViewportThread();

	/** The viewport is removed by changing the renderer or destroying the panel. */
	void removed(bool panelDestroyed);

	/** Called if the camera state has changed. */
	MStatus cameraChanged(MDagPath& cameraPath);

	/** Set to true to cache animation frames. */
	void setUseAnimationCache(bool value);

	/** Set to rendering mode to use */
	void setViewportRenderMode(int renderMode);

	/** Set current AOV to be visible */
	void setCurrentAOV(int aov);

	/** Return true if caching animation frames. */
	bool useAnimationCache();

	/** Clear the animation frame texture cache. */
	void clearTextureCache();

	/** Return the hardware texture. */
	const MHWRender::MTextureAssignment& getTexture() const;

	/** True if the texture has changed since the last call to getTexture. */
	bool hasTextureChanged();

	/** Update the viewport texture from the system memory frame buffer. */
	// TODO: no longer updates texture because it's too late (i.e. change the documentation)
	MStatus refresh();

	/** Perform actions immediately before blitting the viewport texture. */
	void preBlit();

	/** Perform actions immediately after blitting the viewport texture. */
	void postBlit();

	bool createFailed() const { return m_createFailed; }

    /** Returns current used M3dView object */
    const QWidget* getMayaWidget() const { return m_widget; }

    /** Finds M3dView object by panel name. Returns active M3dView if it can't be found by name */
    static MStatus FindMayaView(const MString& panelName, M3dView *view);
private:

	// Members
	// -----------------------------------------------------------------------------

	bool m_createFailed;

	/** The Maya 3D view containing the viewport. */
	M3dView m_view;

	/** Viewport render context. */
	FireRenderContext m_context;

	/** The description of the texture that will receive the RPR frame buffer. */
	MHWRender::MTextureDescription m_textureDesc;

	/** The texture that will receive the RPR frame buffer. */
	MHWRender::MTextureAssignment m_texture;

	/** True if the texture has changed since the last call to getTexture. */
	bool m_textureChanged;

	/** Viewport panel name. */
	MString m_panelName;

	/** True if the render thread is running. */
	volatile bool m_isRunning;

	/** True if rendered animation frames should be cached. */
	bool m_useAnimationCache;

	/** Cached frame buffer textures to use for animation playback. */
	FireMaya::TextureCache m_textureCache;

	/** Used for copying the RPR frame buffer to the hardware texture. */
	std::vector<RV_PIXEL> m_pixels;

	/** True if pixels have been updated. */
	bool m_pixelsUpdated;

	/** A lock to control access to the system memory frame buffer pixels. */
	MMutexLock m_pixelsLock;

	/** A lock to control access to the RPR context. */
	MMutexLock m_contextLock;

	/** Error handler. */
	FireRenderError m_error;

	int m_renderingErrors;

    QWidget* m_widget;

	int m_currentAOV;

	std::vector<int> m_alwaysEnabledAOVs;

	/** Warning Dialog for Shader Caching. */
	RenderCacheWarningDialog rcWarningDialog;
	bool m_showDialogNeeded;
	bool m_closeDialogNeeded;


	// Private Methods
	// -----------------------------------------------------------------------------

	/** Initialize resources. */
	bool initialize();

	/** Clean up resources. */
	void cleanUp();

	/** Get the viewport size. */
	MStatus getSize(unsigned int& width, unsigned int& height);

	/** Resize the viewport. */
	MStatus resize(unsigned int width, unsigned int height);

	/** Resize not using GL interop. */
	void resizeFrameBufferStandard(unsigned int width, unsigned int height);

	/** Resize using GL interop. */
	void resizeFrameBufferGLInterop(unsigned int width, unsigned int height);

	/** Clear the pixel buffer. */
	void clearPixels();

	/** Render a cached frame. */
	MStatus renderCached(unsigned int width, unsigned int height);

	/** Refresh the RPR context. */
	MStatus refreshContext();

	/** Read data from the RPR frame buffer into the texture. */
	void readFrameBuffer(FireMaya::StoredFrame* storedFrame = nullptr);

	/** Update the texture with the supplied frame data. */
	MStatus updateTexture(void* data, unsigned int width, unsigned int height);

	/** Start the render thread. */
	bool start();

	/** Stop the render thread. */
	bool stop();

	/** Add a menu to the Maya panel. */
	void addMenu();

	/** Remove the menu from the Maya panel. */
	void removeMenu();

	void enableNecessaryAOVs(int index, bool flag);

	bool isAOVShouldBeAlwaysEnabled(int aov);
};
