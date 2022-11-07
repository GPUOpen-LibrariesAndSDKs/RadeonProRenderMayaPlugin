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
#include <cassert>
#include <sstream>
#include <functional>

#include "common.h"
#include "frWrap.h"
#include <maya/MNodeMessage.h>
#include <maya/MDagPath.h>
#include <maya/M3dView.h>
#include <maya/MFnCamera.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MImage.h>
#include <maya/MFileIO.h>
#include <maya/MGlobal.h>
#include <maya/MRenderView.h>
#include <maya/MAnimControl.h>
#include <maya/MTextureManager.h>
#include "AutoLock.h"

#include "Context/ContextCreator.h"
#include "Context/FireRenderContext.h"
#include "FireRenderViewport.h"
#include "FireRenderThread.h"

using namespace std;
using namespace std::chrono_literals;
using namespace MHWRender;
using namespace RPR;
using namespace FireMaya;

//#define HIGHLIGHT_TEXTURE_UPDATES	1	// debugging: every update will draw a color line on top of the rendered picture

MStatus FireRenderViewport::FindMayaView(const MString& panelName, M3dView *view)
{
    // Get the Maya 3D view.
    MStatus status = M3dView::getM3dViewFromModelPanel(panelName, *view);
    //Unable to find M3dView. This happens in hypershade vieport. Use active one.
    if (status != MStatus::kSuccess)
        *view = M3dView::active3dView(&status);

    return status;
}

// Life Cycle
// -----------------------------------------------------------------------------
FireRenderViewport::FireRenderViewport(const MString& panelName) :
	m_isRunning(false),
	m_useAnimationCache(true),
	m_pixelsUpdated(false),
	m_panelName(panelName),
	m_textureChanged(false),
	m_showDialogNeeded(false),
	m_closeDialogNeeded(false),
	m_showUpscaledFrame(false),
	m_createFailed(false),
	m_currentAOV(RPR_AOV_COLOR),
	m_pCurrentTexture(nullptr)
{
	m_alwaysEnabledAOVs.push_back(RPR_AOV_COLOR);
	m_alwaysEnabledAOVs.push_back(RPR_AOV_VARIANCE);

	// Initialize.
	if (!initialize())
		m_createFailed = true;

    if (!FindMayaView(panelName, &m_view))
        m_createFailed = true;

    if (!m_createFailed)
        m_widget = m_view.widget();
    else
        m_widget = nullptr;

	// Add the RPR panel menu.
	addMenu();
}

// -----------------------------------------------------------------------------
FireRenderViewport::~FireRenderViewport()
{
	// Stop the render thread if required. Executed in context of the main thread.
	stop();
	// Now cleanup resources in context of rendering thread.
	// TODO: check - may be it is not required to call this in thread context, because if thread is not
	// running anymore - it is safe to call it from any thread.
	FireRenderThread::RunOnceProcAndWait([this]()
	{
		cleanUp();
	});
}


// Public Methods
// -----------------------------------------------------------------------------
// How Maya executes callbacks for refreshing viewport:
// FireRenderOverride::setup
//   -> FireRenderViewport::setup
//     -> FireRenderViewportBlit::updateTexture
// FireRenderOverride::startOperationIterator
// FireRenderOverride::nextRenderOperation (several times) - texture is displayed in viewport here
// FireRenderViewportManager::postRenderMsgCallback
//   -> FireRenderViewport::refresh
// FireRenderOverride::cleanup
MStatus FireRenderViewport::setup()
{
	MAIN_THREAD_ONLY;

	// Check if updating viewport's texture is required.
	// This code is needed for CPU rendering case 
	if (m_pixelsUpdated)
	{
		// Acquire the pixels lock.
		AutoMutexLock pixelsLock(m_pixelsLock);

		// Update the Maya texture from the internal pixel data.
		m_pCurrentTexture->UpdateTexture();
	}

	return doSetup();
}

bool FireRenderViewport::IsDenoiserUpscalerEnabled() const
{
	if (m_contextPtr == nullptr || m_currentAOV != RPR_AOV_COLOR)
	{
		return false;
	}

	if (!NorthStarContext::IsGivenContextNorthStar(m_contextPtr.get()))
	{
		return false;
	}

	return m_contextPtr->IsDenoiserEnabled();
}

MStatus FireRenderViewport::doSetup()
{
	MAIN_THREAD_ONLY;

	// Check for errors.
	if (m_error.check())
		return MStatus::kFailure;

	// Get the viewport dimensions.
	unsigned int width = 0;
	unsigned int height = 0;
	MStatus status = getSize(width, height);
	if (status != MStatus::kSuccess)
		return status;

	// Update render limits based on animation state.
	bool animating = MAnimControl::isPlaying() || MAnimControl::isScrubbing();
	m_contextPtr->updateLimits(animating);

	// Check if animation caching should be used.
	bool useAnimationCache =
		animating && m_useAnimationCache;

	// Stop the viewport render thread if using cached frames.
	if (m_isRunning && useAnimationCache)
		stop();

	if (IsDenoiserUpscalerEnabled())
	{
		width /= 2;
		height /= 2;
	}
	// Check if the viewport size has changed.
	if (width != m_contextPtr->width() || height != m_contextPtr->height())
	{
		status = resize(width, height);
		if (status != MStatus::kSuccess)
			return status;
	}

	// Refresh the context if required.
	status = refreshContext();
	if (status != MStatus::kSuccess)
		return status;

	// Force update viewport only if no production is running
	// TODO: Use different threads for different tasks, so this check would be unneccessary
	int isProductionRunning = 1;
	MGlobal::executeCommand("fireRender -isProductionRunning", isProductionRunning);
	if (0 == isProductionRunning)
	{
		FireRenderThread::RunItemsQueuedForTheMainThread();
	}

	// Check for errors again - the render thread may have
	// encountered an error since the start of this method.
	if (m_error.check())
		return MStatus::kFailure;

	// Render a cached frame if required.
	if (useAnimationCache)
	{
		status = renderCached(width, height);
		if (status != MStatus::kSuccess)
			return status;
	}

	// Otherwise, ensure the render thread is running.
	else if (!m_isRunning)
		start();

	// Viewport setup complete.
	return MStatus::kSuccess;
}

// -----------------------------------------------------------------------------
void FireRenderViewport::removed(bool panelDestroyed)
{
	// Do nothing if the containing panel is being destroyed.
	if (panelDestroyed)
		return;

	// Otherwise, remove the panel menu.
	removeMenu();
}

void FireRenderViewport::ScheduleViewportUpdate()
{
	FireRenderThread::RunProcOnMainThread([&]()
	{
		// Schedule a Maya viewport refresh or set exit flag
		MStatus status;
		M3dView activeView;
		status = M3dView::getM3dViewFromModelPanel(m_panelName, activeView);
		if (status == MStatus::kSuccess) // Regular render view
		{
			m_view.scheduleRefresh();
		}
		else //Standalone render view (hypershade only?)
		{
			activeView = M3dView::active3dView(&status);
			if (activeView.widget() == m_widget)
				m_view.scheduleRefresh();
			else
				m_contextPtr->SetState(FireRenderContext::StateExiting);
		}
	});
}


void FireRenderViewport::OnBufferAvailableCallback(float progress)
{
	// Get the frame hash.
	auto hash = m_contextPtr->GetStateHash();
	stringstream ss;
	ss << m_panelName.asChar() << ";" << size_t(hash);

	// Try find the frame for the hash.
	// if not found => creates new frame in cache
	auto& frame = m_renderedFramesCache[ss.str().c_str()];

	readFrameBuffer(&frame);

	ScheduleViewportUpdate();
}

// -----------------------------------------------------------------------------
bool FireRenderViewport::RunOnViewportThread()
{
	RPR_THREAD_ONLY;

	switch (m_contextPtr->GetState())
	{
		// The context is exiting.
	case FireRenderContext::StateExiting:
		return false;

		// The context is rendering.
	case FireRenderContext::StateRendering:
	{
		// Check if a render is required. Note that this code will be executed
		if (m_contextPtr->needsRedraw() ||				// context needs redrawing
			m_contextPtr->cameraAttributeChanged() ||	// camera changed
			m_contextPtr->keepRenderRunning())			// or we must render just because rendering is not yet completed
		{
			try
			{
				m_showUpscaledFrame = false;
				m_pCurrentTexture = &m_texture;

				FireRenderContext::Lock lock(m_contextPtr.get(), "FireRenderContext::StateRendering"); // lock with constructor which will not change state

				// Perform a render iteration.
				{
					AutoMutexLock contextLock(m_contextLock);
					
					if (!NorthStarContext::IsGivenContextNorthStar(m_contextPtr.get()))
					{
						AutoMutexLock pixelsLock(m_pixelsLock);

						m_contextPtr->render(false);
						m_closeDialogNeeded = true;

						readFrameBuffer();
					}
					else
					{
						m_contextPtr->render(false);
						m_closeDialogNeeded = true;

						readFrameBuffer();
					}
				}

				if (m_renderingErrors > 0)
					m_renderingErrors--;
			}
			catch (...)
			{
				m_view.scheduleRefresh();
				m_renderingErrors++;
				DebugPrint("Failed to Render Viewport: %d errors in a row!", m_renderingErrors);
				if (m_renderingErrors > 3)
					throw;
			}

			ScheduleViewportUpdate();
		}
		else
		{
			if (IsDenoiserUpscalerEnabled() && !m_showUpscaledFrame)
			{
				{
					AutoMutexLock contextLock(m_contextLock);
					AutoMutexLock pixelsLock(m_pixelsLock);

					m_showUpscaledFrame = true;

					readFrameBuffer(nullptr, true);

					m_pCurrentTexture = &m_textureUpscaled;
				}

				ScheduleViewportUpdate();
			}
			else
			{
				// Don't waste CPU time too much when not rendering
				this_thread::sleep_for(2ms);
			}
		}

		return true;
	}

	case FireRenderContext::StatePaused:	// The context is paused.
	case FireRenderContext::StateUpdating:	// The context is updating.
	default:								// Handle all other cases.
		this_thread::sleep_for(5ms);
		return true;
	}
}

// -----------------------------------------------------------------------------
bool FireRenderViewport::start()
{
	// Stop before restarting if already running.
	if (m_isRunning)
		stop();

	// Check dimensions are valid.
	if (m_contextPtr->width() == 0 || m_contextPtr->height() == 0)
		return false;

	// Start rendering.
	{
		// We should lock context, otherwise another asynchronous lock could
		// change context's state, and rendering will stall in StateUpdating.
		FireRenderContext::Lock lock(m_contextPtr.get(), "FireRenderViewport::start"); // lock constructor which do not change state
		m_contextPtr->SetState(FireRenderContext::StateRendering);
	}

	m_isRunning = true;
	m_renderingErrors = 0;

	FireRenderThread::KeepRunning([this]()
	{
		try
		{
			m_isRunning = this->RunOnViewportThread();
		}
		catch (...)
		{
			m_isRunning = false;
			m_error.set(current_exception());
		}

		return m_isRunning;
	});

	m_NorthStarRenderingHelper.Start();

	return true;
}

// -----------------------------------------------------------------------------
bool FireRenderViewport::stop()
{
	MAIN_THREAD_ONLY;
	
	m_NorthStarRenderingHelper.SetStopFlag();

	// should wait for thread
	// m_isRunning could be not updated when exiting Maya during rendering, so check for two conditions
	while (m_isRunning && FireRenderThread::IsThreadRunning())
	{
		//Run items queued for main thread
		FireRenderThread::RunItemsQueuedForTheMainThread();

		// terminate the thread
		m_contextPtr->SetState(FireRenderContext::StateExiting);
		this_thread::sleep_for(10ms); // 10.03.2017 - perhaps this is better than yield()
	}

	m_NorthStarRenderingHelper.StopAndJoin();

	if (rcWarningDialog.shown && m_closeDialogNeeded)
		rcWarningDialog.close();

	return true;
}

// -----------------------------------------------------------------------------
void FireRenderViewport::setUseAnimationCache(bool value)
{
	m_useAnimationCache = value;
	m_view.scheduleRefresh();
}

// -----------------------------------------------------------------------------
void FireRenderViewport::setViewportRenderMode(int renderMode)
{
	FireRenderThread::RunOnceProcAndWait([this, renderMode]()
	{
		AutoMutexLock contextLock(m_contextLock);
		m_contextPtr->setRenderMode(static_cast<FireRenderContext::RenderMode>(renderMode));
		m_contextPtr->setDirty();
		m_view.scheduleRefresh();
	});
}

// -----------------------------------------------------------------------------
bool FireRenderViewport::useAnimationCache()
{
	return m_useAnimationCache;
}

// -----------------------------------------------------------------------------
void FireRenderViewport::clearTextureCache()
{
	m_renderedFramesCache.Clear();
	m_view.scheduleRefresh();
}

// -----------------------------------------------------------------------------
MStatus FireRenderViewport::cameraChanged(MDagPath& cameraPath)
{
	auto status = FireRenderThread::RunOnceAndWait<MStatus>([this, &cameraPath]() -> MStatus
	{

		AutoMutexLock contextLock(m_contextLock);

		try
		{
			m_contextPtr->setCamera(cameraPath, true);
			m_contextPtr->setDirty();
		}
		catch (...)
		{
			m_error.set(current_exception());
			return MStatus::kFailure;
		}

		return MStatus::kSuccess;
	});

	return status;
}

// -----------------------------------------------------------------------------
MStatus FireRenderViewport::refresh()
{
	if (rcWarningDialog.shown && m_closeDialogNeeded)
	{
		rcWarningDialog.close();
	}
	else if (m_showDialogNeeded)
	{
		m_showDialogNeeded = false;
		rcWarningDialog.show();
	}

	// Check for errors.
	if (m_error.check())
		return MStatus::kFailure;

	return MStatus::kSuccess;
}

// -----------------------------------------------------------------------------
void FireRenderViewport::preBlit()
{
}

// -----------------------------------------------------------------------------
void FireRenderViewport::postBlit()
{
}


// Private Methods
// -----------------------------------------------------------------------------
bool FireRenderViewport::initialize()
{
	try
	{
		m_contextPtr = ContextCreator::CreateAppropriateContextForRenderType(RenderType::ViewportRender);
		m_contextPtr->SetRenderType(RenderType::ViewportRender);

		m_pCurrentTexture = &m_texture;

		// Initialize the RPR context.
		bool animating = MAnimControl::isPlaying() || MAnimControl::isScrubbing();
		bool glViewport = MRenderer::theRenderer()->drawAPIIsOpenGL();

		// enable all mandatory aovs so that it can be resolved and used properly
		for (int aov : m_alwaysEnabledAOVs)
		{
			m_contextPtr->enableAOV(aov);
		}

		if (!isAOVShouldBeAlwaysEnabled(m_currentAOV))
		{
			m_contextPtr->enableAOV(m_currentAOV);
		}

		if (!m_contextPtr->buildScene(true, glViewport))
		{
			return false;
		}

		if (NorthStarContext::IsGivenContextNorthStar(m_contextPtr.get()))
		{
			m_NorthStarRenderingHelper.SetData(m_contextPtr.get(), std::bind(&FireRenderViewport::OnBufferAvailableCallback, this, std::placeholders::_1));
		}
	}
	catch (...)
	{
		m_error.set(current_exception());
		return false;
	}

	return true;
}

// -----------------------------------------------------------------------------
void FireRenderViewport::cleanUp()
{
	// Clean the RPR scene.
	m_contextPtr->cleanScene();

	// Delete the hardware backed texture.
	// Do not delete when exiting Maya - this will cause access violation
	// in texture manager.

	if (!gExitingMaya)
	{
		MRenderer* renderer = MRenderer::theRenderer();
		MTextureManager* textureManager = renderer->getTextureManager();

		m_texture.Release();
		m_textureUpscaled.Release();
	}
}

// -----------------------------------------------------------------------------
MStatus FireRenderViewport::getSize(unsigned int& width, unsigned int& height)
{
	// Get the viewport size from the renderer.
	MRenderer* renderer = MRenderer::theRenderer();
	MStatus status = renderer->outputTargetSize(width, height);
	if (status != MStatus::kSuccess)
		return status;

	// Clamp the maximum size to increase
	// performance and reduce memory usage.
	if (width > height && width > FRMAYA_GL_MAX_TEXTURE_SIZE)
	{
		height = height * FRMAYA_GL_MAX_TEXTURE_SIZE / width;
		width = FRMAYA_GL_MAX_TEXTURE_SIZE;
	}
	else if (height > FRMAYA_GL_MAX_TEXTURE_SIZE)
	{
		width = width * FRMAYA_GL_MAX_TEXTURE_SIZE / height;
		height = FRMAYA_GL_MAX_TEXTURE_SIZE;
	}

	// Success.
	return MStatus::kSuccess;
}

// -----------------------------------------------------------------------------
MStatus FireRenderViewport::resize(unsigned int width, unsigned int height)
{
	// Acquire the context and pixels locks.
	AutoMutexLock contextLock(m_contextLock);
	AutoMutexLock pixelsLock(m_pixelsLock);

	try
	{
		// Clear the texture cache - all frames
		// need to be re-rendered at the new size.
		m_renderedFramesCache.Clear();

		if (m_contextPtr->isFirstIterationAndShadersNOTCached()) {
			//first iteration and shaders are _NOT_ cached
			m_closeDialogNeeded = false;
			m_showDialogNeeded = true;
		}

		// Resize the frame buffer.
		resizeFrameBufferStandard(width, height);

		// Update the camera.
		M3dView mView;
        MStatus status = FindMayaView(m_panelName, &mView);
        if (status != MStatus::kSuccess)
            return status;

		MDagPath cameraPath;
		status = mView.getCamera(cameraPath);
		if (status != MStatus::kSuccess)
			return status;

		if (cameraPath.isValid())
			m_contextPtr->setCamera(cameraPath, true);


		// Invalidate the context.
		m_contextPtr->setDirty();
	}
	catch (...)
	{
		m_error.set(current_exception());
		return MStatus::kFailure;
	}

	// Resize completed successfully.
	return MStatus::kSuccess;
}

// -----------------------------------------------------------------------------
void FireRenderViewport::resizeFrameBufferStandard(unsigned int width, unsigned int height)
{
	// Update the RPR context dimensions.
	m_contextPtr->resize(width, height, false);

	// Resize the pixel buffer that
	// will receive frame buffer data.
	m_texture.Resize(width, height);

	if (IsDenoiserUpscalerEnabled())
	{
		m_textureUpscaled.Resize(width * 2, height * 2);
	}
	else
	{
		m_textureUpscaled.Release();
	}
}

// -----------------------------------------------------------------------------
rpr_GLuint* FireRenderViewport::GetGlTexture() const
{
	return static_cast<rpr_GLuint*>(m_texture.GetTexture()->resourceHandle());
}

// -----------------------------------------------------------------------------
MStatus FireRenderViewport::renderCached(unsigned int width, unsigned int height)
{
	// Clear the pixels updated flag so the non-cached frame
	// buffer data doesn't get written to the texture post render.
	m_pixelsUpdated = false;

	try
	{
		// Get the frame hash.
		auto hash = m_contextPtr->GetStateHash();
		stringstream ss;
		ss << m_panelName.asChar() << ";" << size_t(hash);

		// Try find the frame for the hash.
		// if not found => creates new frame in cache
		auto& frame = m_renderedFramesCache[ss.str().c_str()];

		// Render the frame if required.
		bool shouldRender = frame.Resize(width, height); // returns false if frame is not empty
		if (shouldRender)
		{
			AutoMutexLock contextLock(m_contextLock);

			m_contextPtr->render();
			readFrameBuffer(&frame);

			return m_texture.UpdateTexture(frame.data());
		}
		else // Otherwise, update the texture from the frame data.
		{
			return m_texture.UpdateTexture(frame.data());
		}
	}
	catch (...)
	{
		m_error.set(current_exception());
		return MStatus::kFailure;
	}
}

// -----------------------------------------------------------------------------
MStatus FireRenderViewport::refreshContext()
{
	RPR_THREAD_ONLY;

	if (!m_contextPtr->isDirty())
		return MStatus::kSuccess;

	try
	{
		m_contextPtr->Freshen(true);

		return MStatus::kSuccess;
	}
	catch (...)
	{
		m_error.set(current_exception());
		return MStatus::kFailure;
	}
}

// -----------------------------------------------------------------------------
void FireRenderViewport::readFrameBuffer(FireMaya::StoredFrame* storedFrame, bool runDenoiserAndUpscaler/* = false*/)
{
	// Read the frame buffer.
	RenderRegion region(0, m_contextPtr->width() - 1, m_contextPtr->height() - 1, 0);

	FireRenderContext::ReadFrameBufferRequestParams params(region);
	params.aov = m_currentAOV;
	params.width = m_contextPtr->width();
	params.height = m_contextPtr->height();
	params.mergeOpacity = false;
	params.shadowColor = m_contextPtr->m_shadowColor;
	params.shadowTransp = m_contextPtr->m_shadowTransparency;

	// Read to a cached frame if supplied.
	if (storedFrame)
	{
		// setup params
		params.pixels = reinterpret_cast<RV_PIXEL*>(storedFrame->data());
		params.mergeShadowCatcher = false;

		// process frame buffer	
		m_contextPtr->readFrameBufferSimple(params);
	}

	// Otherwise, read to a temporary buffer.
	else
	{
		// setup params
		params.pixels = (RV_PIXEL*) m_texture.GetPixelData();
		params.mergeShadowCatcher = true;

		// process frame buffer
		m_contextPtr->readFrameBufferSimple(params);

		if (runDenoiserAndUpscaler)
		{
			m_textureUpscaled.SetPixelData(m_contextPtr->DenoiseAndUpscaleForViewport());
			m_textureChanged = true;
		}

		// Flag as updated so the pixels will
		// be copied to the viewport texture.
		m_pixelsUpdated = true;
#if HIGHLIGHT_TEXTURE_UPDATES
		static const RV_PIXEL colors[6] =
		{
			{ 1, 0, 0, 1 },
			{ 0, 1, 0, 1 },
			{ 0, 0, 1, 1 },
			{ 1, 1, 0, 1 },
			{ 0, 1, 1, 1 },
			{ 1, 0, 1, 1 }
		};
		static int nn = 0;
		RV_PIXEL c = colors[nn];
		if (++nn == 6) nn = 0;
		LogPrint(">>> fill: %g %g %g", c.r, c.g, c.b);
		for (int i = 0; i < m_contextPtr->width() * 8; i += m_contextPtr->width())
		{
			for (int j = 0; j < 8; j++)
				m_pixels[i + j] = c;
		}
#endif // HIGHLIGHT_TEXTURE_UPDATES
	}
}

// -----------------------------------------------------------------------------
ViewportTexture* FireRenderViewport::getTexture() const
{
	return m_pCurrentTexture;
}

// -----------------------------------------------------------------------------
bool FireRenderViewport::hasTextureChanged()
{
	bool changed = m_textureChanged;
	m_textureChanged = false;
	return changed;
}

void FireRenderViewport::enableNecessaryAOVs(int index, bool flag, rpr_GLuint* glTexture)
{
	static const std::vector<int> rcaov = { RPR_AOV_BACKGROUND, RPR_AOV_OPACITY };
	static const std::vector<int> scaov = { RPR_AOV_MATTE_PASS };

	m_contextPtr->enableAOVAndReset(index, flag, glTexture);

	if (index == RPR_AOV_SHADOW_CATCHER)
	{
		for (int index : scaov)
		{
			m_contextPtr->enableAOVAndReset(index, flag, nullptr);
		}
	}

	if (index == RPR_AOV_REFLECTION_CATCHER)
	{
		for (int index : rcaov)
		{
			m_contextPtr->enableAOVAndReset(index, flag, nullptr);
		}
	}
}

bool FireRenderViewport::isAOVShouldBeAlwaysEnabled(int aov)
{
	for (int aovToTest : m_alwaysEnabledAOVs)
	{
		if (aovToTest == aov)
		{
			return true;
		}
	}

	return false;
}

void FireRenderViewport::setCurrentAOV(int aov)
{
	if (m_currentAOV == aov)
	{
		return;
	}

	if (!m_contextPtr->IsAOVSupported(aov))
	{
		MGlobal::displayError("Selected AOV is not supported in the given Render Quality");
		return;
	}

	AutoMutexLock contextLock(m_contextLock);

	// turning off previous selected aov if it is not color        
	if (!isAOVShouldBeAlwaysEnabled(m_currentAOV))
	{
		enableNecessaryAOVs(m_currentAOV, false, nullptr);
	}

	// turning on newly selected aov(s)
	if (!isAOVShouldBeAlwaysEnabled(aov))
	{
		enableNecessaryAOVs(aov, true, GetGlTexture());
	}

	m_contextPtr->setDirty();
	refreshContext();

	m_currentAOV = aov;
}

// -----------------------------------------------------------------------------
void FireRenderViewport::addMenu()
{
	// The add menu command string.
	MString command = 
			R"(from PySide2 import QtCore, QtWidgets, QtGui
import shiboken2
import maya.OpenMayaUI as omu
import sys
def setFireRenderAnimCache(checked=True):
	maya.cmds.fireRenderViewport(panel=maya.cmds.getPanel(wf=1),cache=checked)
def clearFireRenderCache():
	maya.cmds.fireRenderViewport(panel=maya.cmds.getPanel(wf=1),clear=True)
def setFireViewportMode_globalIllumination(checked=True):
	maya.cmds.fireRenderViewport(panel=maya.cmds.getPanel(wf=1),viewportMode="globalIllumination")
def setFireViewportMode_directIllumination(checked=True):
	maya.cmds.fireRenderViewport(panel=maya.cmds.getPanel(wf=1),viewportMode="directIllumination")
def setFireViewportMode_directIlluminationNoShadow(checked=True):
	maya.cmds.fireRenderViewport(panel=maya.cmds.getPanel(wf=1),viewportMode="directIlluminationNoShadow")
def setFireViewportMode_wireframe(checked=True):
	maya.cmds.fireRenderViewport(panel=maya.cmds.getPanel(wf=1),viewportMode="wireframe")
def setFireViewportMode_materialId(checked=True):
	maya.cmds.fireRenderViewport(panel=maya.cmds.getPanel(wf=1),viewportMode="materialId")
def setFireViewportMode_position(checked=True):
	maya.cmds.fireRenderViewport(panel=maya.cmds.getPanel(wf=1),viewportMode="position")
def setFireViewportMode_normal(checked=True):
	maya.cmds.fireRenderViewport(panel=maya.cmds.getPanel(wf=1),viewportMode="normal")
def setFireViewportMode_texcoord(checked=True):
	maya.cmds.fireRenderViewport(panel=maya.cmds.getPanel(wf=1),viewportMode="texcoord")
def setFireViewportMode_ambientOcclusion(checked=True):
	maya.cmds.fireRenderViewport(panel=maya.cmds.getPanel(wf=1),viewportMode="ambientOcclusion")

def createAOVsMenu(frMenu):

	# numbers in the following arrays are IDs of RPR AOVs that are declared in RadeonProRender.h
	aov_ids = [0, 1, 2, 3, 4, 5, 6, 41, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28]

	def setFireViewportAOV(aov):
		maya.cmds.fireRenderViewport(panel=maya.cmds.getPanel(wf=1),viewportAOV=aov)

	# Maya 2020 has broken lambdas when used as a callback for unknown reason. Explicit version works.
	def activate_aov(index):
		def wrapped(*args, **kwargs):
			setFireViewportAOV(aov_ids[index])

		return wrapped

	frSubMenu = frMenu.addMenu("AOV")

	aovs = ["Color", "Opacity", "World Corrdinate", "UV", "Material Idx", "Geometric Normal", "Shading Normal", "Camera Normal", "Depth", "Object ID", "Object Group ID"]
	aovs.extend(["Shadow Catcher", "Background", "Emission", "Velocity", "Direct Illumination", "Indirect Illumination", "AO", "Direct Diffuse"])
	aovs.extend(["Direct Reflect", "Indirect Diffuse", "Indirect Reflect", "Refract", "Subsurface / Volume"]) 
	aovs.extend(["Light Group 0", "Light Group 1", "Light Group 2", "Light Group 3", "Albedo", "Variance"])

	ag = QtWidgets.QActionGroup(frSubMenu)
	count = 0
	for aov in aovs:
		action = frSubMenu.addAction(aov)
		action.triggered.connect(activate_aov(count))
		action.setActionGroup(ag)
		action.setCheckable(True)
		if count == 0 :
			action.setChecked(True)
		count = count + 1

if sys.version_info[0] < 3:
	ptr = omu.MQtUtil.findControl("m_panelName", long(omu.MQtUtil.mainWindow()))
	w = shiboken2.wrapInstance(long(ptr), QtWidgets.QWidget)
else:
	ptr = omu.MQtUtil.findControl("m_panelName", int(omu.MQtUtil.mainWindow()))
	w = shiboken2.wrapInstance(int(ptr), QtWidgets.QWidget)

menuBar = w.findChildren(QtWidgets.QMenuBar)[0]
frExist = False
for act in menuBar.actions():
	if act.text() == "FIRE_RENDER_NAME":
		frExist = True
if not frExist:
	frMenu = menuBar.addMenu("FIRE_RENDER_NAME")
	animAction = frMenu.addAction("Animation cache")
	animAction.setCheckable(True)
	animAction.setChecked(True)
	animAction.toggled.connect(setFireRenderAnimCache)
	action = frMenu.addAction("Clear animation cache")
	action.triggered.connect(clearFireRenderCache)

	createAOVsMenu(frMenu)

	frSubMenu = frMenu.addMenu("Viewport Mode")
	ag = QtWidgets.QActionGroup(frSubMenu)
	action = frSubMenu.addAction("globalIllumination")
	action.setActionGroup(ag)
	action.setCheckable(True)
	action.setChecked(True)
	action.triggered.connect(setFireViewportMode_globalIllumination)

	action = frSubMenu.addAction("directIllumination")
	action.setCheckable(True)
	action.setActionGroup(ag)
	action.triggered.connect(setFireViewportMode_directIllumination)

	action = frSubMenu.addAction("directIlluminationNoShadow")
	action.setCheckable(True)
	action.setActionGroup(ag)
	action.triggered.connect(setFireViewportMode_directIlluminationNoShadow)

	action = frSubMenu.addAction("wireframe")
	action.setCheckable(True)
	action.setActionGroup(ag)
	action.triggered.connect(setFireViewportMode_wireframe)

	action = frSubMenu.addAction("materialId")
	action.setCheckable(True)
	action.setActionGroup(ag)
	action.triggered.connect(setFireViewportMode_materialId)

	action = frSubMenu.addAction("position")
	action.setCheckable(True)
	action.setActionGroup(ag)
	action.triggered.connect(setFireViewportMode_position)

	action = frSubMenu.addAction("normal")
	action.setCheckable(True)
	action.setActionGroup(ag)
	action.triggered.connect(setFireViewportMode_normal)

	action = frSubMenu.addAction("texcoord")
	action.setCheckable(True)
	action.setActionGroup(ag)
	action.triggered.connect(setFireViewportMode_texcoord)

	action = frSubMenu.addAction("ambientOcclusion")
	action.setCheckable(True)
	action.setActionGroup(ag)
	action.triggered.connect(setFireViewportMode_ambientOcclusion)
)";

	command.substitute("m_panelName", m_panelName);
	command.substitute("FIRE_RENDER_NAME", FIRE_RENDER_NAME);

	MGlobal::executePythonCommand(command);
}

// -----------------------------------------------------------------------------
void FireRenderViewport::removeMenu()
{
	// The remove menu command string.
	MString command;

	// Maya 2017 uses newer Python APIs, so create a
	// different command for the different Maya versions.
	command =
			"from PySide2 import QtCore, QtWidgets\n"
			"import shiboken2\n"
			"import maya.OpenMayaUI as omu\n"
			"ptr = omu.MQtUtil.findControl(\"" + m_panelName + "\", long(omu.MQtUtil.mainWindow()))\n"
			"w = shiboken2.wrapInstance(long(ptr), QtWidgets.QWidget)\n"
			"menuBar = w.findChildren(QtWidgets.QMenuBar)[0]\n"
			"frExist = False\n"
			"for act in menuBar.actions():\n"
			"\tif act.text() == \"" + FIRE_RENDER_NAME + "\":\n"
			"\t\tmenuBar.removeAction(act)\n";

	MGlobal::executePythonCommand(command);
}
