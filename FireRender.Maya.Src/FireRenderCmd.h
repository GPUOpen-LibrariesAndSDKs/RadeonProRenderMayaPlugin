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

#include <memory>

#include <maya/MPxCommand.h>
#include <maya/MSyntax.h>
#include <maya/MArgDatabase.h>
#include <maya/MDagPath.h>
#include <maya/MCommonRenderSettingsData.h>
#include "FireRenderAOVs.h"
#include "RenderRegion.h"
#include "FireRenderIpr.h"
#include "FireRenderProduction.h"
#include "RenderCacheWarningDialog.h"

/** Perform a single frame, IPR, or batch render. */
class FireRenderCmd : public MPxCommand
{
public:

	// MPxCommand Implementation
	// -----------------------------------------------------------------------------

	/**
	 * Perform a render command to either start or
	 * communicate with a frame, IPR or batch render.
	 */
	MStatus doIt(const MArgList& args) override;

	/** Used by Maya to create the command instance. */
	static void* creator();

	/** Return the command syntax object. */
	static MSyntax newSyntax();


	// Static Methods
	// -----------------------------------------------------------------------------

	/** Clean up before plug-in shutdown. */
	static void cleanUp();


private:

	// Static Members
	// -----------------------------------------------------------------------------

	/**
	 * Set to true when a single frame render has started,
	 * and used as guard to prevent multiple simultaneous
	 * renders. This can occur if the user clicks the
	 * render frame button quickly enough.
	 */
	static bool s_rendering;

	/**
	 * Set in case we need to wait for single frame render to be finish,
	 * and we still want to use the built in render function call hierarchy
	 * (for ep.: renderIntoNewWindow)
	 * so wait for it flag and actual rendering is in two separate calls.
	 */
	static bool s_waitForIt;

	/** The current IPR render if any, otherwise nullptr. */
	static std::unique_ptr<FireRenderIpr> s_ipr;
	static std::unique_ptr<FireRenderProduction> s_production;

	/** Warning Dialog for Shader Caching. */
	RenderCacheWarningDialog rcWarningDialog;

	// Render Methods
	// -----------------------------------------------------------------------------

	/** Start or communicate with a single frame render session. */
	MStatus renderFrame(const MArgDatabase& argData);

	/** Start or communicate with an IPR render session. */
	MStatus renderIpr(const MArgDatabase& argData);

	/** Start or communicate with a batch render session. */
	MStatus renderBatch(const MArgDatabase& argData);


	// Private Methods
	// -----------------------------------------------------------------------------

	/** Stop an IPR render. */
	void stopIpr();

	/** Update the whether debug output is enabled or disabled. */
	MStatus updateDebugOutput(const MArgDatabase& argData);

	/** Enables or disables gltf export */
	MStatus exportsGLTF(const MArgDatabase& argData);

	/** Get the output file path, with an optional frame for multi-frame renders. */
	MString getOutputFilePath(const MCommonRenderSettingsData& settings,
		 int frame, const MString& camera, bool preview) const;

	/** Return true if the specified file already exists. */
	bool outputFileExists(const MString& filePath) const;

	/** Populate a render region if specified for the current render. */
	MStatus getRenderRegion(RenderRegion& region, bool& isRegion) const;

	/** Get the current dimensions specified in render arguments. */
	MStatus getDimensions(const MArgDatabase& argData, unsigned int& width, unsigned int& height) const;

	/** Get the DAG path for the camera name specified in render arguments. */
	MStatus getCameraPath(const MArgDatabase& argData, MDagPath& cameraPath) const;

	/** Switch to the render layer specified for the current render if necessary. */
	MStatus switchRenderLayer(const MArgDatabase& argData, MString& oldLayerName, MString& newLayerName);

	/** Restore the render layer that was active before the render started. */
	MStatus restoreRenderLayer(const MString& oldLayerName, const MString& newLayerName);

	/** Get the name of the given camera. */
	MString getCameraName(const MDagPath& cameraPath) const;

	/**
	 * Initialize a command port so the
	 * batch process can communicate with Maya.
	 */
	void initializeCommandPort(int port);

	/** Send a progress message to the Maya script console. */
	void sendBatchProgressMessage(int progress, int frame, const MString& layer);

};

// Command arguments.
#define kCameraFlag "-cam"
#define kCameraFlagLong "-camera"
#define kRenderLayerFlag "-l"
#define kRenderLayerFlagLong "-layer"
#define kWidthFlag "-w"
#define kWidthFlagLong "-width"
#define kHeightFlag "-h"
#define kHeightFlagLong "-height"
#define kIprFlag "-ipr"
#define kIprFlagLong "-iprRender"
#define kPauseFlag "-p"
#define kPauseFlagLong "-pause"
#define kStartFlag "-s"
#define kStartFlagLong "-start"
#define kStopFlag "-st"
#define kStopFlagLong "-stop"
#define kRegionFlag "-re"
#define kRegionFlagLong "-region"
#define kIsRunningFlag "-ir"
#define kIsRunningFlagLong "-isRunning"
#define kProductionIsRunningFlag "-ifr"
#define kProductionIsRunningFlagLong "-isProductionRunning"
#define kBatchFlag "-b"
#define kBatchFlagLong "-batch"
#define kFilenameFlag "-f"
#define kFilenameFlagLong "-filename"
#define kDebugTraceFlag "-d"
#define kDebugTraceFlagLong "-debug"
#define kUpdateRenderViewFlag "-u"
#define kUpdateRenderViewFlagLong "-updateRenderView"
#define kOpenFolder "-of"
#define kOpenFolderLong "-openFolder"
#define kWaitForIt "-wfi"
#define kWaitForItLong "-waitForIt"
#define kWaitForItTwoStep "-wft"
#define kWaitForItTwoStepLong "-waitForItTwo"
#define kExportsGLTF "-eg"
#define kExportsGLTFLong "-exportsGLTF"

