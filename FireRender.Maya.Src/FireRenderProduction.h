#pragma once

#pragma once

#include "FireRenderContext.h"
#include <maya/MDagPath.h>
#include <maya/MThreadAsync.h>
#include <maya/MCommonRenderSettingsData.h>
#include "RenderCacheWarningDialog.h"
#include "FireRenderAOVs.h"
#include "RenderProgressBars.h"
#include "RenderRegion.h"
#include "FireRenderGlobals.h"
#include "FireRenderUtils.h"

#include <functional>

/**
* Manages an production render session in the render view window.
*/
class FireRenderProduction
{
public:
	typedef std::function<void(FireRenderAOVs*, MCommonRenderSettingsData &)> stop_callback;

	// Life Cycle
	// -----------------------------------------------------------------------------

	FireRenderProduction();
	virtual ~FireRenderProduction();


	// Public Methods
	// -----------------------------------------------------------------------------

	/** True if the IPR render is running. */
	bool isRunning();

	/** True if an error occurred. */
	bool isError();

	/** True if render is cancelled*/
	bool isCancelled() const;

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

	/** Set the render region */
	void setRenderRegion(RenderRegion & value);

	/** Set the render camera. */
	void setCamera(MDagPath& camera);

	void setStopCallback(stop_callback callback);

	bool mainThreadPump();

	/** Waits for production render to complete on the main thread until complete */
	void waitForIt();
private:

	// Life Cycle
	// -----------------------------------------------------------------------------

	/** Copy constructor. */
	FireRenderProduction(const FireRenderProduction& other) = delete;


	// Operators
	// -----------------------------------------------------------------------------

	/** Assignment. */
	FireRenderProduction& operator=(const FireRenderProduction& other) = delete;


	// Private Methods
	// -----------------------------------------------------------------------------

	void RenderFullFrame();
	void RenderTiles();

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

private:

	// Members
	// -----------------------------------------------------------------------------

	/** The FireRender context. */
	std::shared_ptr<FireRenderContext> m_context;

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

	/** True is it is being canceled */
	std::atomic_bool m_cancelled;

	bool m_needsContextRefresh;

	/** True if a render view update is scheduled. */
	tbb::atomic<bool> m_renderViewUpdateScheduled;

	/** A lock to control access to the system memory frame buffer pixels. */
	MMutexLock m_pixelsLock;

	/** A lock to control access to the RPR context. */
	MMutexLock m_contextLock;

	/** A lock to control access to the RPR context creation and destruction. */
	MMutexLock m_contextCreationLock;

	/** Error handler. */
	FireRenderError m_error;

	/** FireRender AOVs */
	FireRenderAOVs * m_aovs;
	FireRenderAOV * m_renderViewAOV;

	/** Render Progress Bars */
	std::unique_ptr<RenderProgressBars> m_progressBars;

	MCommonRenderSettingsData m_settings;

	/** Fire Render Globals Data */
	FireRenderGlobalsData m_globals;

	/** Warning Dialog for Shader Caching. */
	RenderCacheWarningDialog rcWarningDialog;

	stop_callback m_stopCallback;
};
