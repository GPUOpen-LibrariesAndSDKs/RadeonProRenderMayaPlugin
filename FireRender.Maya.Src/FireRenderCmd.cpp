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
#include "common.h"
#include "frWrap.h"
#include "FireRenderCmd.h"
#include "FireRenderIpr.h"
#include "FireRenderUtils.h"
#include <maya/MArgDatabase.h>
#include <maya/MRenderView.h>
#include <maya/MItDag.h>
#include <maya/MDagPath.h>
#include <maya/MFnDagNode.h>
#include <maya/MGlobal.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MPlug.h>
#include <maya/MFnRenderLayer.h>
#include <maya/MDagPathArray.h>
#include <maya/MSelectionList.h>
#include <maya/MObjectArray.h>
#include <maya/MPlugArray.h>
#include <maya/MArgList.h>
#include <maya/MAnimControl.h>
#include <maya/MFileIO.h>
#include <maya/MRenderUtil.h>
#include <maya/MCommonSystemUtils.h>

#include <iomanip>
#include <regex>

#include <maya/MIOStream.h>
#include <maya/MFileObject.h>

#include <maya/MProgressWindow.h>
#include "RenderProgressBars.h"
#include "RenderRegion.h"
#include "FireRenderThread.h"
#include "RenderStampUtils.h"

#include "Context/ContextCreator.h"

#include "Context/ContextCreator.h"

#include <imageio.h>

using namespace std;
using namespace OIIO;
using namespace FireMaya;

// Static Initialization
// -----------------------------------------------------------------------------
bool FireRenderCmd::s_rendering = false;
bool FireRenderCmd::s_waitForIt = false;
unique_ptr<FireRenderIpr> FireRenderCmd::s_ipr;
unique_ptr<FireRenderProduction> FireRenderCmd::s_production = make_unique<FireRenderProduction>();


// MPxCommand Implementation
// -----------------------------------------------------------------------------
void* FireRenderCmd::creator()
{
	return new FireRenderCmd;
}

// -----------------------------------------------------------------------------
MSyntax FireRenderCmd::newSyntax()
{
	MSyntax syntax;

	CHECK_MSTATUS(syntax.addFlag(kCameraFlag, kCameraFlagLong, MSyntax::kString));
	CHECK_MSTATUS(syntax.addFlag(kRenderLayerFlag, kRenderLayerFlagLong, MSyntax::kString));
	CHECK_MSTATUS(syntax.addFlag(kWidthFlag, kWidthFlagLong, MSyntax::kLong));
	CHECK_MSTATUS(syntax.addFlag(kHeightFlag, kHeightFlagLong, MSyntax::kLong));
	CHECK_MSTATUS(syntax.addFlag(kIprFlag, kIprFlagLong, MSyntax::kNoArg));
	CHECK_MSTATUS(syntax.addFlag(kPauseFlag, kPauseFlagLong, MSyntax::kBoolean));
	CHECK_MSTATUS(syntax.addFlag(kStartFlag, kStartFlagLong, MSyntax::kNoArg));
	CHECK_MSTATUS(syntax.addFlag(kStopFlag, kStopFlagLong, MSyntax::kNoArg));
	CHECK_MSTATUS(syntax.addFlag(kRegionFlag, kRegionFlagLong, MSyntax::kNoArg));
	CHECK_MSTATUS(syntax.addFlag(kIsRunningFlag, kIsRunningFlagLong, MSyntax::kNoArg));
	CHECK_MSTATUS(syntax.addFlag(kProductionIsRunningFlag, kProductionIsRunningFlagLong, MSyntax::kNoArg));
	CHECK_MSTATUS(syntax.addFlag(kBatchFlag, kBatchFlagLong, MSyntax::kNoArg));
	CHECK_MSTATUS(syntax.addFlag(kFilenameFlag, kFilenameFlagLong, MSyntax::kString));
	CHECK_MSTATUS(syntax.addFlag(kDebugTraceFlag, kDebugTraceFlagLong, MSyntax::kBoolean));
	CHECK_MSTATUS(syntax.addFlag(kUpdateRenderViewFlag, kUpdateRenderViewFlagLong, MSyntax::kNoArg));
	CHECK_MSTATUS(syntax.addFlag(kOpenFolder, kOpenFolderLong, MSyntax::kString));
	CHECK_MSTATUS(syntax.addFlag(kWaitForIt, kWaitForItLong, MSyntax::kNoArg));
	CHECK_MSTATUS(syntax.addFlag(kWaitForItTwoStep, kWaitForItTwoStepLong, MSyntax::kNoArg));
	CHECK_MSTATUS(syntax.addFlag(kExportsGLTF, kExportsGLTFLong, MSyntax::kBoolean));

	return syntax;
}

// -----------------------------------------------------------------------------
MStatus FireRenderCmd::doIt(const MArgList & args)
{
	// Parse arguments.
	MArgDatabase argData(syntax(), args);

	// Enable or disable debugging.
	if (argData.isFlagSet(kDebugTraceFlag))
	{
		return updateDebugOutput(argData);
	}
	else if (argData.isFlagSet(kExportsGLTF))
	{
		return exportsGLTF(argData);
	}
	else if (argData.isFlagSet(kOpenFolder))
	{
		MString path;
		argData.getFlagArgument(kOpenFolder, 0, path);
		if (path == "RPR_MAYA_TRACE_PATH")
			path = getLogFolder();

#ifdef WIN32
		path = std::regex_replace(path.asUTF8(), std::regex("\\\\"), "/").c_str();
#endif

		return MGlobal::executeCommand("launch -dir \""_ms + path + "\""_ms);
	}
	else
	{
		try
		{
			// Start or update an IPR render.
			if (argData.isFlagSet(kIprFlag))
				return renderIpr(argData);
			// Start or update a batch render.
			else if (argData.isFlagSet(kBatchFlag))
				return renderBatch(argData);
			// Start a single frame render.
			else
				return renderFrame(argData);
		}
		catch (std::exception& err)
		{
			MGlobal::displayError(MString("std::exception: ") + err.what());
			s_rendering = false;
		}
		catch (...)
		{
			MGlobal::displayError(MString("Exception in FireRenderCmd !!!"));
			s_rendering = false;
		}
	}

	return MStatus::kSuccess;
}

// Static Methods
// -----------------------------------------------------------------------------
void FireRenderCmd::cleanUp()
{
	if (s_ipr)
		s_ipr->stop();
	s_ipr.reset();

	if (s_production)
		s_production->stop();
	s_production.reset();
}


// Render Methods
// -----------------------------------------------------------------------------
MStatus FireRenderCmd::renderFrame(const MArgDatabase& argData)
{
	// Query production is running
	if (argData.isFlagSet(kProductionIsRunningFlag))
	{
		setResult(s_production != nullptr && s_production->isRunning());
		return MStatus::kSuccess;
	}

	// Only allow one render to be active at a time.
	if (s_rendering)
		return MS::kSuccess;

	if (argData.isFlagSet(kWaitForItTwoStep) || argData.isFlagSet(kWaitForItTwoStepLong)) {
		s_waitForIt = true;
		return MS::kSuccess;
	}

	// Get frame buffer dimensions.
	unsigned int width = 0;
	unsigned int height = 0;

	MStatus status = getDimensions(argData, width, height);
	if (status != MS::kSuccess)
		return status;

	// Get the camera to use for this render.
	MDagPath cameraPath;
	status = getCameraPath(argData, cameraPath);
	if (status != MS::kSuccess)
		return status;

	// Get the render region. Default to the full frame size.
	bool isRegion = false;
	RenderRegion region(0, width - 1, height - 1, 0);
	status = getRenderRegion(region, isRegion);
	if (status != MS::kSuccess)
		return status;

	// Render layer names.
	MString oldLayerName;
	MString newLayerName;
	// Switch to the correct render layer if necessary.
	// Record the render layer names so they can be
	// restored once the render is complete.
	switchRenderLayer(argData, oldLayerName, newLayerName);

	// Start production render on a new thread.
	s_production->setCamera(cameraPath);
	s_production->setResolution(width, height);
	s_production->setRenderRegion(region);
	s_production->updateRegion();

	s_production->setStopCallback([=](FireRenderAOVs* aovs, MCommonRenderSettingsData & settings)
	{
		// Get the full path to the output image
		// file and create folders if necessary.
		MString cameraName = getCameraName(cameraPath);
		unsigned int frame = static_cast<unsigned int>(MAnimControl::currentTime().value());
		MString filePath = getOutputFilePath(settings, frame, cameraName, true);

		// Write output files.
		aovs->writeToFile(*s_production->GetContext(), filePath, settings.imageFormat, [](const MString& path)
		{
			MString cmd;

			// this command will output the following string: "\t[path]\n" to be executed via MEL
			cmd.format("print(\"\\t^1s\\n\")", path);
			MGlobal::executeCommand(cmd);
		});

		// Perform clean up operations.
		MRenderView::endRender();

		restoreRenderLayer(oldLayerName, newLayerName);
		s_rendering = false;
	});

	s_rendering = true;

	s_production->UpdateGlobals();
	if (s_production->isTileRender())
	{
		s_production->startTileRender();
	}
	else
	{
		s_production->startFullFrameRender();
	}

	if (s_waitForIt || argData.isFlagSet(kWaitForIt))
		s_production->waitForIt();

	s_waitForIt = false;

	// For sequence render we need to return non success to abort sequence render
	return !s_production->isCancelled() ? MS::kSuccess : MS::kFailure;
}

// -----------------------------------------------------------------------------
MStatus FireRenderCmd::renderIpr(const MArgDatabase& args)
{
	try
	{
		// Start an IPR render.
		if (args.isFlagSet(kStartFlag) && !s_ipr)
		{
			if (s_production != nullptr)
			{
				s_production->stop();
			}

			// Request from Tahoe team to temporary disable IPR for RPR 2
			if (!CheckIsInteractivePossible())
			{
				MGlobal::displayError("IPR is currently disabled for RPR 2!");
				return MS::kFailure;
			}

			// Get frame buffer dimensions.
			unsigned int width = 0;
			unsigned int height = 0;

			MStatus status = getDimensions(args, width, height);
			if (status != MS::kSuccess)
				return status;

			// Get the camera to use for this render.
			MDagPath cameraPath;
			status = getCameraPath(args, cameraPath);
			if (status != MS::kSuccess)
				return status;

			// Start the IPR render on a new thread.
			s_ipr = make_unique<FireRenderIpr>();
			s_ipr->setCamera(cameraPath);
			s_ipr->setResolution(width, height);
			s_ipr->updateRegion();
			if (!s_ipr->start())
			{
				setResult(false);
				return MStatus::kFailure;
			}
		}

		// Stop an active IPR render.
		if (args.isFlagSet(kStopFlag) && s_ipr)
		{
			stopIpr();
		}

		// Pause an active IPR render.
		if (args.isFlagSet(kPauseFlag) && s_ipr)
		{
			bool pause = false;
			args.getFlagArgument(kPauseFlag, 0, pause);
			s_ipr->pause(pause);
		}

		// Set a result of true if there is an active IPR render.
		if (args.isFlagSet(kIsRunningFlag))
		{
			setResult(s_ipr && s_ipr->isRunning());

			// Check if the IPR render is in an error state.
			if (s_ipr && s_ipr->isError())
			{
				stopIpr();
				return MStatus::kFailure;
			}

			return MStatus::kSuccess;
		}

		// Update the render region.
		if (args.isFlagSet(kRegionFlag) && s_ipr)
		{
			s_ipr->updateRegion();
		}

		// Update the view. Performs a render iteration.
		if (args.isFlagSet(kUpdateRenderViewFlag) && s_ipr)
		{
			// Check if the IPR render is in an error state.
			if (s_ipr && s_ipr->isError())
			{
				stopIpr();
				return MStatus::kFailure;
			}

			s_ipr->updateRenderView();
			return MStatus::kSuccess;
		}
	}
	catch (...)
	{
		// Clean up.
		stopIpr();

		// Process the error.
		FireRenderError error(std::current_exception());
		return MStatus::kFailure;
	}

	// Success.
	return MStatus::kSuccess;
}

// -----------------------------------------------------------------------------
MStatus FireRenderCmd::renderBatch(const MArgDatabase& args)
{
	auto previousUseThreadValue = FireRenderThread::UseTheThread(false);	// No need to use separate thread for batch render

	// The render context.

	NorthStarContextPtr northStarContextPtr = ContextCreator::CreateNorthStarContext();

	NorthStarContext& context = *northStarContextPtr;

	try
	{
		// Read common render settings.
		MCommonRenderSettingsData settings;
		MRenderUtil::getCommonRenderSettings(settings);

		// Read RPR globals.
		FireRenderGlobalsData globals;
		globals.readFromCurrentScene();

		// Switch to the correct render layer if necessary.
		// Record the render layer names so they can be
		// restored once the render is complete.
		MString oldLayerName;
		MString newLayerName;
		switchRenderLayer(args, oldLayerName, newLayerName);

		FireRenderAOVs& aovs = globals.aovs;

		// Enable active AOVs.

		aovs.applyToContext(context);

		// Initialize the scene.
		context.SetRenderType(RenderType::ProductionRender);
		context.buildScene();
		context.updateLimitsFromGlobalData(globals, false, true);
		context.setResolution(settings.width, settings.height, true);

		// Initialize the command port so the
		// batch process can communicate with Maya.
		initializeCommandPort(globals.commandPort);

		// Allocate AOV pixels.
		RenderRegion region(0, settings.width - 1, settings.height - 1, 0);
		aovs.setRegion(region, settings.width, settings.height);
		aovs.allocatePixels();

		// Setup render stamp
		if (globals.useRenderStamp)
		{
			MString renderStamp = globals.renderStampText;
			aovs.setRenderStamp(renderStamp);
		}

		// Get selected devices
		int renderDevice = RenderStampUtils::GetRenderDevice();
		std::string devicesStr("\ndevice selected: ");
		if (renderDevice == RenderStampUtils::RPR_RENDERDEVICE_CPUONLY)
		{
			devicesStr += RenderStampUtils::GetCPUNameString();
		}
		else if (renderDevice == RenderStampUtils::RPR_RENDERDEVICE_GPUONLY)
		{
			devicesStr += RenderStampUtils::GetFriendlyUsedGPUName();
		}
		else
		{
			devicesStr += std::string(RenderStampUtils::GetCPUNameString()) + " / " + RenderStampUtils::GetFriendlyUsedGPUName();
		}
		devicesStr += "\n";

		// Get the list of cameras to render frames for.
		MDagPathArray renderableCameras = GetSceneCameras(true);

		// Process each render-able camera.
		for (MDagPath camera : renderableCameras)
		{
			// Update the context to use the current camera.
			MString cameraName = getCameraName(camera);
			context.setCamera(camera, true);

			// Get frame ranges.
			int frameStart = static_cast<int>(settings.frameStart.value());
			int frameEnd = static_cast<int>(settings.frameEnd.value());
			int frameBy = static_cast<int>(settings.frameBy);

			// Don't render multiple frames for single frame renders.
			if (settings.namingScheme < 2)
				frameEnd = frameStart;

			// Process each frame.
			for (int frame = frameStart; frame <= frameEnd; frame += frameBy)
			{
				// Execute the pre-frame command if there is one.
				MGlobal::executeCommand(settings.preRenderMel);

				// Move the animation to the next frame.
				MTime time;
				time.setValue(static_cast<double>(frame));
				MAnimControl::setCurrentTime(time);

				// Get the full path to the output image
				// file and create folders if necessary.
				MString filePath = getOutputFilePath(settings, frame, cameraName, false);

				// Skip the current frame if required.
				if (settings.skipExistingFrames && outputFileExists(filePath))
					continue;

				// Refresh the context so it matches the
				// current animation state and start the render.
				context.Freshen();
				context.setStartedRendering();

				// Track the last progress percent so a progress
				// message is displayed only if the progress changes.
				int lastProgress = 0;

				// Render a frame until completion criteria are met.
				while (context.keepRenderRunning())
				{
					// Render the frame and update progress.
					context.render();
					context.updateProgress();

					// Send a message to Maya if progress has changed.
					int progress = context.getProgress();
					if (progress > lastProgress)
					{
						sendBatchProgressMessage(progress, frame, newLayerName);
						lastProgress = progress;
					}
				}

				// Resolve the frame buffer and read pixels into AOVs.
				aovs.readFrameBuffers(context);

				// Run denoiser
				if (context.IsDenoiserEnabled())
				{
					FireRenderAOV* pColorAOV = aovs.getAOV(RPR_AOV_COLOR);
					assert(pColorAOV != nullptr);

					context.ProcessDenoise(aovs.getRenderViewAOV(), *pColorAOV, context.m_width, context.m_height, region, [this](RV_PIXEL* data) {});
				}

				// Save the frame to file.
				aovs.writeToFile(context, filePath, settings.imageFormat);

				// Execute the post frame command if there is one.
				MGlobal::executeCommand(settings.postRenderMel);
			}
		}

		MGlobal::displayInfo(MString(devicesStr.c_str()));

		// Perform clean up operations.
		context.cleanScene();
	}
	catch (...)
	{
		// Perform clean up operations.
		context.cleanScene();

		// Process the error.
		FireRenderError error(std::current_exception(), false);

		FireRenderThread::UseTheThread(previousUseThreadValue);
		return MStatus::kFailure;
	}

	FireRenderThread::UseTheThread(previousUseThreadValue);
	// Batch render completed successfully.
	return MS::kSuccess;
}


// Private Methods
// -----------------------------------------------------------------------------
void FireRenderCmd::stopIpr()
{
	s_ipr->stop();
	s_ipr.reset();
}

// -----------------------------------------------------------------------------
MStatus FireRenderCmd::updateDebugOutput(const MArgDatabase& argData)
{
	bool traceFlag = false;
	MStatus status = argData.getFlagArgument(kDebugTraceFlag, 0, traceFlag);
	if (status == MS::kSuccess)
		frw::Context::TraceOutput(traceFlag ? getLogFolder().asUTF8() : nullptr);

	return status;
}

// Implemented in pluginMain.cpp
void RprExportsGLTF(bool enable);

MStatus FireRenderCmd::exportsGLTF(const MArgDatabase& argData)
{
	bool enableFlag = false;
	MStatus status = argData.getFlagArgument(kExportsGLTF, 0, enableFlag);
	if (status == MS::kSuccess)
	{
		RprExportsGLTF(enableFlag);
	}

	return status;
}

// -----------------------------------------------------------------------------
MString FireRenderCmd::getOutputFilePath(const MCommonRenderSettingsData& settings,
	 int frame, const MString& camera, bool preview) const
{
	// The base file name will be populated in the settings if the
	// user has specified a custom file name, otherwise, it will be empty.
	MString sceneName = settings.name;

	// Populate the scene name with the current namespace - this
	// should be equivalent to the scene name and is how Maya generates
	// it for post render operations such as layered PSD creation.
	if (sceneName.length() <= 0)
	{
		MStringArray results;
		MGlobal::executeCommand("file -q -ns", results);

		if (results.length() > 0)
			sceneName = results[0];
	}

	// Use settings to generate the image file name
	// if the scene name was successfully populated.
	if (sceneName.length() > 0)
	{
		// Use the temp folder for preview renders.
		// This method also ensures folders exist.
		auto pathType = preview ? settings.kFullPathTmp : settings.kFullPathImage;
		return settings.getImageName(pathType, frame,
			sceneName, camera, "", MFnRenderLayer::currentLayer());
	}

	// Otherwise, query renderSettings. This is a fall-back method
	// in case the above method fails, but should not normally be executed.
	else
	{
		// Create a 0 padded frame string.
		MString path = "";
		std::stringstream frameStream;
		frameStream << std::setfill('0') << std::setw(settings.framePadding) << frame;

		// Create and execute the command. Use
		// the temp folder for preview renders.
		MString pathType = preview ? "fullPathTemp" : "fullPath";
		MString command = "renderSettings -" + pathType + " -genericFrameImageName \"";
		command += frameStream.str().c_str();
		command += "\" -camera \"" + camera + "\"";

		MStringArray results;
		MGlobal::executeCommand(command, results);

		// Get the result and ensure the path exists.
		if (results.length() > 0)
		{
			path = results[0];
			MCommonSystemUtils::makeDirectory(path.substring(0, path.rindex('/')));
		}

		return path;
	}
}

// -----------------------------------------------------------------------------
bool FireRenderCmd::outputFileExists(const MString& filePath) const
{
	MFileObject fileObj;
	fileObj.setRawFullName(filePath);
	return fileObj.exists();
}

// -----------------------------------------------------------------------------
MStatus FireRenderCmd::getDimensions(const MArgDatabase& argData, unsigned int& width, unsigned int& height) const
{
	// Get dimension arguments.
	if (argData.isFlagSet(kWidthFlag))
		argData.getFlagArgument(kWidthFlag, 0, width);

	if (argData.isFlagSet(kHeightFlag))
		argData.getFlagArgument(kHeightFlag, 0, height);

	// Check that the dimensions are valid.
	if (width <= 0 || height <= 0)
	{
		MGlobal::displayError("Invalid dimensions");
		return MS::kFailure;
	}

	// Success.
	return MS::kSuccess;
}

// -----------------------------------------------------------------------------
MStatus FireRenderCmd::getCameraPath(const MArgDatabase& argData, MDagPath& cameraPath) const
{
	// Get camera name argument.
	MString cameraName;
	if (argData.isFlagSet(kCameraFlag))
		argData.getFlagArgument(kCameraFlag, 0, cameraName);

	// Get the camera scene DAG path.
	MSelectionList sList;
	sList.add(cameraName);
	MStatus status = sList.getDagPath(0, cameraPath);
	if (status != MS::kSuccess)
	{
		MGlobal::displayError("Invalid camera");
		return MS::kFailure;
	}

	// Extend to include the camera shape.
	cameraPath.extendToShape();
	if (cameraPath.apiType() != MFn::kCamera)
	{
		MGlobal::displayError("Invalid camera");
		return MS::kFailure;
	}

	// Success.
	return MS::kSuccess;
}

// -----------------------------------------------------------------------------
MStatus FireRenderCmd::switchRenderLayer(const MArgDatabase& argData, MString& oldLayerName, MString& newLayerName)
{
	// Find the current render layer.
	MObject existingRenderLayer = MFnRenderLayer::currentLayer();
	MFnDependencyNode layerNodeFn(existingRenderLayer);
	oldLayerName = layerNodeFn.name();

	// Check if a different render layer is specified.
	newLayerName = oldLayerName;
	if (argData.isFlagSet(kRenderLayerFlag))
		argData.getFlagArgument(kRenderLayerFlag, 0, newLayerName);

	// Switch the render layer if required.
	if (newLayerName != oldLayerName)
		return MGlobal::executeCommand("editRenderLayerGlobals -currentRenderLayer " + newLayerName, false, true);

	// Success.
	return MS::kSuccess;
}

// -----------------------------------------------------------------------------
MStatus FireRenderCmd::restoreRenderLayer(const MString& oldLayerName, const MString& newLayerName)
{
	// Switch back to the layer that was active
	// before rendering started if necessary.
	if (oldLayerName != newLayerName)
		return MGlobal::executeCommand("editRenderLayerGlobals -currentRenderLayer " + oldLayerName, false, true);

	// Success.
	return MS::kSuccess;
}

// -----------------------------------------------------------------------------
MStatus FireRenderCmd::getRenderRegion(RenderRegion& region, bool& isRegion) const
{
	// Determine if a region has been specified.
	MSelectionList slist;
	slist.add("defaultRenderGlobals");
	MObject renGlobalsObj;
	slist.getDependNode(0, renGlobalsObj);
	MFnDependencyNode globalsNode(renGlobalsObj);

	MPlug regionPlug = globalsNode.findPlug("useRenderRegion");
	if (!regionPlug.isNull())
		regionPlug.getValue(isRegion);

	// Read region values if available.
	if (isRegion)
		return MRenderView::getRenderRegion(region.left, region.right, region.bottom, region.top);

	// Success.
	return MS::kSuccess;
}

// -----------------------------------------------------------------------------
MString FireRenderCmd::getCameraName(const MDagPath& cameraPath) const
{
	MDagPath path = cameraPath;
	path.extendToShape();
	MObject transform = path.transform();
	MFnDependencyNode transfNode(transform);
	return transfNode.name();
}

// -----------------------------------------------------------------------------
void FireRenderCmd::initializeCommandPort(int port)
{
	MString portString = std::to_string(port).c_str();

	MString commandPortFunction =
		"import socket\n"
		"import sys\n"
		"HOST = '127.0.0.1'\n"
		"PORT = " + portString + "\n"
		"ADDR = (HOST, PORT)\n"
		"def SendCommand(message):\n"
		"\tif PORT == 0:\n"
		"\t\treturn\n"
		"\tclient = socket.socket(socket.AF_INET, socket.SOCK_STREAM)\n"
		"\tclient.connect(ADDR)\n"
		"\tif sys.version_info[0] < 3:\n"
		"\t\tclient.send(message)\n"
		"\telse:\n"
		"\t\tclient.send(str.encode(message))\n"
		"\tclient.close()\n";

	MGlobal::executePythonCommand(commandPortFunction);
}

// -----------------------------------------------------------------------------
void FireRenderCmd::sendBatchProgressMessage(int progress, int frame, const MString& layer)
{
	// Create the output message.
	std::stringstream message;
	message << "// Percentage of rendering done: " << progress <<
		" (frame " << frame << ", layer " << layer.asChar() << " //";

	// Send the message to Maya and display locally.
	MGlobal::executePythonCommand("SendCommand(\"print(\\\"" + MString(message.str().c_str()) + "\\\\n\\\");\")");
	MGlobal::displayInfo(message.str().c_str());
}
