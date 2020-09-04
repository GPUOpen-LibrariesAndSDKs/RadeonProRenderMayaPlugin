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

#include "Context/TahoeContext.h"
#include <maya/MDagPath.h>
#include <maya/MThreadAsync.h>
#include "RenderCacheWarningDialog.h"
#include "maya/MSelectionList.h"

#include <mutex>

/**
 * Manages an interactive photo real (IPR)
 * render session in the render view window.
 */
class FireRenderIpr
{
public:

	// Life Cycle
	// -----------------------------------------------------------------------------

	FireRenderIpr();
	virtual ~FireRenderIpr();


	// Public Methods
	// -----------------------------------------------------------------------------

	/** True if the IPR render is running. */
	bool isRunning();

	/** True if an error occurred. */
	bool isError();

	/** Start a threaded IPR render. */
	bool start();

	/** Pause the threaded IPR render. */
	bool pause(bool value);

	/** Stop the current threaded IPR render. */
	bool stop();

	/** Perform the threaded render operation. */
	bool RunOnViewportThread();

	/** Update the render region. */
	void updateRegion();

	/** Set the frame buffer resolution. */
	void setResolution(unsigned int w, unsigned int h);

	/** Set the render camera. */
	void setCamera(MDagPath& camera);

	/** Update the Maya render view. */
	void updateRenderView();

private:

	// Life Cycle
	// -----------------------------------------------------------------------------

	/** Copy constructor. */
	FireRenderIpr(const FireRenderIpr& other) = delete;


	// Operators
	// -----------------------------------------------------------------------------

	/** Assignment. */
	FireRenderIpr& operator=(const FireRenderIpr& other) = delete;


	// Private Methods
	// -----------------------------------------------------------------------------

	/** Schedule a render view update. */
	void scheduleRenderViewUpdate();

	/** Start the Maya render view render. */
	void startMayaRender();

	/** Stop the Maya render view render. */
	void stopMayaRender();

	/** Read data from the RPR frame buffer into the texture. */
	void readFrameBuffer();

	/** Refresh the context. */
	void refreshContext();

	/** Process selection change. */
	void CheckSelection();

	/** Set up Out of Core textures parameters for context */
	void SetupOOC(FireRenderGlobalsData& globals);

	/** Updates Render information at the bottom of Maya's render view */
	void updateMayaRenderInfo();

private:

	// Members
	// -----------------------------------------------------------------------------

	/** The FireRender context. */
	FireRenderContextPtr m_contextPtr;

	/** The current camera. */
	MDagPath m_camera;

	/** Frame buffer width. */
	unsigned int m_width;

	/** Frame buffer height. */
	unsigned int m_height;

	/** The region to render. */
	RenderRegion m_region;

	/** Frame buffer pixel data for copying to the render view. */
	std::vector<RV_PIXEL> m_pixels;

	/** True if rendering to a region. */
	bool m_isRegion;

	/** True if the render has started. */
	bool m_renderStarted;

	/** True if the IPR is running. */
	bool m_isRunning;

	/** True if the IPR is paused. */
	bool m_isPaused;

	bool m_needsContextRefresh;

	unsigned int m_currentAOVToDisplay;

	/** True if a render view update is scheduled. */
	tbb::atomic<bool> m_renderViewUpdateScheduled;

	/** A lock to control access to the system memory frame buffer pixels. */
	MMutexLock m_pixelsLock;

	/** A lock to control access to the RPR context. */
	MMutexLock m_contextLock;

	/** Error handler. */
	FireRenderError m_error;

	/** Warning Dialog for Shader Caching. */
	RenderCacheWarningDialog rcWarningDialog;

	/** Mutex for syncronization of updateRegion and readFrameBuffer methods. */
	std::mutex m_regionUpdateMutex;

	MSelectionList m_previousSelectionList;
};
