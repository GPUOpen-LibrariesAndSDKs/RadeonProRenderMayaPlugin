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
#include "Context/FireRenderContext.h"
#include "FireRenderTextureCache.h"
#include <maya/MThreadAsync.h>
#include <maya/MTextureManager.h>

#include "RenderCacheWarningDialog.h"

#include "NorthStarRenderingHelper.h"
#include "ViewportTexture.h"

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
	ViewportTexture* getTexture() const;

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

	bool ShouldBeRecreated() const { return m_contextPtr && !m_contextPtr->DoesContextSupportCurrentSettings(); }

	void OnBufferAvailableCallback(float progress);
private:

	// Members
	// -----------------------------------------------------------------------------

	bool m_createFailed;

	/** The Maya 3D view containing the viewport. */
	M3dView m_view;

	/** Viewport render context. */
	FireRenderContextPtr m_contextPtr;

	/** The texture that will receive the RPR frame buffer. */
	ViewportTexture m_texture;

	//
	ViewportTexture m_textureUpscaled;

	/** True if the texture has changed since the last call to getTexture. */
	bool m_textureChanged;

	/** Viewport panel name. */
	MString m_panelName;

	/** True if the render thread is running. */
	volatile bool m_isRunning;

	/** True if rendered animation frames should be cached. */
	bool m_useAnimationCache;

	/** Cached frame buffer textures to use for animation playback. */
	FireMaya::TextureCache m_renderedFramesCache;

	/** True if pixels have been updated. */
	bool m_pixelsUpdated;

	/** A lock to control access to the system memory frame buffer pixels. */
	std::mutex m_pixelsLock;

	/** A lock to control access to the RPR context. */
	std::mutex m_contextLock;

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

	NorthStarRenderingHelper m_NorthStarRenderingHelper;

	bool m_showUpscaledFrame;

	ViewportTexture* m_pCurrentTexture;

	// Private Methods
	// -----------------------------------------------------------------------------
private:

	/** Initialize resources. */
	bool initialize();

	/** Clean up resources. */
	void cleanUp();

	bool IsDenoiserUpscalerEnabled() const;

	friend void RenderUpdateCallback(float progress, void* pData);
	void OnRenderUpdateCallback(float progress);

	/** Get the viewport size. */
	MStatus getSize(unsigned int& width, unsigned int& height);

	/** Resize the viewport. */
	MStatus resize(unsigned int width, unsigned int height);

	/** Resize not using GL interop. */
	void resizeFrameBufferStandard(unsigned int width, unsigned int height);

	/** Render a cached frame. */
	MStatus renderCached(unsigned int width, unsigned int height);

	/** Refresh the RPR context. */
	MStatus refreshContext();

	/** Read data from the RPR frame buffer into the texture. */
	void readFrameBuffer(FireMaya::StoredFrame* storedFrame = nullptr, bool runUpscaler = false);

	/** Start the render thread. */
	bool start();

	/** Stop the render thread. */
	bool stop();

	/** Add a menu to the Maya panel. */
	void addMenu();

	/** Remove the menu from the Maya panel. */
	void removeMenu();

	void enableNecessaryAOVs(int index, bool flag, rpr_GLuint* glTexture);

	bool isAOVShouldBeAlwaysEnabled(int aov);

	rpr_GLuint* GetGlTexture() const;

	void ScheduleViewportUpdate();
};
