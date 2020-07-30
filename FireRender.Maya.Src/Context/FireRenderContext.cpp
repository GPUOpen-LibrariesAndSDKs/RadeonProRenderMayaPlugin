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
#include <GL/glew.h>
#include <maya/M3dView.h>
#include "FireRenderContext.h"
#include "FireRenderUtils.h"
#include <cassert>
#include <float.h>
#include <maya/MDagPathArray.h>
#include <maya/MMatrix.h>
#include <maya/MRenderView.h>
#include <maya/MFnNurbsSurface.h>
#include <maya/MFnMeshData.h>
#include <maya/MFnMesh.h>
#include <maya/MFnLight.h>
#include <maya/MSelectionList.h>
#include <maya/MImage.h>
#include <maya/MItDag.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnDagNode.h>
#include <maya/MPlug.h>
#include <maya/MItDependencyNodes.h>
#include <maya/MUserEventMessage.h>
#include <maya/MItDependencyGraph.h>

#include "AutoLock.h"
#include "VRay.h"
#include "ImageFilter/ImageFilter.h"

#include "RprComposite.h"
#include <iostream>
#include <fstream>

#include "FireRenderThread.h"
#include "FireRenderMaterialSwatchRender.h"
#include "CompositeWrapper.h"

#ifdef OPTIMIZATION_CLOCK
	#include <chrono>
#endif

using namespace RPR;
using namespace FireMaya;

#ifdef DEBUG_LOCKS
std::map<FireRenderContext*,FireRenderContext::Lock::frcinfo> FireRenderContext::Lock::lockMap;
#endif

#ifdef MAYA2015
#undef min
#undef max
#endif

#include <imageio.h>

#ifndef MAYA2015
#include <maya/MUuid.h>
#endif
#include "FireRenderIBL.h"
#include <maya/MFnRenderLayer.h>

#ifdef OPTIMIZATION_CLOCK
	int FireRenderContext::timeInInnerAddPolygon;
	int FireRenderContext::overallAddPolygon;
	int FireRenderContext::overallCreateMeshEx;
	unsigned long long FireRenderContext::timeGetDataFromMaya;
	unsigned long long FireRenderContext::translateData;
	int FireRenderContext::inTranslateMesh;
	int FireRenderContext::inGetFaceMaterials;
	int FireRenderContext::getTessellatedObj;
	int FireRenderContext::deleteNodes;
#endif

int FireRenderContext::INCORRECT_PLUGIN_ID = -1;

FireRenderContext::FireRenderContext() :
	m_width(0),
	m_height(0),
	m_useRegion(false),
	m_motionBlur(false),
	m_cameraMotionBlur(false),
	m_viewportMotionBlur(false),
	m_motionBlurCameraExposure(0.0f),
	m_cameraAttributeChanged(false),
	m_renderStartTime(0),
	m_samplesPerUpdate(1),
	m_secondsSpentOnLastRender(0.0),
	m_lastRenderResultState(NOT_SET),
	m_polycountLastRender(0),
	m_currentIteration(0),
	m_currentFrame(0),
	m_progress(0),
	m_interactive(false),
	m_camera(this, MDagPath()),
	m_glInteropActive(false),
	m_globalsChanged(false),
	m_renderLayersChanged(false),
	m_cameraDirty(true),
	m_denoiserChanged(false),
	m_denoiserFilter(nullptr),
	m_shadowColor{ 0.0f, 0.0f, 0.0f },
	m_bgColor{ 1.0f, 1.0f, 1.0f },
	m_shadowTransparency(0),
	m_backgroundTransparency(0),
	m_shadowWeight(1),
	m_bgWeight(1),
	m_RenderType(RenderType::Undefined),
	m_bIsGLTFExport(false)
{
	DebugPrint("FireRenderContext::FireRenderContext()");

	m_workStartTime = clock();
	m_state = StateUpdating;
	m_dirty = false;
}

FireRenderContext::~FireRenderContext()
{
	DebugPrint("FireRenderContext(context=%p)::~FireRenderContext()", this);
	// Unsubscribe from all callbacks.
	removeCallbacks();

	m_denoiserFilter.reset();
}

int FireRenderContext::initializeContext()
{
	DebugPrint("FireRenderContext::initializeContext()");

	RPR_THREAD_ONLY;

	LOCKMUTEX(this);

	auto createFlags = FireMaya::Options::GetContextDeviceFlags(m_RenderType);

	rpr_int res;
	createContextEtc(createFlags, true, false, &res);

	return res;
}

void FireRenderContext::resize(unsigned int w, unsigned int h, bool renderView, rpr_GLuint* glTexture)
{
	RPR_THREAD_ONLY;

	// Set the context resolution.
	setResolution(w, h, renderView, glTexture);
}

void FireRenderContext::setResolution(unsigned int w, unsigned int h, bool renderView, rpr_GLuint* glTexture)
{
	RPR_THREAD_ONLY;
	DebugPrint("FireRenderContext::setResolution(%d,%d)", w, h);
	m_width = w;
	m_height = h;
	m_isRenderView = renderView;

	auto context = scope.Context();

	if (!context)
		return;

	for (int i = 0; i != RPR_AOV_MAX; ++i)
	{
		initBuffersForAOV(context, i, glTexture);
	}
}

bool FireRenderContext::ConsiderSetupDenoiser(bool useRAMBufer /* = false*/)
{
	bool shouldDenoise = IsDenoiserSupported() &&
						(m_globals.denoiserSettings.enabled && 
						((m_RenderType == RenderType::ProductionRender) || 
						(m_RenderType == RenderType::IPR)));

	if (!shouldDenoise)
		return false;

	if (!useRAMBufer)
	{
		setupDenoiserFB();
		return true;
	}

	if (useRAMBufer && (m_pixelBuffers.size() != 0))
	{
		setupDenoiserRAM();
		return true;
	}

	return false;
}

void FireRenderContext::resetAOV(int index, rpr_GLuint* glTexture)
{
	auto context = scope.Context();

	initBuffersForAOV(context, index, glTexture);
}

void FireRenderContext::enableAOVAndReset(int index, bool flag, rpr_GLuint* glTexture)
{
	enableAOV(index, flag);
	resetAOV(index, flag ? glTexture : nullptr);
}

#if (RPR_VERSION_MINOR < 34)
std::set<int> aovsExcluded;
#else
std::set<int> aovsExcluded{ RPR_AOV_VIEW_SHADING_NORMAL, RPR_AOV_COLOR_RIGHT };
#endif

void FireRenderContext::initBuffersForAOV(frw::Context& context, int index, rpr_GLuint* glTexture)
{
	rpr_framebuffer_format fmt = { 4, RPR_COMPONENT_TYPE_FLOAT32 };

	m.framebufferAOV[index].Reset();
	m.framebufferAOV_resolved[index].Reset();

	auto it = aovsExcluded.find(index); // not all AOVs listed in RadeonProRender.h are supported by Tahoe
	if (it != aovsExcluded.end())
		return;

	if (aovEnabled[index]) 
    {
		m.framebufferAOV[index] = frw::FrameBuffer(context, m_width, m_height, fmt);
		m.framebufferAOV[index].Clear();
		context.SetAOV(m.framebufferAOV[index], index);

		// Create an OpenGL interop resolved frame buffer if
		// required, otherwise, create a standard frame buffer.
		if (m_glInteropActive && glTexture)
        { 
			m.framebufferAOV_resolved[index] = frw::FrameBuffer(context, glTexture);
        }
        else
        {
            m.framebufferAOV_resolved[index] = frw::FrameBuffer(context, m_width, m_height, fmt);
        }

		m.framebufferAOV_resolved[index].Clear();
	}
	else
	{
		if (IsAOVSupported(index))
		{
			context.SetAOV(nullptr, index);
		}
	}
}

void FireRenderContext::setCamera(MDagPath& cameraPath, bool useNonDefaultCameraType)
{
	MAIN_THREAD_ONLY;
	DebugPrint("FireRenderContext::setCamera(...)");
	m_camera.SetObject(cameraPath.node());

	if (useNonDefaultCameraType)
	{
		m_camera.setType(m_globals.cameraType);
	}
	else
		m_camera.setType(0);
}

void FireRenderContext::updateLimits(bool animation)
{
	// Update limits.
	updateLimitsFromGlobalData(m_globals, animation);
}

void FireRenderContext::updateLimitsFromGlobalData(const FireRenderGlobalsData & globalData, bool animation, bool batch)
{
	CompletionCriteriaParams params = isInteractive() ? globalData.completionCriteriaViewport : globalData.completionCriteriaFinalRender;

	// Set to a single iteration for animations.
	if (animation)
	{
		params.completionCriteriaMaxIterations = 1;
		params.makeInfiniteTime();
	}

	int iterationStep = 1;

	// Update completion criteria.
	setCompletionCriteria(params);
}

bool FireRenderContext::buildScene(bool isViewport, bool glViewport, bool freshen)
{
	MAIN_THREAD_ONLY;
	DebugPrint("FireRenderContext::buildScene()");

	m_globals.readFromCurrentScene();

	// Backdoor for enabling aovs in IPR/Viewport
	if (isInteractive())
	{
		EnableAOVsFromRSIfEnvVarSet(*this, m_globals.aovs);
	}

	if (m_globals.denoiserSettings.enabled)
	{
		turnOnAOVsForDenoiser();
	}

	auto createFlags = FireMaya::Options::GetContextDeviceFlags(m_RenderType);

#ifdef DBG_LOG_RENDER_TYPE
	{
		std::ofstream loggingFile;
		loggingFile.open("C:\\temp\\dbg\\render_flags_log.txt", std::ofstream::out | std::ofstream::app);
		loggingFile << "m_RenderType = " << (int)m_RenderType << "\n";
		loggingFile << "createFlags = " << createFlags << "\n\n";
		loggingFile.close();
	}
#endif

	{
		LOCKMUTEX(this);

		FireRenderThread::UseTheThread((createFlags & RPR_CREATION_FLAGS_ENABLE_CPU) != 0);

		bool avoidMaterialSystemDeletion_workaround = isViewport && (createFlags & RPR_CREATION_FLAGS_ENABLE_CPU);

		rpr_int res;
		if (!createContextEtc(createFlags, !avoidMaterialSystemDeletion_workaround, glViewport, &res))
		{
			// Failed to create context
			if (glViewport)
			{
				// If we attempted to create gl interop context, try again without interop
				if (!createContextEtc(createFlags, !avoidMaterialSystemDeletion_workaround, false))
				{
					// Failed again, aborting
					MGlobal::displayError("Failed to create Radeon ProRender context in interop and non-interop modes. Aborting.");
					return false;
				}
				else
				{
					// Success, display a message and continue
					MGlobal::displayWarning("Unable to create Radeon ProRender context in interop mode. Falling back to non-interop mode.");
				}
			}
			else
			{
				MGlobal::displayError("Aborting switching to Radeon ProRender.");
				MString msg;
				FireRenderError error(res, msg, true);

				return false;
			}
		}

		updateLimitsFromGlobalData(m_globals);
		setupContext(m_globals);

		setMotionBlurParameters(m_globals);

		// Update render selected objects only flag
		int isRenderSelectedOnly = 0;
		MGlobal::executeCommand("isRenderSelectedObjectsOnlyFlagSet()", isRenderSelectedOnly);
		m_renderSelectedObjectsOnly = isRenderSelectedOnly > 0;

		MStatus status;

		MItDag itDag(MItDag::kDepthFirst, MFn::kDagNode, &status);
		if (MStatus::kSuccess != status)
			MGlobal::displayError("MItDag::MItDag");

		for (; !itDag.isDone(); itDag.next())
		{
			MDagPath dagPath;
			status = itDag.getPath(dagPath);

			if (MStatus::kSuccess != status)
			{
				MGlobal::displayError("MDagPath::getPath");
				break;
			}

			AddSceneObject(dagPath);
		}

		//Should be called when all scene objects are constucted
		BuildLateinitObjects();

		attachCallbacks();
	}

	if(freshen)
		Freshen(true, [] { return false; });

	return true;
}

void FireRenderContext::turnOnAOVsForDenoiser(bool allocBuffer)
{
	static const std::vector<int> aovsToAdd = { RPR_AOV_SHADING_NORMAL, RPR_AOV_WORLD_COORDINATE,
		RPR_AOV_OBJECT_ID, RPR_AOV_DEPTH, RPR_AOV_DIFFUSE_ALBEDO };

	// Turn on necessary AOVs
	for (const int aov : aovsToAdd)
	{
		if (!isAOVEnabled(aov))
		{
			enableAOV(aov);

			if (allocBuffer)
			{
                auto ctx = scope.Context();
				initBuffersForAOV(ctx, aov);
			}
		}
	}
}

#if defined(LINUX)
bool FireRenderContext::CanCreateAiDenoiser() const
{
	return false;
}

#else

bool FireRenderContext::CanCreateAiDenoiser() const
{
	bool canCreateAiDenoiser = false;

	std::string gpuName;

	MIntArray devicesUsing;
	MGlobal::executeCommand("optionVar -q RPR_DevicesSelected", devicesUsing);

	auto allDevices = HardwareResources::GetAllDevices();
	size_t numDevices = std::min<size_t>(devicesUsing.length(), allDevices.size());

	for (int i = 0; i < numDevices; i++)
	{
		const HardwareResources::Device& gpuInfo = allDevices[i];
		
		if (devicesUsing[i])
		{
			gpuName = gpuInfo.name;
			canCreateAiDenoiser = true;
		}
	}

	return canCreateAiDenoiser;
}
#endif

void FireRenderContext::setupDenoiserRAM()
{
	bool canCreateAiDenoiser = CanCreateAiDenoiser();
	bool useOpenImageDenoise = !canCreateAiDenoiser;

	try
	{
		MString path;
		MStatus s = MGlobal::executeCommand("getModulePath -moduleName RadeonProRender", path);
		MString mlModelsFolder = path + "/data/models";

		m_denoiserFilter = std::shared_ptr<ImageFilter>(new ImageFilter(
			context(), 
			(uint32_t)m_pixelBuffers[RPR_AOV_COLOR].width(),
			(uint32_t)m_pixelBuffers[RPR_AOV_COLOR].height(),
			mlModelsFolder.asChar()
		));

		RifParam p;

		switch (m_globals.denoiserSettings.type)
		{
		case FireRenderGlobals::kBilateral:
			m_denoiserFilter->CreateFilter(RifFilterType::BilateralDenoise);
			m_denoiserFilter->AddInput(RifColor, m_pixelBuffers[RPR_AOV_COLOR].data(), m_pixelBuffers[RPR_AOV_COLOR].size(), 0.3f);
			m_denoiserFilter->AddInput(RifNormal, m_pixelBuffers[RPR_AOV_SHADING_NORMAL].data(), m_pixelBuffers[RPR_AOV_SHADING_NORMAL].size(), 0.01f);
			m_denoiserFilter->AddInput(RifWorldCoordinate, m_pixelBuffers[RPR_AOV_WORLD_COORDINATE].data(), m_pixelBuffers[RPR_AOV_WORLD_COORDINATE].size(), 0.01f);

			p = { RifParamType::RifInt, m_globals.denoiserSettings.radius };
			m_denoiserFilter->AddParam("radius", p);
			break;

		case FireRenderGlobals::kLWR:
			m_denoiserFilter->CreateFilter(RifFilterType::LwrDenoise);
			m_denoiserFilter->AddInput(RifColor, m_pixelBuffers[RPR_AOV_COLOR].data(), m_pixelBuffers[RPR_AOV_COLOR].size(), 0.1f);
			m_denoiserFilter->AddInput(RifNormal, m_pixelBuffers[RPR_AOV_SHADING_NORMAL].data(), m_pixelBuffers[RPR_AOV_SHADING_NORMAL].size(), 0.1f);
			m_denoiserFilter->AddInput(RifDepth, m_pixelBuffers[RPR_AOV_DEPTH].data(), m_pixelBuffers[RPR_AOV_DEPTH].size(), 0.1f);
			m_denoiserFilter->AddInput(RifWorldCoordinate, m_pixelBuffers[RPR_AOV_WORLD_COORDINATE].data(), m_pixelBuffers[RPR_AOV_WORLD_COORDINATE].size(), 0.1f);
			m_denoiserFilter->AddInput(RifObjectId, m_pixelBuffers[RPR_AOV_OBJECT_ID].data(), m_pixelBuffers[RPR_AOV_OBJECT_ID].size(), 0.1f);
			m_denoiserFilter->AddInput(RifTrans, m_pixelBuffers[RPR_AOV_OBJECT_ID].data(), m_pixelBuffers[RPR_AOV_OBJECT_ID].size(), 0.1f);

			p = { RifParamType::RifInt, m_globals.denoiserSettings.samples };
			m_denoiserFilter->AddParam("samples", p);

			p = { RifParamType::RifInt,  m_globals.denoiserSettings.filterRadius };
			m_denoiserFilter->AddParam("halfWindow", p);

			p.mType = RifParamType::RifFloat;
			p.mData.f = m_globals.denoiserSettings.bandwidth;
			m_denoiserFilter->AddParam("bandwidth", p);
			break;

		case FireRenderGlobals::kEAW:
			m_denoiserFilter->CreateFilter(RifFilterType::EawDenoise);
			m_denoiserFilter->AddInput(RifColor, m_pixelBuffers[RPR_AOV_COLOR].data(), m_pixelBuffers[RPR_AOV_COLOR].size(), m_globals.denoiserSettings.color);
			m_denoiserFilter->AddInput(RifNormal, m_pixelBuffers[RPR_AOV_SHADING_NORMAL].data(), m_pixelBuffers[RPR_AOV_SHADING_NORMAL].size(), m_globals.denoiserSettings.normal);
			m_denoiserFilter->AddInput(RifDepth, m_pixelBuffers[RPR_AOV_DEPTH].data(), m_pixelBuffers[RPR_AOV_DEPTH].size(), m_globals.denoiserSettings.depth);
			m_denoiserFilter->AddInput(RifTrans, m_pixelBuffers[RPR_AOV_OBJECT_ID].data(), m_pixelBuffers[RPR_AOV_OBJECT_ID].size(), m_globals.denoiserSettings.trans);
			m_denoiserFilter->AddInput(RifWorldCoordinate, m_pixelBuffers[RPR_AOV_WORLD_COORDINATE].data(), m_pixelBuffers[RPR_AOV_WORLD_COORDINATE].size(), 0.1f);
			m_denoiserFilter->AddInput(RifObjectId, m_pixelBuffers[RPR_AOV_OBJECT_ID].data(), m_pixelBuffers[RPR_AOV_OBJECT_ID].size(), 0.1f);
			break;

		case FireRenderGlobals::kML:
		{
			RifFilterType ft = m_globals.denoiserSettings.colorOnly ? RifFilterType::MlDenoiseColorOnly : RifFilterType::MlDenoise;
			m_denoiserFilter->CreateFilter(ft, useOpenImageDenoise);
		}
		m_denoiserFilter->AddInput(RifColor, m_pixelBuffers[RPR_AOV_COLOR].data(), m_pixelBuffers[RPR_AOV_COLOR].size(), 0.0f);

		if (!m_globals.denoiserSettings.colorOnly)
		{
			m_denoiserFilter->AddInput(RifNormal, m_pixelBuffers[RPR_AOV_SHADING_NORMAL].data(), m_pixelBuffers[RPR_AOV_SHADING_NORMAL].size(), 0.0f);
			m_denoiserFilter->AddInput(RifDepth, m_pixelBuffers[RPR_AOV_DEPTH].data(), m_pixelBuffers[RPR_AOV_DEPTH].size(), 0.0f);
			m_denoiserFilter->AddInput(RifAlbedo, m_pixelBuffers[RPR_AOV_DIFFUSE_ALBEDO].data(), m_pixelBuffers[RPR_AOV_DIFFUSE_ALBEDO].size(), 0.0f);
		}

		break;

		default:
			assert(false);
		}

		m_denoiserFilter->AttachFilter();
	}
	catch (std::exception& e)
	{
		m_denoiserFilter.reset();
		ErrorPrint(e.what());
		MGlobal::displayError("RPR failed to setup denoiser, turning it off.");
	}
}

void FireRenderContext::setupDenoiserFB()
{
	const rpr_framebuffer fbColor = m.framebufferAOV_resolved[RPR_AOV_COLOR].Handle();
	const rpr_framebuffer fbShadingNormal = m.framebufferAOV_resolved[RPR_AOV_SHADING_NORMAL].Handle();
	const rpr_framebuffer fbDepth = m.framebufferAOV_resolved[RPR_AOV_DEPTH].Handle();
	const rpr_framebuffer fbWorldCoord = m.framebufferAOV_resolved[RPR_AOV_WORLD_COORDINATE].Handle();
	const rpr_framebuffer fbObjectId = m.framebufferAOV_resolved[RPR_AOV_OBJECT_ID].Handle();
	const rpr_framebuffer fbDiffuseAlbedo = m.framebufferAOV_resolved[RPR_AOV_DIFFUSE_ALBEDO].Handle();
	const rpr_framebuffer fbTrans = fbObjectId;

	bool canCreateAiDenoiser = CanCreateAiDenoiser();
	bool useOpenImageDenoise = !canCreateAiDenoiser;

	try
	{
		MString path;
		MStatus s = MGlobal::executeCommand("getModulePath -moduleName RadeonProRender", path);
		MString mlModelsFolder = path + "/data/models";

		m_denoiserFilter = std::shared_ptr<ImageFilter>(new ImageFilter(context(), m_width, m_height, mlModelsFolder.asChar()));

		RifParam p;

		switch (m_globals.denoiserSettings.type)
		{
		case FireRenderGlobals::kBilateral:
			m_denoiserFilter->CreateFilter(RifFilterType::BilateralDenoise);
			m_denoiserFilter->AddInput(RifColor, fbColor, 0.3f);
			m_denoiserFilter->AddInput(RifNormal, fbShadingNormal, 0.01f);
			m_denoiserFilter->AddInput(RifWorldCoordinate, fbWorldCoord, 0.01f);

			p = { RifParamType::RifInt, m_globals.denoiserSettings.radius };
			m_denoiserFilter->AddParam("radius", p);
			break;

		case FireRenderGlobals::kLWR:
			m_denoiserFilter->CreateFilter(RifFilterType::LwrDenoise);
			m_denoiserFilter->AddInput(RifColor, fbColor, 0.1f);
			m_denoiserFilter->AddInput(RifNormal, fbShadingNormal, 0.1f);
			m_denoiserFilter->AddInput(RifDepth, fbDepth, 0.1f);
			m_denoiserFilter->AddInput(RifWorldCoordinate, fbWorldCoord, 0.1f);
			m_denoiserFilter->AddInput(RifObjectId, fbObjectId, 0.1f);
			m_denoiserFilter->AddInput(RifTrans, fbTrans, 0.1f);

			p = { RifParamType::RifInt, m_globals.denoiserSettings.samples };
			m_denoiserFilter->AddParam("samples", p);

			p = { RifParamType::RifInt,  m_globals.denoiserSettings.filterRadius };
			m_denoiserFilter->AddParam("halfWindow", p);

			p.mType = RifParamType::RifFloat;
			p.mData.f = m_globals.denoiserSettings.bandwidth;
			m_denoiserFilter->AddParam("bandwidth", p);
			break;

		case FireRenderGlobals::kEAW:
			m_denoiserFilter->CreateFilter(RifFilterType::EawDenoise);
			m_denoiserFilter->AddInput(RifColor, fbColor, m_globals.denoiserSettings.color);
			m_denoiserFilter->AddInput(RifNormal, fbShadingNormal, m_globals.denoiserSettings.normal);
			m_denoiserFilter->AddInput(RifDepth, fbDepth, m_globals.denoiserSettings.depth);
			m_denoiserFilter->AddInput(RifTrans, fbTrans, m_globals.denoiserSettings.trans);
			m_denoiserFilter->AddInput(RifWorldCoordinate, fbWorldCoord, 0.1f);
			m_denoiserFilter->AddInput(RifObjectId, fbObjectId, 0.1f);
			break;

		case FireRenderGlobals::kML:
			{
				RifFilterType ft = m_globals.denoiserSettings.colorOnly ? RifFilterType::MlDenoiseColorOnly : RifFilterType::MlDenoise;
				m_denoiserFilter->CreateFilter(ft, useOpenImageDenoise);
			}
			m_denoiserFilter->AddInput(RifColor, fbColor, 0.0f);

			if (!m_globals.denoiserSettings.colorOnly)
			{
				m_denoiserFilter->AddInput(RifNormal, fbShadingNormal, 0.0f);
				m_denoiserFilter->AddInput(RifDepth, fbDepth, 0.0f);
				m_denoiserFilter->AddInput(RifAlbedo, fbDiffuseAlbedo, 0.0f);
			}

			break;

		default:
			assert(false);
		}

		m_denoiserFilter->AttachFilter();
	}
	catch (std::exception& e)
	{
		m_denoiserFilter.reset();
		ErrorPrint( e.what() );
		MGlobal::displayError("RPR failed to setup denoiser, turning it off.");
	}
}

void FireRenderContext::UpdateDefaultLights()
{
	MAIN_THREAD_ONLY;
	bool enabled = true;
	for (auto ob : m_sceneObjects)
	{
		if (auto node = dynamic_cast<FireRenderNode*>(ob.second.get()))
		{
			if (node->IsEmissive())
			{
				enabled = false;
				break;
			}
		}
	}

	if (enabled)
	{
		MObject renGlobalsObj;
		GetDefaultRenderGlobals(renGlobalsObj);
		MFnDependencyNode globalsNode(renGlobalsObj);
		MPlug plug = globalsNode.findPlug("enableDefaultLight");
		plug.getValue(enabled);
	}

	if (!enabled)	// switch off if there is one
	{
		if (m_defaultLight)
		{
			GetScene().Detach(m_defaultLight);
			m_defaultLight.Reset();
			setCameraAttributeChanged(true);
		}
	}
	else if (!m_camera.Object().isNull())
	{
		MFnDagNode dagNode(m_camera.Object());
		MDagPath dagPath;
		dagNode.getPath(dagPath);
		if (dagPath.isValid())
		{
			if (!m_defaultLight)
			{
				m_defaultLight = GetContext().CreateDirectionalLight();
				GetScene().Attach(m_defaultLight);
			}

			// build direction light matrix
			MVector vZ(-0.2, 0.2, 0.5);
			vZ.normalize();
			MVector vY = vZ ^ MVector::xAxis;
			vY.normalize();
			MVector vX = vY ^ vZ;
			vX.normalize();

			MMatrix mLightLocal;
			mLightLocal.setToIdentity();

			vX.get(mLightLocal[0]);
			vY.get(mLightLocal[1]);
			vZ.get(mLightLocal[2]);

			MMatrix mLight = mLightLocal * dagPath.inclusiveMatrix();

			rpr_float mfloats[4][4];
			mLight.get(mfloats);

			float scale = 2.5f; // for equal Maya's default light
			m_defaultLight.SetRadiantPower(scale, scale, scale);
			m_defaultLight.SetTransform(mfloats[0]);

			setCameraAttributeChanged(true);
		}
	}
}

void FireRenderContext::setRenderMode(RenderMode renderMode)
{
	RPR_THREAD_ONLY;
	DebugPrint("FireRenderContext::setRenderMode( %d )", renderMode);

	GetContext().SetParameter(RPR_CONTEXT_RENDER_MODE, renderMode);
	setDirty();
}

void FireRenderContext::setPreview()
{
	int preview = m_interactive || (m_RenderType == RenderType::Thumbnail); 
	GetContext().SetParameter(RPR_CONTEXT_PREVIEW, preview);
}

void FireRenderContext::cleanScene()
{
	FireRenderThread::RunOnceProcAndWait([this]()
	{
		DebugPrint("FireRenderContext::cleanScene()");
		LOCKMUTEX(this);
		removeCallbacks();

		// Remove shapes first (It is connected with issue in Hybrid. We should clean up all meshes first before deleting lights)
		FireRenderObjectMap::iterator it = m_sceneObjects.begin();

		while (it != m_sceneObjects.end())	
		{
			FireRenderMesh* fireRenderMesh = dynamic_cast<FireRenderMesh*> (it->second.get());
			FireRenderLight* fireRenderLight = dynamic_cast<FireRenderLight*> (it->second.get());

			if (fireRenderMesh != nullptr)
			{
				fireRenderMesh->setVisibility(false);
				it = m_sceneObjects.erase(it);
			}
			else if (fireRenderLight != nullptr && fireRenderLight->data().isAreaLight)
			{
				fireRenderLight->detachFromScene();
				it = m_sceneObjects.erase(it);
			}
			else
			{
				it++;
			}
		}

		m_sceneObjects.clear();

		m_camera.clear();
		m_defaultLight.Reset();
		m.Reset();
		m_denoiserFilter.reset();

		if (white_balance)
		{
			rprContextDetachPostEffect(context(), white_balance.Handle());
			white_balance.Reset();
		}
		if (simple_tonemap)
		{
			rprContextDetachPostEffect(context(), simple_tonemap.Handle());
			simple_tonemap.Reset();
		}
		if (tonemap)
		{
			rprContextDetachPostEffect(context(), tonemap.Handle());
			tonemap.Reset();
		}
		if (normalization)
		{
			rprContextDetachPostEffect(context(), normalization.Handle());
			normalization.Reset();
		}
		if (gamma_correction)
		{
			rprContextDetachPostEffect(context(), gamma_correction.Handle());
			gamma_correction.Reset();
		}

		for (auto& fb : m.framebufferAOV)
			fb.Reset();

		for (auto& fb : m.framebufferAOV_resolved)
			fb.Reset();

		scope.Reset();
	});
}

void FireRenderContext::cleanSceneAsync(std::shared_ptr<FireRenderContext> refToKeepAlive)
{
	m_cleanSceneFuture = std::async( [] (std::shared_ptr<FireRenderContext> refToKeepAlive)
		{ refToKeepAlive->cleanScene(); }, refToKeepAlive);
}

void FireRenderContext::initSwatchScene()
{
	DebugPrint("FireRenderContext::buildSwatchScene(...)");

	auto createFlags = FireMaya::Options::GetContextDeviceFlags();

	rpr_int res;
	if (!createContextEtc(createFlags, true, false, &res))
	{
		MString msg;
		FireRenderError errorToShow(res, msg, true);
		throw res;
	}

	enableAOV(RPR_AOV_COLOR);

	FireRenderMesh* mesh = new FireRenderMesh(this, MDagPath());
	mesh->buildSphere();
	mesh->setVisibility(true);
	m_sceneObjects["mesh"] = std::shared_ptr<FireRenderObject>(mesh);

	if (mesh && mesh->Elements().size() > 0)
	{
		if (auto shader = GetShader(MObject()))
			mesh->Element(0).shape.SetShader(shader);
	}

	m_camera.buildSwatchCamera();

	FireRenderLight *light = new FireRenderLight(this, MDagPath());
	light->buildSwatchLight();
	light->attachToScene();
	m_sceneObjects["light"] = std::shared_ptr<FireRenderObject>(light);

	m_globals.readFromCurrentScene();
	setupContext(m_globals);

	CompletionCriteriaParams completionParams;
	int iterations = FireRenderGlobalsData::getThumbnailIterCount();
	completionParams.completionCriteriaMaxIterations = iterations;
	completionParams.completionCriteriaMinIterations = iterations;

	// Time will be infinite. Left all "hours", "minutes" and "seconds" as 0.

	setSamplesPerUpdate(iterations);
	setCompletionCriteria(completionParams);

	setPreview();
}

long TimeDiff(time_t currTime, time_t startTime)
{
	return (long)((currTime - startTime) * 1000 / CLOCKS_PER_SEC);
}


void FireRenderContext::render(bool lock)
{
	RPR_THREAD_ONLY;
	LOCKMUTEX((lock ? this : nullptr));

	auto context = scope.Context();

	if (!context)
		return;

	if (m_restartRender)
	{
		for (int i = 0; i != RPR_AOV_MAX; ++i) 
		{
			if (aovEnabled[i])
			{
				m.framebufferAOV[i].Clear();

				if (m.framebufferAOV_resolved[i].IsValid())
				{
					m.framebufferAOV_resolved[i].Clear();
				}
			}
		}

		if (m_interactive && m_denoiserChanged && m_globals.denoiserSettings.enabled && IsDenoiserSupported())
		{
			turnOnAOVsForDenoiser(true);
			setupDenoiserFB();
			m_denoiserChanged = false;
		}

		m_restartRender = false;
		m_renderStartTime = clock();
		m_currentIteration = 0;
		m_currentFrame = 0;
	}

	// may need to change iteration step
	int iterationStep = m_samplesPerUpdate;

	if (!m_completionCriteriaParams.isUnlimitedIterations())
	{
		int remainingIterations = m_completionCriteriaParams.completionCriteriaMaxIterations - m_currentIteration;
		if (remainingIterations < iterationStep)
		{
			iterationStep = remainingIterations;
		}
	}

	context.SetParameter(RPR_CONTEXT_ITERATIONS, iterationStep);
	context.SetParameter(RPR_CONTEXT_FRAMECOUNT, m_currentFrame);

	ContextWorkProgressData progressData;

	progressData.currentIndex = m_currentIteration;
	progressData.totalCount = m_completionCriteriaParams.completionCriteriaMaxIterations;
	progressData.currentTimeInMiliseconds = TimeDiff(clock(), m_workStartTime);
	progressData.progressType = ProgressType::RenderPassStarted;

	TriggerProgressCallback(progressData);

	if (m_useRegion)
		context.RenderTile(m_region.left, m_region.right+1, m_height - m_region.top - 1, m_height - m_region.bottom);
	else
		context.Render();

	if (m_currentIteration == 0)
	{
		m_lastRenderStartTime = std::chrono::system_clock::now();

		DebugPrint("RPR GPU Memory used: %dMB", context.GetMemoryUsage() >> 20);
	}

	m_currentIteration += iterationStep;
	m_currentFrame++;

	m_cameraAttributeChanged = false;
}

void FireRenderContext::setSamplesPerUpdate(int samplesPerUpdate)
{
	m_samplesPerUpdate = samplesPerUpdate;
}

void FireRenderContext::saveToFile(MString& filePath, const ImageFileDescription& imgDescription)
{
	RPR_THREAD_ONLY;
	DebugPrint("FireRenderContext::saveToFile(...)");
	float* data = new float[m_width * m_height * 4];

	{
		LOCKMUTEX(this);

		rpr_framebuffer fb = frameBufferAOV_Resolved(RPR_AOV_COLOR);

		size_t dataSize = 0;
		auto frstatus = rprFrameBufferGetInfo(fb, RPR_FRAMEBUFFER_DATA, 0, NULL, &dataSize);
		checkStatus(frstatus);

		frstatus = rprFrameBufferGetInfo(fb, RPR_FRAMEBUFFER_DATA, dataSize, &data[0], NULL);
		checkStatus(frstatus);
	}

	OIIO::ImageOutput *outImage = OIIO::ImageOutput::create(filePath.asChar());
	if (outImage)
	{
		OIIO::ImageSpec imgSpec(m_width, m_height, 4, OIIO::TypeDesc::FLOAT);
		outImage->open(filePath.asChar(), imgSpec);
		outImage->write_image(OIIO::TypeDesc::FLOAT, &data[0]);
		outImage->close();
		delete outImage;
	}
	else
	{
		float* tmpBuffer = new float[m_width * m_height * 4];
		memcpy(&tmpBuffer[0], &data[0], sizeof(float) * m_width * m_height * 4);
		for (unsigned int y = 0; y < (m_height); y++)
		{
			memcpy(
				&data[y * m_width * 4],
				&tmpBuffer[(m_height - y - 1) * m_width * 4],
				sizeof(float) * m_width * 4
			);
		}
		delete[] tmpBuffer;

		MStatus status;
		//save 24bit with MImage
		//for some reason Maya invert the red channel with the blue channel so compensate this effect
		for (unsigned int pId = 0; pId < (m_width * m_height); pId++)
		{
			float red = data[4 * pId];
			data[4 * pId] = data[4 * pId + 2];
			data[4 * pId + 2] = red;
		}
		MImage newImage;
		newImage.create(m_width, m_height, 4u, MImage::kFloat);
		newImage.setFloatPixels(&data[0], m_width, m_height);
		newImage.convertPixelFormat(MImage::kByte);
		newImage.setRGBA(true);
		status = newImage.writeToFile(filePath, imgDescription.extension.c_str());
		if (status != MS::kSuccess)
		{
			MGlobal::displayError("Unable to save " + filePath);
		}
	}

	delete[] data;
}

std::vector<float> FireRenderContext::getRenderImageData()
{
	RPR_THREAD_ONLY;
	DebugPrint("FireRenderContext::getRenderImageData()");

	rpr_framebuffer fb = frameBufferAOV_Resolved(RPR_AOV_COLOR);

	size_t dataSize = 0;
	auto frstatus = rprFrameBufferGetInfo(fb, RPR_FRAMEBUFFER_DATA, 0, NULL, &dataSize);
	checkStatus(frstatus);

	std::vector<float> data(m_width * m_height * 4);
	frstatus = rprFrameBufferGetInfo(fb, RPR_FRAMEBUFFER_DATA, dataSize, data.data(), NULL);
	checkStatus(frstatus);

	return data;
}

unsigned int FireRenderContext::width() const
{
	return m_width;
}

unsigned int FireRenderContext::height() const
{
	return m_height;
}

bool FireRenderContext::isRenderView() const
{
	return m_isRenderView;
}

bool FireRenderContext::createContext(rpr_creation_flags createFlags, rpr_context& result, int* pOutRes)
{
	RPR_THREAD_ONLY;

	rpr_context context = nullptr;

	bool useThread = (createFlags & RPR_CREATION_FLAGS_ENABLE_CPU) == RPR_CREATION_FLAGS_ENABLE_CPU;
	DebugPrint("* Creating Context: %d (0x%x) - useThread: %d", createFlags, createFlags, useThread);

	if (isMetalOn() && !(createFlags & RPR_CREATION_FLAGS_ENABLE_CPU))
	{
		createFlags = createFlags | RPR_CREATION_FLAGS_ENABLE_METAL;
	}

	int res = CreateContextInternal(createFlags, &context);

	if (pOutRes != nullptr)
	{
		*pOutRes = res;
	}

	switch (res)
	{
	case RPR_SUCCESS:
		result = context;
		FireRenderThread::UseTheThread(useThread);
		return true;
	case RPR_ERROR_UNSUPPORTED:
		MGlobal::displayError("RPR_ERROR_UNSUPPORTED");
		return false;
	}

	MGlobal::displayError(MString("rprCreateContext returned error: ") + res);

	DebugPrint("rprCreateContext returned error: %d (%s, %d)", res, __FILE__, __LINE__);
	return false;
}

int FireRenderContext::getThreadCountToOverride() const
{
	bool useViewportParams = isInteractive();
	                     
	bool isOverriden;
	int cpuThreadCount;

	FireRenderGlobalsData::getCPUThreadSetup(isOverriden, cpuThreadCount, m_RenderType);

	if (isOverriden)
	{
		return cpuThreadCount;
	}

	// means we dont want to override cpu thread count
	return 0;
}

void FireRenderContext::BuildLateinitObjects()
{
	for (const MDagPath path : m_LateinitMASHInstancers)
	{
		CreateSceneObject<InstancerMASH, NodeCachingOptions::AddPath>(path);
	}
	m_LateinitMASHInstancers.clear();
}

bool FireRenderContext::createContextEtc(rpr_creation_flags creation_flags, bool destroyMaterialSystemOnDelete, bool glViewport, int* pOutRes)
{
	return FireRenderThread::RunOnceAndWait<bool>([this, &creation_flags, destroyMaterialSystemOnDelete, glViewport, pOutRes]()
	{
		RPR_THREAD_ONLY;

		DebugPrint("FireRenderContext::createContextEtc(%d)", creation_flags);

		// Use OpenGL interop for OpenGL based viewports if required.
		if (glViewport)
		{
			// GL interop is active if enabled and not using CPU rendering.
			bool useCPU = (creation_flags & RPR_CREATION_FLAGS_ENABLE_CPU) != 0;
			m_glInteropActive = !useCPU && IsGLInteropEnabled();

			if (m_glInteropActive)
				creation_flags |= RPR_CREATION_FLAGS_ENABLE_GL_INTEROP;
		}
		else
			m_glInteropActive = false;

		rpr_context handle;
		bool contextCreated = createContext(creation_flags, handle, pOutRes);
		if (!contextCreated)
		{
			MGlobal::displayError("Unable to create Radeon ProRender context.");
			return false;
		}

		scope.Init(handle, destroyMaterialSystemOnDelete);

#ifdef _DEBUG
		static int dumpDebug;
		if (!dumpDebug)
		{
			scope.Context().DumpParameterInfo();
			dumpDebug = 1;
		}
#endif

		for (auto& fb : m.framebufferAOV)
			fb.Reset();

		for (auto& fb : m.framebufferAOV_resolved)
			fb.Reset();

		return true;
	});
}

rpr_context FireRenderContext::context()
{
	RPR_THREAD_ONLY;

	assert(scope.Context());
	return scope.Context().Handle();
}

rpr_material_system FireRenderContext::materialSystem()
{
	RPR_THREAD_ONLY;

	assert(scope.MaterialSystem());
	return scope.MaterialSystem().Handle();
}

rpr_framebuffer FireRenderContext::frameBufferAOV(int aov) const
{
	RPR_THREAD_ONLY;

	assert(m.framebufferAOV[aov]);
	return m.framebufferAOV[aov].Handle();
}

rpr_framebuffer FireRenderContext::frameBufferAOV_Resolved(int aov) {
	RPR_THREAD_ONLY;
	frw::FrameBuffer fb;

	if (m.framebufferAOV[aov].Handle() == nullptr)
	{
		return nullptr;
	}

	if (needResolve())
	{
		//resolve tone mapping
		m.framebufferAOV[aov].Resolve(m.framebufferAOV_resolved[aov], aov != RPR_AOV_COLOR);
		fb = m.framebufferAOV_resolved[aov];
	}
	else
	{
		fb = m.framebufferAOV[aov];
	}
	return fb.Handle();
}

// -----------------------------------------------------------------------------
bool FireRenderContext::ConsiderShadowReflectionCatcherOverride(const ReadFrameBufferRequestParams& params)
{
	RPR_THREAD_ONLY;

	/**
	 * Shadow catcher can work only if COLOR, BACKGROUND, OPACITY and SHADOW_CATCHER AOVs are turned on.
	 * If all of them are turned on and shadow catcher is requested - run composite pipeline.
	 */
	bool isShadowCather = (params.aov == RPR_AOV_COLOR) &&
		params.mergeShadowCatcher &&
		m.framebufferAOV[RPR_AOV_SHADOW_CATCHER] &&
		m.framebufferAOV[RPR_AOV_BACKGROUND] &&
		m.framebufferAOV[RPR_AOV_OPACITY] &&
		scope.GetShadowCatcherShader();

	/**
	 * Reflection catcher can work only if COLOR, BACKGROUND, OPACITY and REFLECTION_CATCHER AOVs are turned on.
	 * If all of them are turned on and reflection catcher is requested - run composite pipeline.
	 */
	bool isReflectionCatcher = (params.aov == RPR_AOV_COLOR) &&
		params.mergeShadowCatcher &&
		m.framebufferAOV[RPR_AOV_REFLECTION_CATCHER] &&
		m.framebufferAOV[RPR_AOV_BACKGROUND] &&
		m.framebufferAOV[RPR_AOV_OPACITY] &&
		scope.GetReflectionCatcherShader();

	if (isShadowCather && isReflectionCatcher)
	{
		compositeReflectionShadowCatcherOutput(params);
		return true;
	}

	if (isShadowCather)
	{
		compositeShadowCatcherOutput(params);
		return true;
	}

	if (isReflectionCatcher)
	{
		compositeReflectionCatcherOutput(params);
		return true;
	}

	// SC or RC not used
	return false;
}

void FireRenderContext::DebugDumpAOV(int aov) const
{
#ifdef _DEBUG
	std::map<unsigned int, std::string> aovNames =
	{
		 {RPR_AOV_COLOR, "RPR_AOV_COLOR"}
		,{RPR_AOV_OPACITY, "RPR_AOV_OPACITY" }
		,{RPR_AOV_WORLD_COORDINATE, "RPR_AOV_WORLD_COORDINATE" }
		,{RPR_AOV_UV, "RPR_AOV_UV" }
		,{RPR_AOV_MATERIAL_IDX, "RPR_AOV_MATERIAL_IDX" }
		,{RPR_AOV_GEOMETRIC_NORMAL, "RPR_AOV_GEOMETRIC_NORMAL" }
		,{RPR_AOV_SHADING_NORMAL, "RPR_AOV_SHADING_NORMAL" }
		,{RPR_AOV_DEPTH, "RPR_AOV_DEPTH" }
		,{RPR_AOV_OBJECT_ID, "RPR_AOV_OBJECT_ID" }
		,{RPR_AOV_OBJECT_GROUP_ID, "RPR_AOV_OBJECT_GROUP_ID" }
		,{RPR_AOV_SHADOW_CATCHER, "RPR_AOV_SHADOW_CATCHER" }
		,{RPR_AOV_BACKGROUND, "RPR_AOV_BACKGROUND" }
		,{RPR_AOV_EMISSION, "RPR_AOV_EMISSION" }
		,{RPR_AOV_VELOCITY, "RPR_AOV_VELOCITY" }
		,{RPR_AOV_DIRECT_ILLUMINATION, "RPR_AOV_DIRECT_ILLUMINATION" }
		,{RPR_AOV_INDIRECT_ILLUMINATION, "RPR_AOV_INDIRECT_ILLUMINATION"}
		,{RPR_AOV_AO, "RPR_AOV_AO" }
		,{RPR_AOV_DIRECT_DIFFUSE, "RPR_AOV_DIRECT_DIFFUSE" }
		,{RPR_AOV_DIRECT_REFLECT, "RPR_AOV_DIRECT_REFLECT" }
		,{RPR_AOV_INDIRECT_DIFFUSE, "RPR_AOV_INDIRECT_DIFFUSE" }
		,{RPR_AOV_INDIRECT_REFLECT, "RPR_AOV_INDIRECT_REFLECT" }
		,{RPR_AOV_REFRACT, "RPR_AOV_REFRACT" }
		,{RPR_AOV_VOLUME, "RPR_AOV_VOLUME" }
		,{RPR_AOV_LIGHT_GROUP0, "RPR_AOV_LIGHT_GROUP0" }
		,{RPR_AOV_LIGHT_GROUP1, "RPR_AOV_LIGHT_GROUP1" }
		,{RPR_AOV_LIGHT_GROUP2, "RPR_AOV_LIGHT_GROUP2" }
		,{RPR_AOV_LIGHT_GROUP3, "RPR_AOV_LIGHT_GROUP3" }
		,{RPR_AOV_DIFFUSE_ALBEDO, "RPR_AOV_DIFFUSE_ALBEDO" }
		,{RPR_AOV_VARIANCE, "RPR_AOV_VARIANCE" }
		,{RPR_AOV_VIEW_SHADING_NORMAL, "RPR_AOV_VIEW_SHADING_NORMAL" }
		,{RPR_AOV_REFLECTION_CATCHER, "RPR_AOV_REFLECTION_CATCHER" }
		,{RPR_AOV_MAX, "RPR_AOV_MAX" }
	};

	std::stringstream ssFileNameResolved;
	ssFileNameResolved << aovNames[aov] << "_resolved.png";
	rprFrameBufferSaveToFile(m.framebufferAOV_resolved[aov].Handle(), ssFileNameResolved.str().c_str());

	std::stringstream ssFileNameNOTResolved;
	ssFileNameNOTResolved << aovNames[aov] << "_NOTresolved.png";
	rprFrameBufferSaveToFile(m.framebufferAOV[aov].Handle(), ssFileNameNOTResolved.str().c_str());
#endif
}

std::vector<float> FireRenderContext::GetDenoisedData(bool& result)
{	
#ifdef _DEBUG
#ifdef DUMP_FRAMEBUFF
	if (m_globals.denoiserSettings.type == FireRenderGlobals::kBilateral)
	{
		m.framebufferAOV_resolved[RPR_AOV_COLOR].DebugDumpFrameBufferToFile(std::string("RPR_AOV_COLOR"));
		m.framebufferAOV_resolved[RPR_AOV_SHADING_NORMAL].DebugDumpFrameBufferToFile(std::string("RPR_AOV_SHADING_NORMAL"));
		m.framebufferAOV_resolved[RPR_AOV_WORLD_COORDINATE].DebugDumpFrameBufferToFile(std::string("RPR_AOV_WORLD_COORDINATE"));
		m.framebufferAOV_resolved[RPR_AOV_DEPTH].DebugDumpFrameBufferToFile(std::string("RPR_AOV_DEPTH"));
		m_pixelBuffers[RPR_AOV_COLOR].debugDump(m_pixelBuffers[
			RPR_AOV_COLOR].height(),
				m_pixelBuffers[RPR_AOV_COLOR].width(),
				std::string("RPR_AOV_COLOR"));

		m_pixelBuffers[RPR_AOV_SHADING_NORMAL].debugDump(m_pixelBuffers[
			RPR_AOV_SHADING_NORMAL].height(),
				m_pixelBuffers[RPR_AOV_SHADING_NORMAL].width(),
				std::string("RPR_AOV_SHADING_NORMAL"));

		m_pixelBuffers[RPR_AOV_WORLD_COORDINATE].debugDump(m_pixelBuffers[
			RPR_AOV_WORLD_COORDINATE].height(),
				m_pixelBuffers[RPR_AOV_WORLD_COORDINATE].width(),
				std::string("RPR_AOV_WORLD_COORDINATE"));

		m_pixelBuffers[RPR_AOV_DEPTH].debugDump(m_pixelBuffers[
			RPR_AOV_DEPTH].height(),
				m_pixelBuffers[RPR_AOV_DEPTH].width(),
				std::string("RPR_AOV_DEPTH"));
	}
#endif // DUMP_FRAMEBUFF
#endif // DEBUG

	try
	{
		m_denoiserFilter->Run();
		std::vector<float> vecData = m_denoiserFilter->GetData();
		result = true;
		return vecData;
	}
	catch (std::exception& e)
	{
		m_denoiserFilter.reset();
		ErrorPrint(e.what());
		MGlobal::displayError("RPR failed to execute denoiser, turning it off.");

		result = false;
		return std::vector<float>();
	}
}

RV_PIXEL* FireRenderContext::GetAOVData(const ReadFrameBufferRequestParams& params)
{
	rpr_int frstatus = RPR_SUCCESS;
	size_t dataSize;

	// Get data from the RPR frame buffer.
	rpr_framebuffer frameBuffer = frameBufferAOV_Resolved(params.aov);
	frstatus = rprFrameBufferGetInfo(frameBuffer, RPR_FRAMEBUFFER_DATA, 0, nullptr, &dataSize);
	checkStatus(frstatus);

#ifdef _DEBUG
#ifdef DUMP_PIXELS_SOURCE
	rprFrameBufferSaveToFile(frameBuffer, "C:\\temp\\dbg\\3.png");
#endif
#endif

	// Check that the reported frame buffer size
	// in bytes matches the required dimensions.
	assert(dataSize == (sizeof(RV_PIXEL) * params.PixelCount()));

	// Copy the frame buffer into temporary memory, if
	// required, or directly into the supplied pixel buffer.
	if (params.UseTempData())
		m_tempData.resize(params.PixelCount());

	RV_PIXEL* data = params.UseTempData() ? m_tempData.get() : params.pixels;
	frstatus = rprFrameBufferGetInfo(frameBuffer, RPR_FRAMEBUFFER_DATA, dataSize, &data[0], nullptr);
	checkStatus(frstatus);

	return data;
}

void FireRenderContext::MergeOpacity(const ReadFrameBufferRequestParams& params, size_t dataSize)
{
	// No need to merge opacity for any FB other then color
	if (!params.mergeOpacity || params.aov != RPR_AOV_COLOR)
		return;

	rpr_framebuffer opacityFrameBuffer = frameBufferAOV_Resolved(RPR_AOV_OPACITY);
	if (opacityFrameBuffer == nullptr)
		return;

	m_opacityData.resize(params.PixelCount());

	if (params.UseTempData())
	{
		m_opacityTempData.resize(params.PixelCount());

		rpr_int frstatus = rprFrameBufferGetInfo(opacityFrameBuffer, RPR_FRAMEBUFFER_DATA, dataSize, m_opacityTempData.get(), nullptr);
		checkStatus(frstatus);

		copyPixels(m_opacityData.get(), m_opacityTempData.get(), params.width, params.height, params.region, params.flip);
	}
	else
	{
		rpr_int frstatus = rprFrameBufferGetInfo(opacityFrameBuffer, RPR_FRAMEBUFFER_DATA, dataSize, m_opacityData.get(), nullptr);
		checkStatus(frstatus);
	}
}

void FireRenderContext::CombineOpacity(ReadFrameBufferRequestParams& params)
{
	//combine (Opacity to Alpha)
	// No need to merge opacity for any FB other then color
	if (!params.mergeOpacity || params.aov != RPR_AOV_COLOR)
		return;

	combineWithOpacity(params.pixels, params.region.getArea(), m_opacityData.get());
}

void FireRenderContext::readFrameBuffer(ReadFrameBufferRequestParams& params)
{
	RPR_THREAD_ONLY;

	// resolve frame buffer
	rpr_framebuffer frameBuffer = frameBufferAOV_Resolved(params.aov);

	// debug output (if enabled)
#ifdef DUMP_AOV_SOURCE
	DebugDumpAOV(params.aov);
#endif

	// process shadow and/or reflection catcher logic
	bool isShadowReflectionCatcherUsed = ConsiderShadowReflectionCatcherOverride(params);
	if (isShadowReflectionCatcherUsed)
		return; // fo now; should run denoiser if enabled instead

	// load data normally either from RIF or from AOV
	bool shouldRunDenoiser = (params.aov == RPR_AOV_COLOR && IsDenoiserEnabled() && (!params.isDenoiserDisabled));

	RV_PIXEL* data = nullptr;
	std::vector<float> vecData;
	if (shouldRunDenoiser && (params.aov == RPR_AOV_COLOR))
	{
		// - run RIF and load data
		bool denoiseResult = false;
		vecData = GetDenoisedData(denoiseResult);
		if (!denoiseResult)
			return;

		assert(vecData.size() == params.PixelCount() * 4);

		data = (RV_PIXEL*)&vecData[0];
	}
	else
	{
		// load data from AOV
		data = GetAOVData(params);
	}

	// No need to merge opacity for any FB other then color
	MergeOpacity(params, (sizeof(RV_PIXEL) * params.PixelCount()));

	// Copy the region from the temporary
	// buffer into supplied pixel memory.
	if (params.UseTempData() || IsDenoiserEnabled())
	{
		copyPixels(params.pixels, data, params.width, params.height, params.region, params.flip);
	}

	//combine (Opacity to Alpha)
	// No need to merge opacity for any FB other then color
	CombineOpacity(params);
}

#ifdef _DEBUG
void generateBitmapImage(unsigned char *image, int height, int width, int pitch, const char* imageFileName);
#endif

// -----------------------------------------------------------------------------
void FireRenderContext::copyPixels(RV_PIXEL* dest, RV_PIXEL* source,
	unsigned int sourceWidth, unsigned int sourceHeight,
	const RenderRegion& region, bool flip) const
{
	RPR_THREAD_ONLY;
	// Get region dimensions.
	unsigned int regionWidth = region.getWidth();
	unsigned int regionHeight = region.getHeight();

	for (unsigned int y = 0; y < regionHeight; y++)
	{
		unsigned int destIndex = y * regionWidth;
				
		unsigned int sourceIndex = flip ?
			(sourceHeight - (region.bottom + y) - 1) * sourceWidth + region.left :
			(sourceHeight - (region.top - y) - 1) * sourceWidth + region.left;

		memcpy(&dest[destIndex], &source[sourceIndex], sizeof(RV_PIXEL) * regionWidth);
	}

#ifdef _DEBUG
#ifdef DUMP_PIXELS_SOURCE
	if (debugDump) {
		static int debugDumpIdx = 0;
		std::vector<RV_PIXEL> sourcePixels;
		for (unsigned int y = 0; y < regionHeight; y++)
		{
			for (unsigned int x = 0; x < regionWidth; x++)
			{
				RV_PIXEL pixel = source[x + y * regionWidth];
				sourcePixels.push_back(pixel);
			}
		}

		std::vector<unsigned char> buffer2;
		for (unsigned int y = 0; y < regionHeight; y++)
		{
			for (unsigned int x = 0; x < regionWidth; x++)
			{
				RV_PIXEL& pixel = sourcePixels[x + y * regionWidth];
				char r = 255 * pixel.r;
				char g = 255 * pixel.g;
				char b = 255 * pixel.b;

				buffer2.push_back(r);
				buffer2.push_back(g);
				buffer2.push_back(b);
				buffer2.push_back(255);
			}
		}

		std::string dumpAddr = "C:\\temp\\dbg\\2.bmp" + std::to_string(debugDumpIdx++);
		unsigned char* dst2 = buffer2.data();
		generateBitmapImage(dst2, sourceHeight, sourceWidth, sourceWidth * 4, dumpAddr.c_str());
	}
#endif
#endif
}

// -----------------------------------------------------------------------------
void FireRenderContext::combineWithOpacity(RV_PIXEL* pixels, unsigned int size, RV_PIXEL *opacityPixels) const
{
	if (opacityPixels != NULL)
	{
		float alpha = 0.0f;
		for (unsigned int i = 0; i < size; i++)
		{
			alpha = opacityPixels[i].r;

			pixels[i].a = alpha;
		}
	}
}

FireRenderObject* FireRenderContext::getRenderObject(const std::string& name)
{
	auto it = m_sceneObjects.find(name);
	if (it != m_sceneObjects.end())
		return it->second.get();
	return nullptr;
}

FireRenderObject* FireRenderContext::getRenderObject(const MDagPath& ob)
{
	auto uuid = getNodeUUid(ob);
	return getRenderObject(uuid);
}

FireRenderObject* FireRenderContext::getRenderObject(const MObject& ob)
{
	auto uuid = getNodeUUid(ob);
	return getRenderObject(uuid);
}


void FireRenderContext::RemoveRenderObject(const MObject& ob)
{
	RPR_THREAD_ONLY;

	for (auto it = m_sceneObjects.begin(); it != m_sceneObjects.end();)
	{
		if (auto frNode = dynamic_cast<FireRenderNode*>(it->second.get()))
		{
			MDagPath dagPath;

			// it is unsafe to call DagPath for the object which has been just removed from scene (crash may occur)
			if (frNode->Object() != ob)
			{
				dagPath = frNode->DagPath();
			}

			if (!dagPath.isValid() || dagPath.node() == ob || dagPath.transform() == ob)
			{
				// remove object from dag path cache
				FireRenderObject* pRobj = getRenderObject(ob);
				if (pRobj != nullptr)
				{
					MDagPath tmpPath;
					bool found = GetNodePath(tmpPath, pRobj->uuid());

					if (found)
					{
						m_nodePathCache.erase(pRobj->uuid());
					}
				}

				// remove object from main meshes cache
				/**
					Sometimes it get fired for Node or Object, but the map of meshes should contain only FireRenderMesh
					Dynamic cast is needed to type check
				*/
				FireRenderMesh* mesh = dynamic_cast<FireRenderMesh*>(frNode);
				if (mesh != nullptr)
				{
					RemoveMainMesh(mesh);
				}

				// remove object from scene
				frNode->detachFromScene();
				it = m_sceneObjects.erase(it);
				setDirty();

				continue;
			}
		}
		++it;
	}
}

void FireRenderContext::RefreshInstances()
{
	MAIN_THREAD_ONLY;

	std::list<MDagPath> toAdd;
	for (auto it = m_sceneObjects.begin(); it != m_sceneObjects.end();)
	{
		if (auto frNode = dynamic_cast<FireRenderNode*>(it->second.get()))
		{
			auto dagPath = frNode->DagPath();
			if (!dagPath.isValid())
			{
				frNode->detachFromScene();
				it = m_sceneObjects.erase(it);
				setDirty();
				continue;
			}

			MDagPathArray paths;
			if (MStatus::kSuccess == MDagPath::getAllPathsTo(frNode->Object(), paths))
			{
				for (auto path : paths)
				{
					if (!getRenderObject(path))
						toAdd.push_back(path);
				}
			}
		}
		++it;
	}

	for (auto it : toAdd)
		AddSceneObject(it);
}

rpr_scene  FireRenderContext::scene()
{
	RPR_THREAD_ONLY;

	return scope.Scene().Handle();
}

FireRenderCamera & FireRenderContext::camera()
{
	return m_camera;
}

void FireRenderContext::attachCallbacks()
{
	DebugPrint("FireRenderContext(context=%p)::attachCallbacks()", this);
	if (getCallbackCreationDisabled())
		return;

	//Remove old callbacks from this context if any exist
	removeCallbacks();

	MStatus status;
	m_removedNodeCallback = MDGMessage::addNodeRemovedCallback(FireRenderContext::removedNodeCallback, "dependNode", this, &status);
	m_addedNodeCallback = MDGMessage::addNodeAddedCallback(FireRenderContext::addedNodeCallback, "dependNode", this, &status);

	MSelectionList slist;
	MObject node;
	slist.add("RadeonProRenderGlobals");
	slist.getDependNode(0, node);
	m_renderGlobalsCallback = MNodeMessage::addAttributeChangedCallback(node, FireRenderContext::globalsChangedCallback, this, &status);
	slist.add("renderLayerManager");
	slist.getDependNode(1, node);
	m_renderLayerCallback = MNodeMessage::addAttributeChangedCallback(node, FireRenderContext::renderLayerManagerCallback, this, &status);
}

void FireRenderContext::removeCallbacks()
{
	DebugPrint("FireRenderContext(context=%p)::removeCallbacks()", this);
	
	if (m_removedNodeCallback)
		MMessage::removeCallback(m_removedNodeCallback);
	if (m_addedNodeCallback)
		MMessage::removeCallback(m_addedNodeCallback);
	if (m_renderGlobalsCallback)
		MMessage::removeCallback(m_renderGlobalsCallback);
	if (m_renderLayerCallback)
		MMessage::removeCallback(m_renderLayerCallback);

	m_removedNodeCallback = m_addedNodeCallback = m_renderGlobalsCallback = m_renderLayerCallback = 0;
}

void FireRenderContext::removedNodeCallback(MObject &node, void *clientData)
{
	if (!DoesNodeAffectContextRefresh(node))
	{
		return;
	}

	if (auto frContext = GetCallbackContext(clientData))
	{
		DebugPrint("FireRenderContext(context=%p)::removedNodeCallback()", frContext);
		// If we have same node in added list then it wasn't yet processed by FireRenderContext.
		// We should remove it from added list, since it's already removed. Calling any function on such node
		// will lead to crash in Maya
		auto it = std::find(frContext->m_addedNodes.begin(), frContext->m_addedNodes.end(), node);
		if (it != frContext->m_addedNodes.end())
		{
			frContext->m_addedNodes.erase(it);
			return;
		}

		frContext->m_removedNodes.push_back(node);
		frContext->setDirty();
	}
}

void FireRenderContext::addedNodeCallback(MObject &node, void *clientData)
{
	if (auto frContext = GetCallbackContext(clientData))
	{
		DebugPrint("FireRenderContext(context=%p)::addedNodeCallback()", frContext);

		if (DoesNodeAffectContextRefresh(node))
		{
			frContext->m_addedNodes.push_back(node);
			frContext->setDirty();
		}
	}
}

bool FireRenderContext::DoesNodeAffectContextRefresh(const MObject &node)
{
	if (node.isNull())
	{
		return false;
	}

	return node.hasFn(MFn::kDagNode) || node.hasFn(MFn::kDisplayLayer);
}

void FireRenderContext::addNode(const MObject& node)
{
	MAIN_THREAD_ONLY;

	if (!DoesNodeAffectContextRefresh(node))
	{
		return;
	}

	if (node.hasFn(MFn::kDagNode))
	{
		MFnDagNode dagNode(node);

		MDagPath dagPath;
		if (MStatus::kSuccess == dagNode.getPath(dagPath))
			AddSceneObject(dagPath);
	}
	else if (node.hasFn(MFn::kDisplayLayer))
	{
		FireRenderDisplayLayer* layer = new FireRenderDisplayLayer(this, node);
		AddSceneObject(layer);
	}
}

void FireRenderContext::removeNode(MObject& node)
{
	MAIN_THREAD_ONLY;

	RemoveRenderObject(node);
}

void FireRenderContext::updateFromGlobals(bool applyLock)
{
	MAIN_THREAD_ONLY;

	if (m_tonemappingChanged)
	{
        if (applyLock)
        {
            LOCKFORUPDATE(this);
        }

		m_globals.readFromCurrentScene();
		updateTonemapping(m_globals);

		m_tonemappingChanged = false;
	}

	if (!m_globalsChanged)
		return;

    if (applyLock)
    {
        LOCKFORUPDATE(this);
    }
    
	m_globals.readFromCurrentScene();
	setupContext(m_globals);

	updateLimitsFromGlobalData(m_globals);
	updateMotionBlurParameters(m_globals);

	m_camera.setType(m_globals.cameraType);

	m_globalsChanged = false;
}

void FireRenderContext::updateRenderLayers()
{
	MAIN_THREAD_ONLY;

	if (!m_renderLayersChanged)
		return;

	for (const auto& it : m_sceneObjects)
	{
		if (auto frNode = dynamic_cast<FireRenderNode*>(it.second.get()))
		{
			auto node = frNode->Object();
			MDagPath path = MDagPath::getAPathTo(node);
			MFnDependencyNode nodeFn(node);
			MString name = nodeFn.name();
			if (path.isValid())
			{
				if (path.isVisible())
					frNode->attachToScene();
				else
					frNode->detachFromScene();
			}
			else
			{
				MGlobal::displayError("invalid path " + name);
			}
		}
	}

	m_renderLayersChanged = false;
}

// TODO Need to be refactored in a more flexible way
void FireRenderContext::globalsChangedCallback(MNodeMessage::AttributeMessage msg, MPlug &plug, MPlug &otherPlug, void *clientData)
{
	MAIN_THREAD_ONLY

	DebugPrint("FireRenderContext::globalsChangedCallback(%d, %s, isNull=%d, 0x%x)", msg,  plug.name().asUTF8(), otherPlug.isNull(), clientData);

	if (auto frContext = GetCallbackContext(clientData))
	{
		AutoMutexLock lock(frContext->m_dirtyMutex);

		bool restartRender = false;
		if (FireRenderGlobalsData::isTonemapping(plug.name()))
		{
			frContext->m_tonemappingChanged = true;
			restartRender = true;
		}
		else
		{
			if (FireRenderGlobalsData::isDenoiser(plug.name()))
			{
				frContext->m_denoiserChanged = true;
				restartRender = true;
			}
			else if (FireRenderGlobalsData::IsMotionBlur(plug.name()))
			{
				restartRender = true;
			}

			RenderType renderType = frContext->GetRenderType();
			RenderQuality quality = GetRenderQualityForRenderType(renderType);

			if (!frContext->IsRenderQualitySupported(quality))
			{
				// If current Render Context does not support newly selected render quality we need to restart the render
				frContext->m_DoesContextSupportCurrentSettings = false;			
			}

			frContext->m_globalsChanged = true;

			frContext->setDirty();

			if (restartRender)
			{
				frContext->m_restartRender = restartRender;
			}
		}
	}
}

void FireRenderContext::updateMotionBlurParameters(const FireRenderGlobalsData& globalData)
{
	RPR_THREAD_ONLY;

	bool updateMeshes = false;
	bool updateCamera = false;

	// Here is logic about proper update only necessary objects depending of type of Context
	if (isInteractive())
	{
		if (m_viewportMotionBlur != globalData.viewportMotionBlur)
		{
			updateMeshes = true;
			updateCamera = true;
		}
	}
	else
	{
		if (m_motionBlur == false && globalData.motionBlur == false)
		{
			return;
		}

		if (m_motionBlur != globalData.motionBlur)
		{
			updateMeshes = true;
			updateCamera = true;
		}

		if (m_cameraMotionBlur != m_globals.cameraMotionBlur)
		{
			updateCamera = true;
		}
	}

	if (m_motionBlurCameraExposure != m_globals.motionBlurCameraExposure)
	{
		updateCamera = true;
	}

	if (updateMeshes)
	{
		for (const auto& it : m_sceneObjects)
		{
			if (auto frMesh = dynamic_cast<FireRenderMesh*>(it.second.get()))
			{
				frMesh->setDirty();
			}
		}
	}

	if (updateCamera)
	{
		m_camera.setDirty();
	}

	setMotionBlurParameters(globalData);
}

void FireRenderContext::setMotionBlurParameters(const FireRenderGlobalsData& globalData)
{
	m_motionBlur = globalData.motionBlur;
	m_cameraMotionBlur = m_globals.cameraMotionBlur;
	m_motionBlurCameraExposure = m_globals.motionBlurCameraExposure;
	m_viewportMotionBlur = globalData.viewportMotionBlur;
}

bool FireRenderContext::isInteractive() const
{
	return (m_RenderType == RenderType::IPR) || (m_RenderType == RenderType::ViewportRender);
}

bool FireRenderContext::isGLInteropActive() const
{
	return m_glInteropActive;
}

void FireRenderContext::renderLayerManagerCallback(MNodeMessage::AttributeMessage msg, MPlug & plug, MPlug & otherPlug, void * clientData)
{
	if (auto frContext = GetCallbackContext(clientData))
	{
		if (plug.partialName() == "crl")
		{
			AutoMutexLock lock(frContext->m_dirtyMutex);

			frContext->m_renderLayersChanged = true;
			frContext->setDirty();
		}
	}
}

bool FireRenderContext::AddSceneObject(FireRenderObject* ob)
{
	RPR_THREAD_ONLY;

	if (ob->uuid().empty())
		return false;

	if (m_sceneObjects.find(ob->uuid()) != m_sceneObjects.end())
		DebugPrint("ERROR: Replacing existing object without deleting first");

	m_sceneObjects[ob->uuid()] = std::shared_ptr<FireRenderObject>(ob);
	ob->setDirty();

	return true;
}

bool IsXgenGuides(MObject& node)
{
	MFnDependencyNode fnNode(node);
	MStatus returnStatus;

	// - get out attribute
	MObject attr = fnNode.attribute("outSplineData", &returnStatus);
	CHECK_MSTATUS(returnStatus)

	MPlug plug = fnNode.findPlug(attr, false, &returnStatus);
	CHECK_MSTATUS(returnStatus)

	// - iterate through node connections; search for connected xgmSplineDescription
	MItDependencyGraph itdep(
		plug,
		MFn::kPluginDependNode,
		MItDependencyGraph::kDownstream,
		MItDependencyGraph::kDepthFirst,
		MItDependencyGraph::kPlugLevel,
		&returnStatus);
	CHECK_MSTATUS(returnStatus)

	for (; !itdep.isDone(); itdep.next())
	{
		MFnDependencyNode connectionNode(itdep.currentItem());
		MString connectionNode_typename = connectionNode.typeName();

		// xgmSplineDescription found => this is guides => shouldn't create hairs
		if (connectionNode_typename == "xgmSplineDescription")
			return true;
	}

	return false;
}

bool FireRenderContext::AddSceneObject(const MDagPath& dagPath)
{
	MAIN_THREAD_ONLY;
	using namespace FireMaya;

	FireRenderObject* ob = nullptr;
	MObject node = dagPath.node();

	bool volumeSupported = IsVolumeSupported();
	bool hairSupported = IsHairSupported();

	if (node.hasFn(MFn::kDagNode))
	{
		MFnDagNode dagNode(node);
		MDagPath dagPathTmp;

		if (isGeometry(node))
		{
			ob = CreateSceneObject<FireRenderMesh, NodeCachingOptions::AddPath>(dagPath);
		}
		else if (dagNode.typeId() == TypeId::FireRenderIESLightLocator
			|| isLight(node)
			|| VRay::isNonEnvironmentLight(dagNode))
		{
			if (dagNode.typeName() == "ambientLight") 
			{
				ob = CreateSceneObject<FireRenderEnvLight, NodeCachingOptions::AddPath>(dagPath);
			}
			else if (dagNode.typeId() == FireMaya::TypeId::FireRenderPhysicalLightLocator)
			{
				ob = CreateSceneObject<FireRenderPhysLight, NodeCachingOptions::AddPath>(dagPath);
			}
			else
			{
				ob = CreateSceneObject<FireRenderLight, NodeCachingOptions::AddPath>(dagPath);
			}
		}
		else if (dagNode.typeId() == TypeId::FireRenderIBL
			|| dagNode.typeId() == TypeId::FireRenderEnvironmentLight
			|| dagNode.typeName() == "ambientLight"
			|| VRay::isEnvironmentLight(dagNode))
		{
			ob = CreateEnvLight(dagPath);
		}
		else if (dagNode.typeId() == TypeId::FireRenderSkyLocator
			|| VRay::isSkyLight(dagNode))
		{
			ob = CreateSky(dagPath);
		}
		else if (dagNode.typeName() == "fluidShape" && volumeSupported)
		{
			// Maya native volume
			ob = CreateSceneObject<FireRenderFluidVolume, NodeCachingOptions::AddPath>(dagPath);
		}
		else if (dagNode.typeName() == "RPRVolume" && volumeSupported)
		{
			// can read .vdb files
			ob = CreateSceneObject<FireRenderRPRVolume, NodeCachingOptions::AddPath>(dagPath);
		}
		else if (dagNode.typeName() == "instancer")
		{
			m_LateinitMASHInstancers.push_back(dagPath);
		}
		else if (dagNode.typeName() == "xgmSplineDescription")
		{
			if (!IsXgenGuides(node) && hairSupported)
			{
				// check if xgmSplineDescription is a guides for other xgmSplineDescription
				// don't need to create hair object if that is the case
				ob = CreateSceneObject<FireRenderHairXGenGrooming, NodeCachingOptions::AddPath>(dagPath);
			}
			else if (dagNode.typeName() == "HairShape" && hairSupported)
			{
				// check if xgmSplineDescription is a guides for other xgmSplineDescription
				// don't need to create hair object if that is the case
				ob = CreateSceneObject<FireRenderHairXGenGrooming, NodeCachingOptions::AddPath>(dagPath);
			}
		}
		else if (dagNode.typeName() == "HairShape" && hairSupported)
		{
			ob = CreateSceneObject<FireRenderHairOrnatrix, NodeCachingOptions::AddPath>(dagPath);
		}
		else if (isTransformWithInstancedShape(node, dagPathTmp))
		{
			ob = CreateSceneObject<FireRenderMesh, NodeCachingOptions::DontAddPath>(dagPathTmp);
		}
		else if (dagNode.typeName() == "pfxHair" && hairSupported)
		{
			ob = CreateSceneObject<FireRenderHairNHair, NodeCachingOptions::AddPath>(dagPath);
		}
		else if (dagNode.typeName() == "transform")
		{
			ob = CreateSceneObject<FireRenderNode, NodeCachingOptions::AddPath>(dagPath);
		}
		else
		{
			DebugPrint("Ignoring %s: %s", dagNode.typeName().asUTF8(), dagNode.name().asUTF8());
		}
	}
	else
	{
		MFnDependencyNode depNode(node);
		DebugPrint("Ignoring %s: %s", depNode.typeName().asUTF8(), depNode.name().asUTF8());
	}

	return !!ob;
}

FireRenderEnvLight* FireRenderContext::CreateEnvLight(const MDagPath& dagPath)
{
	return CreateSceneObject<FireRenderEnvLight, NodeCachingOptions::AddPath>(dagPath);
}

FireRenderSky* FireRenderContext::CreateSky(const MDagPath& dagPath)
{
	return CreateSceneObject<FireRenderSky, NodeCachingOptions::AddPath>(dagPath);
}

void FireRenderContext::setDirty()
{
	m_dirty = true;
}


bool FireRenderContext::isDirty()
{
	return m_dirty || (m_dirtyObjects.size() != 0) || m_cameraDirty || m_tonemappingChanged;
}

bool FireRenderContext::needsRedraw(bool setToFalseOnExit)
{
	bool value = m_needRedraw;

	if (setToFalseOnExit)
		m_needRedraw = false;

	return value;
}

void FireRenderContext::setDirtyObject(FireRenderObject* obj)
{
	if (obj == &m_camera)
	{
		m_cameraDirty = true;
		return;
	}

	// We should skip inactive cameras, because their changes shouldn't affect result image
	// If ignore this step - image in IPR would redraw when moving different camera in viewport
	// That image redrawing in IPR causes black square artifats
	MItDag itDag;
	MStatus status = itDag.reset(obj->Object(), MItDag::kDepthFirst, MFn::kCamera);
	CHECK_MSTATUS(status);
	for (; !itDag.isDone(); itDag.next())
	{
		return;
	}

	AutoMutexLock lock(m_dirtyMutex);

	// Find the object in objects list
	{
		auto it = m_sceneObjects.find(obj->uuid());
		if (it != m_sceneObjects.end())
		{
			std::shared_ptr<FireRenderObject> ptr = it->second;
			m_dirtyObjects[obj] = ptr;
		}
	}
}

HashValue FireRenderContext::GetStateHash()
{
	HashValue hash(size_t(this));

	for (auto& it : m_sceneObjects)
	{
		if (it.second)
			hash << it.second->GetStateHash();
	}

	hash << m_camera.GetStateHash();

	return hash;
}

void FireRenderContext::UpdateTimeAndTriggerProgressCallback(ContextWorkProgressData& syncProgressData, ProgressType progressType)
{
	if (progressType != ContextWorkProgressData::ProgressType::Unknown)
	{
		syncProgressData.progressType = progressType;
	}

	syncProgressData.currentTimeInMiliseconds = TimeDiff(clock(), m_workStartTime);
	TriggerProgressCallback(syncProgressData);
}

void FireRenderContext::TriggerProgressCallback(const ContextWorkProgressData& syncProgressData)
{
	if (m_WorkProgressCallback)
	{
		m_WorkProgressCallback(syncProgressData);
	}
}

bool FireRenderContext::Freshen(bool lock, std::function<bool()> cancelled)
{
	MAIN_THREAD_ONLY;

	if (!isDirty() || cancelled())
		return false;

	LOCKFORUPDATE((lock ? this : nullptr));

	m_inRefresh = true;

	updateFromGlobals(false /*applyLock*/);

	decltype(m_addedNodes) addedNodes = m_addedNodes;
	m_addedNodes.clear();

	for (MObject& node : addedNodes)
		addNode(node);

	if (cancelled())
		return false;

	decltype(m_removedNodes) removedNodes = m_removedNodes;
	m_removedNodes.clear();

	for (MObject& node : removedNodes)
		removeNode(node);

	//Should be called when all scene objects are updated
	BuildLateinitObjects();

	bool changed = m_dirty;

	if (m_cameraDirty)
	{
		m_cameraDirty = false;
		m_camera.Freshen();
		changed = true;
	}

	size_t dirtyObjectsSize = m_dirtyObjects.size();

	ContextWorkProgressData syncProgressData;
	syncProgressData.totalCount = dirtyObjectsSize;
	time_t syncStartTime = clock();

	UpdateTimeAndTriggerProgressCallback(syncProgressData, ProgressType::SyncStarted);

	for (auto it = m_dirtyObjects.begin(); it != m_dirtyObjects.end(); )
	{
		if ((m_state != FireRenderContext::StateRendering) && (m_state != FireRenderContext::StateUpdating))
			break;

		// Request the object with removal it from the dirty list. Use mutex to prevent list's modifications.
		m_dirtyMutex.lock();
		
		std::shared_ptr<FireRenderObject> ptr = it->second.lock();

		it = m_dirtyObjects.erase(it);

		m_dirtyMutex.unlock();

		// Now perform update
		if (ptr)
		{
			changed = true;
			DebugPrint("Freshing object");

			UpdateTimeAndTriggerProgressCallback(syncProgressData, ProgressType::ObjectPreSync);
			ptr->Freshen();

			syncProgressData.currentIndex++;
			UpdateTimeAndTriggerProgressCallback(syncProgressData, ProgressType::ObjectSyncComplete);

			if (cancelled())
			{
				return false;
			}
		}
		else
		{
			DebugPrint("Cancelled freshing null object");
		}
	}

	syncProgressData.elapsed = TimeDiff(clock(), syncStartTime);
	UpdateTimeAndTriggerProgressCallback(syncProgressData, ProgressType::SyncComplete);

	if (changed)
	{
		UpdateDefaultLights();
		setCameraAttributeChanged(true);
	}

	setPreview();

	if (cancelled())
		return false;

	updateRenderLayers();

	m_dirty = false;

	auto hash = GetStateHash();
	DebugPrint("Hash Value: %08X", int(hash));

	m_inRefresh = false;

	m_needRedraw = true;

	return true;
}

void FireRenderContext::SetState(StateEnum newState)
{
	if (m_state == newState)
	{
		return;
	}

	m_state = newState;

	if (m_state == StateEnum::StateExiting)
	{
		ContextWorkProgressData data;
		data.elapsed = TimeDiff(clock(), m_renderStartTime);

		UpdateTimeAndTriggerProgressCallback(data, ProgressType::RenderComplete);
	}
}

bool FireRenderContext::setUseRegion(bool value)
{
	m_useRegion = value && IsRenderRegionSupported();

	return m_useRegion;
}

bool FireRenderContext::useRegion()
{
	return m_useRegion;
}

void FireRenderContext::setRenderRegion(const RenderRegion & region)
{
	m_region = region;
}

RenderRegion FireRenderContext::renderRegion()
{
	return m_region;
}

void FireRenderContext::setCallbackCreationDisabled(bool value)
{
	m_callbackCreationDisabled = value;
}

bool FireRenderContext::getCallbackCreationDisabled()
{
	return m_callbackCreationDisabled;
}

bool FireRenderContext::renderSelectedObjectsOnly() const
{
	return m_renderSelectedObjectsOnly;
}

bool FireRenderContext::motionBlur() const
{
	return !isInteractive() ? m_motionBlur : m_viewportMotionBlur;
}

bool FireRenderContext::cameraMotionBlur() const
{
	return !isInteractive() ? m_cameraMotionBlur : m_viewportMotionBlur;
}

float FireRenderContext::motionBlurCameraExposure() const
{
	return m_motionBlurCameraExposure;
}

void FireRenderContext::setCameraAttributeChanged(bool value)
{
	m_cameraAttributeChanged = value;
	if (value)
		m_restartRender = true;
}

void FireRenderContext::setCompletionCriteria(const CompletionCriteriaParams& completionCriteriaParams)
{
	m_completionCriteriaParams = completionCriteriaParams;
}

const CompletionCriteriaParams& FireRenderContext::getCompletionCriteria(void) const
{
	return m_completionCriteriaParams;
}

bool FireRenderContext::isUnlimited()
{
	// Corresponds to the kUnlimited enum in FireRenderGlobals.
	return m_completionCriteriaParams.isUnlimited();
}

void FireRenderContext::setStartedRendering()
{
	m_renderStartTime = clock();
	m_currentIteration = 0;
}

bool FireRenderContext::keepRenderRunning()
{
	// Check if adaptive sampling finished it's work.
	// At zero iterarion there's no render info for active pixel count.
	if (m_currentIteration != 0 && GetScope().Context().IsAdaptiveSamplingFinalized())
	{
		return false;
	}

	// check iteration count completion criteria
	if (!m_completionCriteriaParams.isUnlimitedIterations() &&
		m_currentIteration >= m_completionCriteriaParams.completionCriteriaMaxIterations)
	{
		return false;
	}

	if (m_completionCriteriaParams.isUnlimitedTime())
	{
		return true;
	}

	// check time limit completion criteria
	long numberOfClicks = clock() - m_renderStartTime;
	double secondsSpentRendering = numberOfClicks / (double)CLOCKS_PER_SEC;

	return secondsSpentRendering < m_completionCriteriaParams.getTotalSecondsCount();
}

bool FireRenderContext::isFirstIterationAndShadersNOTCached() 
{
    if (isMetalOn())
    {
        // Metal does not cache shaders the way OCL does
        return false;
    }

	if (m_currentIteration == 0)
    {
		return !areShadersCached();
	}
	return false;
}

void FireRenderContext::updateProgress()
{
	long numberOfClocks = clock() - m_renderStartTime;
	double secondsSpentRendering = numberOfClocks / (double)CLOCKS_PER_SEC;

	m_secondsSpentOnLastRender = secondsSpentRendering;

	if (m_completionCriteriaParams.isUnlimited())
	{
		m_progress = 0;
	}
	else if (!m_completionCriteriaParams.isUnlimitedIterations())
	{
		double iterationState = m_currentIteration / (double)m_completionCriteriaParams.completionCriteriaMaxIterations;
		m_progress = static_cast<int>(ceil(iterationState * 100));
	}
	else
	{
		double timeState = secondsSpentRendering / m_completionCriteriaParams.getTotalSecondsCount();
		m_progress = static_cast<int>(ceil(timeState * 100));
	}
}

int FireRenderContext::getProgress()
{
	return std::min(m_progress, 100);
}

void FireRenderContext::setProgress(int percents)
{
	m_progress = std::min(percents, 100);
}



void FireRenderContext::doOutputFromComposites(const ReadFrameBufferRequestParams& params, size_t dataSize, const frw::FrameBuffer& frameBufferOut)
{
	// Find the number of pixels in the frame buffer.
	int pixelCount = params.PixelCount();
	// Check that the reported frame buffer size
	// in bytes matches the required dimensions.
	assert(dataSize == (sizeof(RV_PIXEL) * pixelCount));

	// A temporary pixel buffer is required if the region is less
	// than the full width and height, or the image should be flipped.
	bool useTempData = params.UseTempData();
	if (useTempData)
		m_tempData.resize(pixelCount);

	RV_PIXEL* data = useTempData ? m_tempData.get() : params.pixels;
	rpr_int frstatus = rprFrameBufferGetInfo(frameBufferOut.Handle(), RPR_FRAMEBUFFER_DATA, dataSize, &data[0], nullptr);
	checkStatus(frstatus);

	if (useTempData)
	{
		copyPixels(params.pixels, data, params.width, params.height, params.region, params.flip);
	}
}

void ShadowCatcherOutDbg(
	unsigned int width,
	unsigned int height, 
	frw::Context& context,
	const RprComposite& compositeToSave,
	std::string name
	)
{
#define SHADOWCATCHERDEBUG
#ifdef SHADOWCATCHERDEBUG
	rpr_framebuffer frameBufferOutDbg = 0;
	rpr_framebuffer_format fmtOutDbg = { 4, RPR_COMPONENT_TYPE_FLOAT32 };
	rpr_framebuffer_desc descOutDbg;
	descOutDbg.fb_width = width;
	descOutDbg.fb_height = height;

	rpr_int frstatus = rprContextCreateFrameBuffer(context.Handle(), fmtOutDbg, &descOutDbg, &frameBufferOutDbg);
	checkStatus(frstatus);
	frstatus = rprCompositeCompute(compositeToSave, frameBufferOutDbg);
	checkStatus(frstatus);

	std::string outputName ("C:/temp/dbg/"); 
	outputName += name;
	outputName += ".png";
	frstatus = rprFrameBufferSaveToFile(frameBufferOutDbg, outputName.c_str());
#endif
}

size_t GetDataSize(rpr_framebuffer frameBuffer)
{
	// Get data from the RPR frame buffer
	size_t dataSize;
	rpr_int frstatus = rprFrameBufferGetInfo(frameBuffer, RPR_FRAMEBUFFER_DATA, 0, nullptr, &dataSize);
	checkStatus(frstatus);
	return dataSize;
}

frw::FrameBuffer GetOutFrameBuffer(const FireRenderContext::ReadFrameBufferRequestParams& params, frw::Context& context)
{
	rpr_framebuffer_format fmtOut = { 4, RPR_COMPONENT_TYPE_FLOAT32 };
	frw::FrameBuffer frameBufferOut(context, params.width, params.height, fmtOut);
	return frameBufferOut;
}

// -----------------------------------------------------------------------------
void FireRenderContext::compositeShadowCatcherOutput(const ReadFrameBufferRequestParams& params)
{
	RPR_THREAD_ONLY;

	// get data from frame buffers
	rpr_framebuffer colorFrameBuffer = frameBufferAOV_Resolved(RPR_AOV_COLOR);
	rpr_framebuffer opacityFrameBuffer = frameBufferAOV_Resolved(RPR_AOV_OPACITY);
	rpr_framebuffer shadowCatcherFrameBuffer = frameBufferAOV_Resolved(RPR_AOV_SHADOW_CATCHER);
	rpr_framebuffer backgroundFrameBuffer = frameBufferAOV_Resolved(RPR_AOV_BACKGROUND);

	// inputs
	frw::Context context = GetContext();
	CompositeWrapper noAlpha(context, 1.0f, 0.0f); // Input Wrapper
	CompositeWrapper color(context, colorFrameBuffer);
	CompositeWrapper opacity(context, opacityFrameBuffer);
	CompositeWrapper shadowCatcher(context, shadowCatcherFrameBuffer);
	CompositeWrapper shadowColor(context, params.shadowColor[0], params.shadowColor[1], params.shadowColor[2], 1.0f);
	CompositeWrapper const1(context, 1.0f);
	CompositeWrapper shadowTransp(context, 1.0f*params.shadowWeight - params.shadowTransp);
	CompositeWrapper background(context, backgroundFrameBuffer);
	CompositeWrapper backgroundTransp(context, 1.0f*params.bgWeight - params.bgTransparency);
	CompositeWrapper backgroundColor(context, params.bgColor[0], params.bgColor[1], params.bgColor[2], 1.0f);

	// background * (1-min(alpha+sc*shadowTransp*(1-shadowColor), 1)) + color*alpha 
	CompositeWrapper step1 = noAlpha * opacity + noAlpha * shadowCatcher * shadowTransp * (const1 - shadowColor);
	CompositeWrapper step2 = const1 - CompositeWrapper::min(step1, const1);
	CompositeWrapper res = background * backgroundTransp * backgroundColor * step2 + noAlpha * color * opacity;

	// write result to output
	frw::FrameBuffer frameBufferOut = GetOutFrameBuffer(params, context);
	res.Compute(frameBufferOut);
	doOutputFromComposites(params, GetDataSize(colorFrameBuffer), frameBufferOut);
}

// -----------------------------------------------------------------------------
void FireRenderContext::compositeReflectionCatcherOutput(const ReadFrameBufferRequestParams& params)
{
	RPR_THREAD_ONLY;

	// get data from frame buffers
	rpr_framebuffer frameBufferColor = frameBufferAOV_Resolved(RPR_AOV_COLOR);
	rpr_framebuffer opacityFrameBuffer = frameBufferAOV_Resolved(RPR_AOV_OPACITY);
	rpr_framebuffer reflectionCatcherFrameBuffer = frameBufferAOV_Resolved(RPR_AOV_REFLECTION_CATCHER);
	rpr_framebuffer backgroundFrameBuffer = frameBufferAOV_Resolved(RPR_AOV_BACKGROUND);

	// inputs
	frw::Context context = GetContext();
	CompositeWrapper noAlpha(context, 1.0f, 0.0f);
	CompositeWrapper color(context, frameBufferColor);
	CompositeWrapper opacity(context, opacityFrameBuffer);
	CompositeWrapper reflectionCatcher(context, reflectionCatcherFrameBuffer);
	CompositeWrapper const1(context, 1.0f);
	CompositeWrapper background(context, backgroundFrameBuffer);
	CompositeWrapper backgroundTransp(context, 1.0f*params.bgWeight - params.bgTransparency);
	CompositeWrapper backgroundColor(context, params.bgColor[0], params.bgColor[1], params.bgColor[2], 1.0f);

	// background * (1-alpha) + color * (alpha+rc)
	CompositeWrapper step1 = const1 - noAlpha * opacity;
	CompositeWrapper step2 = background * backgroundTransp * backgroundColor * step1;
	CompositeWrapper res = step2 + color * (noAlpha * opacity + reflectionCatcher);

	// write result to output
	frw::FrameBuffer frameBufferOut = GetOutFrameBuffer(params, context);
	res.Compute(frameBufferOut);
	doOutputFromComposites(params, GetDataSize(frameBufferColor), frameBufferOut);
}

void FireRenderContext::compositeReflectionShadowCatcherOutput(const ReadFrameBufferRequestParams& params)
{
	RPR_THREAD_ONLY;
	
	// get data from frame buffers
	rpr_framebuffer frameBufferColor = frameBufferAOV_Resolved(RPR_AOV_COLOR);
	rpr_framebuffer opacityFrameBuffer = frameBufferAOV_Resolved(RPR_AOV_OPACITY);
	rpr_framebuffer reflectionCatcherFrameBuffer = frameBufferAOV_Resolved(RPR_AOV_REFLECTION_CATCHER);
	rpr_framebuffer backgroundFrameBuffer = frameBufferAOV_Resolved(RPR_AOV_BACKGROUND);
	rpr_framebuffer shadowCatcherFrameBuffer = frameBufferAOV_Resolved(RPR_AOV_SHADOW_CATCHER);

	// inputs
	frw::Context context = GetContext();
	CompositeWrapper noAlpha(context, 1.0f, 0.0f);
	CompositeWrapper color(context, frameBufferColor);
	CompositeWrapper opacity(context, opacityFrameBuffer);
	CompositeWrapper reflectionCatcher(context, reflectionCatcherFrameBuffer);
	CompositeWrapper shadowCatcher(context, shadowCatcherFrameBuffer);
	CompositeWrapper shadowColor(context, params.shadowColor[0], params.shadowColor[1], params.shadowColor[2], 1.0f);
	CompositeWrapper const1(context, 1.0f);
	CompositeWrapper shadowTransp(context, 1.0f*params.shadowWeight - params.shadowTransp);
	CompositeWrapper background(context, backgroundFrameBuffer);
	CompositeWrapper backgroundTransp(context, 1.0f*params.bgWeight - params.bgTransparency);
	CompositeWrapper backgroundColor(context, params.bgColor[0], params.bgColor[1], params.bgColor[2], 1.0f);

	// background * (1-min(alpha+sc, 1)) + color*(alpha+rc)
	CompositeWrapper step1 = noAlpha * opacity + noAlpha * shadowCatcher * shadowTransp * (const1 - shadowColor);
	CompositeWrapper step2 = const1 - CompositeWrapper::min(step1, const1);
	CompositeWrapper step3 = color * (noAlpha * opacity + reflectionCatcher);
	CompositeWrapper res = background * backgroundTransp * backgroundColor * step2 + step3;

	// write result to output
	frw::FrameBuffer frameBufferOut = GetOutFrameBuffer(params, context);
	res.Compute(frameBufferOut);
	doOutputFromComposites(params, GetDataSize(frameBufferColor), frameBufferOut);
}

RenderType FireRenderContext::GetRenderType() const
{
	return m_RenderType;
}

void FireRenderContext::SetRenderType(RenderType renderType)
{
	m_RenderType = renderType;

	if ((m_RenderType == RenderType::ViewportRender) ||
		(m_RenderType == RenderType::IPR))
	{
		m_interactive = true;
	}
}

bool FireRenderContext::ShouldResizeTexture(unsigned int& max_width, unsigned int& max_height) const
{
	if (GetRenderType() == RenderType::Thumbnail)
	{
		max_width = FireRenderMaterialSwatchRender::MaterialSwatchPreviewTextureSize;
		max_height = FireRenderMaterialSwatchRender::MaterialSwatchPreviewTextureSize;
		return true;
	}

	return false;
}

frw::Shader FireRenderContext::GetShader(MObject ob, const FireRenderMesh* pMesh, bool forceUpdate)
{ 
	scope.SetContextInfo(this);

	MFnDependencyNode node(ob);

	frw::Shader shader = scope.GetShader(ob, pMesh, forceUpdate);

	shader.SetName(node.name().asChar());

	return shader;
}

void FireRenderContext::enableAOV(int aov, bool flag)
{
	if (IsAOVSupported(aov))
	{
		aovEnabled[aov] = flag;
	}
}

bool FireRenderContext::isAOVEnabled(int aov) const 
{
	return aovEnabled[aov];
}

frw::Shader FireRenderContext::GetDefaultColorShader(frw::Value color)
{
	frw::DiffuseShader shader(GetMaterialSystem());
	shader.SetColor(color);

	return shader;
}
