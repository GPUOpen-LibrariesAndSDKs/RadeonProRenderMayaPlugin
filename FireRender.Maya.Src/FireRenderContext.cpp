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

#include "AutoLock.h"
#include "VRay.h"
#include "ImageFilter/ImageFilter.h"

#include "RprComposite.h"

#include "FireRenderThread.h"
#include "FireRenderMaterialSwatchRender.h"

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

#ifdef OSMac_
#include <OpenImageIO/imageio.h>
#else
#include <imageio.h>
#endif


#ifndef MAYA2015
#include <maya/MUuid.h>
#endif
#include "FireRenderIBL.h"
#include <maya/MFnRenderLayer.h>

rpr_int g_tahoePluginID = -1;

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

FireRenderContext::FireRenderContext() :
	m_transparent(NULL),
	m_width(0),
	m_height(0),
	m_useRegion(false),
	m_motionBlur(false),
	m_cameraAttributeChanged(false),
	m_startTime(0),
	m_samplesPerUpdate(1),
	m_currentIteration(0),
	m_progress(0),
	m_lastIterationTime(0),
	m_timeIntervalForOutputUpdate(0.1),
	m_interactive(false),
	m_camera(this, MDagPath()),
	m_glInteropActive(false),
	m_globalsChanged(false),
	m_renderLayersChanged(false),
	m_cameraDirty(true),
	m_denoiserChanged(false),
	m_denoiserFilter(nullptr),
	m_RenderType(RenderType::Undefined)
{
	DebugPrint("FireRenderContext::FireRenderContext()");

	state = StateUpdating;
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

	// Here we have buffers setup with proper size, lets setup denoiser if needed
	if (m_globals.denoiserSettings.enabled && ((m_RenderType == RenderType::ProductionRender) || (m_RenderType == RenderType::IPR)))
	{
		setupDenoiser();
	}
}

void FireRenderContext::resetAOV(int index, rpr_GLuint* glTexture)
{
	auto context = scope.Context();

	initBuffersForAOV(context, index, glTexture);
}

void FireRenderContext::enableAOVAndReset(int index, bool flag)
{
	enableAOV(index, flag);
	resetAOV(index, nullptr);
}

void FireRenderContext::initBuffersForAOV(frw::Context& context, int index, rpr_GLuint* glTexture)
{
	rpr_framebuffer_format fmt = { 4, RPR_COMPONENT_TYPE_FLOAT32 };

	m.framebufferAOV[index].Reset();
	m.framebufferAOV_resolved[index].Reset();

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
		context.SetAOV(nullptr, index);
	}
}

void FireRenderContext::enableDisplayGammaCorrection(const FireRenderGlobalsData &globals)
{
	RPR_THREAD_ONLY;
	GetContext().SetParameter("displaygamma", globals.displayGamma);
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

bool FireRenderContext::buildScene(bool animation, bool isViewport, bool glViewport, bool freshen)
{
	MAIN_THREAD_ONLY;
	DebugPrint("FireRenderContext::buildScene()");

	m_globals.readFromCurrentScene();

	if (m_globals.denoiserSettings.enabled)
	{
		turnOnAOVsForDenoiser();
	}

	auto createFlags = FireMaya::Options::GetContextDeviceFlags(m_RenderType);

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

		m_transparent = frw::TransparentShader(GetMaterialSystem());
		m_transparent.SetValue("color", 1.0f);

		updateLimitsFromGlobalData(m_globals);
		m_globals.setupContext(*this);

		setMotionBlur(m_globals.motionBlur);

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

		attachCallbacks();
	}

	if(freshen)
		Freshen();

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

#if defined(LINUX) || defined(OSMac_)
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
			break;
		}
	}

	std::transform(gpuName.begin(), gpuName.end(), gpuName.begin(), ::tolower);

	if (gpuName.find("amd") != std::string::npos || gpuName.find("radeon") != std::string::npos)
		canCreateAiDenoiser = true;

	return canCreateAiDenoiser;
}
#endif

void FireRenderContext::setupDenoiser()
{
	const rpr_framebuffer fbColor = m.framebufferAOV_resolved[RPR_AOV_COLOR].Handle();
	const rpr_framebuffer fbShadingNormal = m.framebufferAOV[RPR_AOV_SHADING_NORMAL].Handle();
	const rpr_framebuffer fbDepth = m.framebufferAOV[RPR_AOV_DEPTH].Handle();
	const rpr_framebuffer fbWorldCoord = m.framebufferAOV[RPR_AOV_WORLD_COORDINATE].Handle();
	const rpr_framebuffer fbObjectId = m.framebufferAOV[RPR_AOV_OBJECT_ID].Handle();
	const rpr_framebuffer fbTrans = fbObjectId;
	const rpr_framebuffer fbDiffuseAlbedo = m.framebufferAOV_resolved[RPR_AOV_DIFFUSE_ALBEDO].Handle();

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
			m_denoiserFilter->CreateFilter(RifFilterType::MlDenoise, useOpenImageDenoise);
			m_denoiserFilter->AddInput(RifColor, fbColor, 0.0f);
			m_denoiserFilter->AddInput(RifNormal, fbShadingNormal, 0.0f);
			m_denoiserFilter->AddInput(RifDepth, fbDepth, 0.0f);
			m_denoiserFilter->AddInput(RifAlbedo, fbDiffuseAlbedo, 0.0f);
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

	GetContext().SetParameter("rendermode", renderMode);
	setDirty();
}

void FireRenderContext::setPreview()
{
	int preview = m_interactive || (m_RenderType == RenderType::Thumbnail); 
	GetContext().SetParameter("preview", preview);
}

void FireRenderContext::cleanScene()
{
	FireRenderThread::RunOnceProcAndWait([this]()
	{
		DebugPrint("FireRenderContext::cleanScene()");
		LOCKMUTEX(this);
		removeCallbacks();

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
	m_globals.setupContext(*this);

	CompletionCriteriaParams completionParams;
	int iterations = FireRenderGlobalsData::getThumbnailIterCount();
	completionParams.completionCriteriaMaxIterations = iterations;
	completionParams.completionCriteriaMinIterations = iterations;

	// Time will be infinite. Left all "hours", "minutes" and "seconds" as 0.

	setSamplesPerUpdate(iterations);
	setCompletionCriteria(completionParams);

	setPreview();
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
		for (int i = 0; i != RPR_AOV_MAX; ++i) {
			if (aovEnabled[i]){
				m.framebufferAOV[i].Clear();
				m.framebufferAOV_resolved[i].Clear();
			}
		}

		if (m_interactive && m_denoiserChanged && m_globals.denoiserSettings.enabled)
		{
			turnOnAOVsForDenoiser(true);
			setupDenoiser();
			m_denoiserChanged = false;
		}

		m_restartRender = false;
		m_startTime = clock();
		m_currentIteration = 0;
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

	context.SetParameter("iterations", iterationStep);

	if (m_useRegion)
		context.RenderTile(m_region.left, m_region.right, m_height - m_region.top - 1, m_height - m_region.bottom - 1);
	else
		context.Render();

	if (m_currentIteration == 0)
	{
		DebugPrint("RPR GPU Memory used: %dMB", context.GetMemoryUsage() >> 20);
	}

	m_currentIteration += iterationStep;

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

unsigned int FireRenderContext::width()
{
	return m_width;
}

unsigned int FireRenderContext::height()
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

	auto cachePath = getShaderCachePath();

	if (g_tahoePluginID == -1)
	{
#ifdef OSMac_
		g_tahoePluginID = rprRegisterPlugin("/Users/Shared/RadeonProRender/lib/libTahoe64.dylib");
#elif __linux__
		g_tahoePluginID = rprRegisterPlugin("libTahoe64.so");
#else
		g_tahoePluginID = rprRegisterPlugin("Tahoe64.dll");
#endif
	}

	if (g_tahoePluginID == -1)
	{
		MGlobal::displayError("Unable to register Radeon ProRender plug-in.");
		return false;
	}

	rpr_int plugins[] = { g_tahoePluginID };
	size_t pluginCount = sizeof(plugins) / sizeof(plugins[0]);

	bool useThread = (createFlags & RPR_CREATION_FLAGS_ENABLE_CPU) == RPR_CREATION_FLAGS_ENABLE_CPU;

	DebugPrint("* Creating Context: %d (0x%x) - useThread: %d", createFlags, createFlags, useThread);

	rpr_context context = nullptr;
    if (isMetalOn() && !(createFlags & RPR_CREATION_FLAGS_ENABLE_CPU))
    {
        createFlags = createFlags | RPR_CREATION_FLAGS_ENABLE_METAL;
    }

	// setup CPU thread count
	std::vector<rpr_context_properties> ctxProperties;

	ctxProperties.push_back((rpr_context_properties)RPR_CONTEXT_CREATEPROP_SAMPLER_TYPE);
	ctxProperties.push_back((rpr_context_properties)RPR_CONTEXT_SAMPLER_TYPE_CMJ);

	int threadCountToOverride = getThreadCountToOverride();
	if ((createFlags & RPR_CREATION_FLAGS_ENABLE_CPU) && threadCountToOverride > 0)
	{
		ctxProperties.push_back ((rpr_context_properties) RPR_CONTEXT_CREATEPROP_CPU_THREAD_LIMIT);
		ctxProperties.push_back ((rpr_context_properties) threadCountToOverride);
	}

	ctxProperties.push_back( (rpr_context_properties) 0);

#ifdef RPR_VERSION_MAJOR_MINOR_REVISION
	int res = rprCreateContext(RPR_VERSION_MAJOR_MINOR_REVISION, plugins, pluginCount, createFlags, ctxProperties.data(), cachePath.asUTF8(), &context);
#else
	int res = rprCreateContext(RPR_API_VERSION, plugins, pluginCount, createFlags, ctxProperties.data(), cachePath.asUTF8(), &context);
#endif

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
			m_glInteropActive = !useCPU;

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
	if (needResolve())
	{
		//resolve tone mapping
		m.framebufferAOV[aov].Resolve(m.framebufferAOV_resolved[aov]);
		fb = m.framebufferAOV_resolved[aov];
	}
	else
	{
		fb = m.framebufferAOV[aov];
	}
	return fb.Handle();
}

// -----------------------------------------------------------------------------
void FireRenderContext::readFrameBuffer(RV_PIXEL* pixels, int aov,
	unsigned int width, unsigned int height, const RenderRegion& region,
	bool flip, bool mergeOpacity, bool mergeShadowCatcher)
{
	RPR_THREAD_ONLY;

	/**
	 * Shadow catcher can work only if COLOR, BACKGROUND, OPACITY and SHADOW_CATCHER AOVs turned on.
	 * If all of them turned on and shadow catcher requested - run composite pipeline.
	 */
	if ( (aov == RPR_AOV_COLOR) && 
		mergeShadowCatcher && 
		m.framebufferAOV[RPR_AOV_SHADOW_CATCHER] &&
		m.framebufferAOV[RPR_AOV_BACKGROUND] &&
		m.framebufferAOV[RPR_AOV_OPACITY] &&
		scope.GetShadowCatcherShader()
		)
	{
		compositeOutput(pixels, width, height, region, flip);
		return;
	}

	// A temporary pixel buffer is required if the region is less
	// than the full width and height, or the image should be flipped.
	bool useTempData = flip || region.getWidth() < width || region.getHeight() < height;

	// Find the number of pixels in the frame buffer.
	int pixelCount = width * height;

	rpr_framebuffer frameBuffer = frameBufferAOV_Resolved(aov);

	RV_PIXEL* data = nullptr;
	rpr_int frstatus = RPR_SUCCESS;
	size_t dataSize;
	std::vector<float> vecData;

	bool isDenoiserEnabled = m_denoiserFilter != nullptr;

	// apply Denoiser
	if (aov == RPR_AOV_COLOR && isDenoiserEnabled)
	{
		try
		{
			m_denoiserFilter->Run();
			vecData = m_denoiserFilter->GetData();

			assert(vecData.size() == pixelCount * 4);

			data = (RV_PIXEL*)&vecData[0];
			dataSize = vecData.size() * sizeof(float);
		}
		catch (std::exception& e)
		{
			m_denoiserFilter.reset();
			ErrorPrint( e.what() );
			MGlobal::displayError("RPR failed to execute denoiser, turning it off.");
			return;
		}
	}
	else
	{
		// Get data from the RPR frame buffer.
		frstatus = rprFrameBufferGetInfo(frameBuffer, RPR_FRAMEBUFFER_DATA, 0, nullptr, &dataSize);
		checkStatus(frstatus);

#ifdef _DEBUG
#ifdef DUMP_PIXELS_SOURCE
		rprFrameBufferSaveToFile(frameBuffer, "C:\\temp\\dbg\\3.png");
#endif
#endif

		// Check that the reported frame buffer size
		// in bytes matches the required dimensions.
		assert(dataSize == (sizeof(RV_PIXEL) * pixelCount));

		// Copy the frame buffer into temporary memory, if
		// required, or directly into the supplied pixel buffer.
		if (useTempData)
			m_tempData.resize(pixelCount);

		data = useTempData ? m_tempData.get() : pixels;
		frstatus = rprFrameBufferGetInfo(frameBuffer, RPR_FRAMEBUFFER_DATA, dataSize, &data[0], nullptr);
		checkStatus(frstatus);
	}

	// No need to merge opacity for any FB other then color
	if (mergeOpacity && aov == RPR_AOV_COLOR)
	{
		rpr_framebuffer opacityFrameBuffer = frameBufferAOV_Resolved(RPR_AOV_OPACITY);
		if (opacityFrameBuffer != nullptr)
		{
			m_opacityData.resize(pixelCount);

			if (useTempData)
			{
				m_opacityTempData.resize(pixelCount);

				frstatus = rprFrameBufferGetInfo(opacityFrameBuffer, RPR_FRAMEBUFFER_DATA, dataSize, m_opacityTempData.get(), nullptr);
				checkStatus(frstatus);

				copyPixels(m_opacityData.get(), m_opacityTempData.get(), width, height, region, flip, false);
			}
			else
			{
				frstatus = rprFrameBufferGetInfo(opacityFrameBuffer, RPR_FRAMEBUFFER_DATA, dataSize, m_opacityData.get(), nullptr);
				checkStatus(frstatus);
			}
		}
	}

	// Copy the region from the temporary
	// buffer into supplied pixel memory.
	if (useTempData || isDenoiserEnabled)
	{
		copyPixels(pixels, data, width, height, region, flip, aov == RPR_AOV_COLOR);
	}

	//combine (Opacity to Alpha)
	// No need to merge opacity for any FB other then color
	if (mergeOpacity && aov == RPR_AOV_COLOR)
	{
		combineWithOpacity(pixels, region.getArea(), m_opacityData.get());
	}
}

#ifdef _DEBUG
void generateBitmapImage(unsigned char *image, int height, int width, int pitch, const char* imageFileName);
#endif

// -----------------------------------------------------------------------------
void FireRenderContext::copyPixels(RV_PIXEL* dest, RV_PIXEL* source,
	unsigned int sourceWidth, unsigned int sourceHeight,
	const RenderRegion& region, bool flip, bool isColor) const
{
	RPR_THREAD_ONLY;
	// Get region dimensions.
	unsigned int regionWidth = region.getWidth();
	unsigned int regionHeight = region.getHeight();

	// A color frame buffer contains pixel data for the full
	// source width and height, even if only a region is rendered.
	if (isColor)
	{
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
		unsigned char* dst2 = buffer2.data();
		generateBitmapImage(dst2, sourceHeight, sourceWidth, sourceWidth * 4, "C:\\temp\\dbg\\2.bmp");
#endif
#endif
	}

	// A non color AOV buffer is the same size as the full frame
	// buffer, but only contains data for the rendered region. This
	// may be due to a bug in the RPR core API.
	else
	{
		// Possibly another bug in the RPR core API, the
		// source data width is one pixel greater than the
		// region width if the left side of the region is zero.
		unsigned int w = regionWidth < sourceWidth ? regionWidth - 1 : regionWidth;

		for (unsigned int y = 0; y < regionHeight; y++)
		{
			unsigned int destIndex = y * regionWidth;
			unsigned int sourceIndex = flip ?
				(regionHeight - y - 1) * w : y * w;

			memcpy(&dest[destIndex], &source[sourceIndex], sizeof(RV_PIXEL) * regionWidth);
		}
	}
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
					bool found = GetNodePath(tmpPath, pRobj->GetUuid());

					if (found)
					{
						m_nodePathCache.erase(pRobj->GetUuid());
					}
				}

				// remove object from main meshes cache
				std::string uuid = getNodeUUid(ob);
				RemoveMainMesh(uuid);

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


frw::Shader FireRenderContext::transparentShader()
{
	return m_transparent;
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
		m_globals.updateTonemapping(*this);

		m_tonemappingChanged = false;
	}

	if (!m_globalsChanged)
		return;

    if (applyLock)
    {
        LOCKFORUPDATE(this);
    }
    
	m_globals.readFromCurrentScene();
	m_globals.setupContext(*this);

	updateLimitsFromGlobalData(m_globals);
	setMotionBlur(m_globals.motionBlur);

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


void FireRenderContext::globalsChangedCallback(MNodeMessage::AttributeMessage msg, MPlug &plug, MPlug &otherPlug, void *clientData)
{
	MAIN_THREAD_ONLY

	DebugPrint("FireRenderContext::globalsChangedCallback(%d, %s, isNull=%d, 0x%x)", msg,  plug.name().asUTF8(), otherPlug.isNull(), clientData);

	if (auto frContext = GetCallbackContext(clientData))
	{
		AutoMutexLock lock(frContext->m_dirtyMutex);

		if (FireRenderGlobalsData::isTonemapping(plug.name()))
		{
			frContext->m_tonemappingChanged = true;
		}
		else
		{
			if (FireRenderGlobalsData::isDenoiser(plug.name()))
			{
				frContext->m_denoiserChanged = true;
			}

			frContext->m_globalsChanged = true;

			frContext->setDirty();
		}
	}
}

void FireRenderContext::setMotionBlur(bool doBlur)
{
	RPR_THREAD_ONLY;

	if (m_motionBlur == doBlur)
		return;

	m_motionBlur = doBlur;
	m_motionBlurCameraExposure = m_globals.motionBlurCameraExposure;
	m_motionBlurScale = m_globals.motionBlurScale;

	for (const auto& it : m_sceneObjects)
	{
		if (auto frMesh = dynamic_cast<FireRenderMesh*>(it.second.get()))
			frMesh->setDirty();
	}
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

bool FireRenderContext::AddSceneObject(const MDagPath& dagPath)
{
	MAIN_THREAD_ONLY;
	using namespace FireMaya;

	FireRenderObject* ob = nullptr;
	MObject node = dagPath.node();

	if (node.hasFn(MFn::kDagNode))
	{
		MFnDagNode dagNode(node);
		MDagPath dagPathTmp;

		if (isGeometry(node))
		{
			ob = CreateSceneObject<FireRenderMesh, NodeCachingOptions::AddPath>(dagPath);
		}
		else if (isTransformWithInstancedShape(node, dagPathTmp))
		{
			ob = CreateSceneObject<FireRenderMesh, NodeCachingOptions::DontAddPath>(dagPathTmp);
		}
		else if (dagNode.typeId() == TypeId::FireRenderIESLightLocator
			|| isLight(node)
			|| VRay::isNonEnvironmentLight(dagNode))
		{
			if (dagNode.typeName() == "ambientLight") 
			{
				ob = CreateSceneObject<FireRenderEnvLight, NodeCachingOptions::AddPath>(dagPath);
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
			ob = CreateSceneObject<FireRenderEnvLight, NodeCachingOptions::AddPath>(dagPath);
		}
		else if (dagNode.typeId() == TypeId::FireRenderSkyLocator
			|| VRay::isSkyLight(dagNode))
		{
			ob = CreateSceneObject<FireRenderSky, NodeCachingOptions::AddPath>(dagPath);
		}
		else if (dagNode.typeName() == "fluidShape")
		{
			ob = CreateSceneObject<FireRenderVolume, NodeCachingOptions::AddPath>(dagPath);
		}
		else if (dagNode.typeName() == "RPRVolume")
		{
			ob = CreateSceneObject<FireRenderRPRVolume, NodeCachingOptions::AddPath>(dagPath);
		}
		else
		{
			MTypeId type_id = dagNode.typeId();
			if (dagNode.typeName() == "xgmSplineDescription")
			{
				ob = CreateSceneObject<FireRenderHair, NodeCachingOptions::AddPath>(dagPath);
			}
			else
			{
				DebugPrint("Ignoring %s: %s", dagNode.typeName().asUTF8(), dagNode.name().asUTF8());
			}
		}
	}
	else
	{
		MFnDependencyNode depNode(node);
		DebugPrint("Ignoring %s: %s", depNode.typeName().asUTF8(), depNode.name().asUTF8());
	}

	return !!ob;
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

	bool changed = m_dirty;

	if (m_cameraDirty)
	{
		m_cameraDirty = false;
		m_camera.Freshen();
		changed = true;
	}

#ifdef OPTIMIZATION_CLOCK
	int overallFreshen = 0;
	timeInInnerAddPolygon = 0;
	overallAddPolygon = 0;
	overallCreateMeshEx = 0;
	timeGetDataFromMaya = 0;
	translateData = 0;
	inTranslateMesh = 0;
	inGetFaceMaterials = 0;
	getTessellatedObj = 0;
	deleteNodes = 0;

	auto start = std::chrono::steady_clock::now();
#endif

	for (auto it = m_dirtyObjects.begin(); it != m_dirtyObjects.end(); )
	{
		if ((state != FireRenderContext::StateRendering) && (state != FireRenderContext::StateUpdating))
			break;

		// Request the object with removal it from the dirty list. Use mutex to prevent list's modifications.
		m_dirtyMutex.lock();
		
		std::shared_ptr<FireRenderObject> ptr = it->second.lock();

		it = m_dirtyObjects.erase(it);

		m_dirtyMutex.unlock();

		// Now perform update
		if (ptr)
		{
#ifdef OPTIMIZATION_CLOCK
			auto start_iter = std::chrono::steady_clock::now();
#endif

			DebugPrint("Freshing object");

			ptr->Freshen();
			changed = true;

#ifdef OPTIMIZATION_CLOCK
			auto end_iter = std::chrono::steady_clock::now();
			auto elapsed_iter = std::chrono::duration_cast<std::chrono::milliseconds>(end_iter - start_iter);
			int ms_iter = elapsed_iter.count();
			overallFreshen += ms_iter;
#endif

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

#ifdef OPTIMIZATION_CLOCK
	auto end = std::chrono::steady_clock::now();
	auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
	int ms = elapsed.count();
	LogPrint("time spent in Freshen = %d ms", overallFreshen);
#endif

	scope.CommitShaders();

#ifdef OPTIMIZATION_CLOCK
	auto after_commit_shd = std::chrono::steady_clock::now();
	LogPrint("time spent in CommitShaders = %d ms", std::chrono::duration_cast<std::chrono::milliseconds>(after_commit_shd - end));
#endif

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

#ifdef OPTIMIZATION_CLOCK
	auto total_end = std::chrono::steady_clock::now();
	LogPrint("time spent in after CommitShaders till end = %d ms", std::chrono::duration_cast<std::chrono::milliseconds>(total_end - after_commit_shd));
	LogPrint("Elapsed time in Translate Mesh: %d ms", inTranslateMesh);
	LogPrint("Elapsed time in AddPolygon: %d ms", overallAddPolygon);
	LogPrint("Elapsed time in CreateMeshEx: %d ms", overallCreateMeshEx);
	LogPrint("Elapsed time in timeGetDataFromMaya: %llu nanoS", timeGetDataFromMaya);
	LogPrint("Elapsed time in innerAddPolygon: %d microS", timeInInnerAddPolygon);
	LogPrint("Elapsed time in translateData: %llu nanoS", translateData);
	LogPrint("Elapsed time in inGetFaceMaterials: %d microS", inGetFaceMaterials);
	LogPrint("Elapsed time in getTessellatedObj: %d microS", getTessellatedObj);
	LogPrint("Elapsed time in deleteNodes: %d microS", deleteNodes);
#endif

	m_inRefresh = false;

	m_needRedraw = true;

	return true;
}

void FireRenderContext::setUseRegion(bool value)
{
	m_useRegion = value;
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
	return m_motionBlur;
}

float FireRenderContext::motionBlurCameraExposure() const
{
	return m_motionBlurCameraExposure;
}

float FireRenderContext::motionBlurScale() const
{
	return m_motionBlurScale;
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

bool FireRenderContext::isUnlimited()
{
	// Corresponds to the kUnlimited enum in FireRenderGlobals.
	return m_completionCriteriaParams.isUnlimited();
}

void FireRenderContext::setStartedRendering()
{
	m_startTime = clock();
	m_currentIteration = 0;
}

bool FireRenderContext::keepRenderRunning()
{
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
	long numberOfClicks = clock() - m_startTime;
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
		long numberOfClocks = clock() - m_startTime;
		double secondsSpentRendering = numberOfClocks / (double)CLOCKS_PER_SEC;
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

bool FireRenderContext::updateOutput()
{
	long numberOfClicks = clock() - m_lastIterationTime;
	double secondsSpentRendering = numberOfClicks / (double)CLOCKS_PER_SEC;
	if (m_timeIntervalForOutputUpdate < secondsSpentRendering) {
		m_lastIterationTime = clock();
		return true;
	}
	else {
		return false;
	}
}

// -----------------------------------------------------------------------------
void FireRenderContext::compositeOutput(RV_PIXEL* pixels, unsigned int width, unsigned int height, const RenderRegion& region, bool flip)
{
	RPR_THREAD_ONLY;
	// A temporary pixel buffer is required if the region is less
	// than the full width and height, or the image should be flipped.
	bool useTempData = flip || region.getWidth() < width || region.getHeight() < height;

	// Find the number of pixels in the frame buffer.
	int pixelCount = width * height;

	rpr_framebuffer frameBuffer = frameBufferAOV_Resolved(RPR_AOV_COLOR);
	rpr_framebuffer opacityFrameBuffer = frameBufferAOV_Resolved(RPR_AOV_OPACITY);
	rpr_framebuffer shadowCatcherFrameBuffer = frameBufferAOV(RPR_AOV_SHADOW_CATCHER);
	rpr_framebuffer backgroundFrameBuffer = frameBufferAOV_Resolved(RPR_AOV_BACKGROUND);

	// Get data from the RPR frame buffer.
	size_t dataSize;
	rpr_int frstatus = rprFrameBufferGetInfo(frameBuffer, RPR_FRAMEBUFFER_DATA, 0, nullptr, &dataSize);
	checkStatus(frstatus);

	// Check that the reported frame buffer size
	// in bytes matches the required dimensions.
	assert(dataSize == (sizeof(RV_PIXEL) * pixelCount));

	auto context = GetContext();
	// Step 1.
	// Combine normalized color, background and opacity AOVs using lerp
	RprComposite compositeBg(context.Handle(), RPR_COMPOSITE_FRAMEBUFFER);
	compositeBg.SetInputFb("framebuffer.input", backgroundFrameBuffer);

	RprComposite compositeColor(context.Handle(), RPR_COMPOSITE_FRAMEBUFFER);
	compositeColor.SetInputFb("framebuffer.input", frameBuffer);

	RprComposite compositeOpacity(context.Handle(), RPR_COMPOSITE_FRAMEBUFFER);
	compositeOpacity.SetInputFb("framebuffer.input", opacityFrameBuffer);

	RprComposite compositeLerp1(context.Handle(), RPR_COMPOSITE_LERP_VALUE);
	compositeLerp1.SetInputC("lerp.color0", compositeBg);
	compositeLerp1.SetInputC("lerp.color1", compositeColor);
	compositeLerp1.SetInputC("lerp.weight", compositeOpacity);

	// Step 2.
	// Combine result from step 1, black color and normalized shadow catcher AOV
	RprComposite compositeShadowCatcher(context.Handle(), RPR_COMPOSITE_FRAMEBUFFER);
	compositeShadowCatcher.SetInputFb("framebuffer.input", shadowCatcherFrameBuffer);

	//Find first shadow catcher shader
	frw::Shader shadowCatcherShader = scope.GetShadowCatcherShader();
	assert(shadowCatcherShader);
	float r = 0.0f;
	float g = 0.0f;
	float b = 0.0f;
	float a = 0.0f;
	shadowCatcherShader.GetShadowColor(&r, &g, &b, &a);

	float weight = shadowCatcherShader.GetShadowWeight();

	RprComposite compositeShadowColor(context.Handle(), RPR_COMPOSITE_CONSTANT);

	// We have "Shadow Transparency" parameter in Shadow Catcher, but not "Shadow opacity"
	// It means we should invert this parameter
	compositeShadowColor.SetInput4f("constant.input", r, g, b, 1.0f - a);

	RprComposite compositeShadowWeight(context.Handle(), RPR_COMPOSITE_CONSTANT);
	compositeShadowWeight.SetInput4f("constant.input", weight, weight, weight, weight);

	RprComposite compositeShadowCatcherNorm(context.Handle(), RPR_COMPOSITE_NORMALIZE);
	compositeShadowCatcherNorm.SetInputC("normalize.color", compositeShadowCatcher);
	compositeShadowCatcherNorm.SetInput1U("normalize.aovtype", RPR_AOV_SHADOW_CATCHER);
	
	RprComposite compositeSCWeight(context.Handle(), RPR_COMPOSITE_ARITHMETIC);
	compositeSCWeight.SetInputC("arithmetic.color0", compositeShadowCatcherNorm);
	compositeSCWeight.SetInputC("arithmetic.color1", compositeShadowWeight);
	compositeSCWeight.SetInputOp("arithmetic.op", RPR_MATERIAL_NODE_OP_MUL);

	RprComposite compositeLerp2(context.Handle(), RPR_COMPOSITE_LERP_VALUE);
	//Setting BgIsEnv to false means that we should use shadow catcher color instead of environment background
	if (shadowCatcherShader.BgIsEnv())
		compositeLerp2.SetInputC("lerp.color0", compositeLerp1);
	else
		compositeLerp2.SetInputC("lerp.color0", compositeColor);
		
	compositeLerp2.SetInputC("lerp.color1", compositeShadowColor);
	compositeLerp2.SetInputC("lerp.weight", compositeSCWeight);

	// Step 3.
	// Compute results from step 2 into separate framebuffer
	rpr_framebuffer frameBufferComposite = 0;
	rpr_framebuffer_format fmt = { 4, RPR_COMPONENT_TYPE_FLOAT32 };
	rpr_framebuffer_desc desc;
	desc.fb_width = width;
	desc.fb_height = height;

	frstatus = rprContextCreateFrameBuffer(context.Handle(), fmt, &desc, &frameBufferComposite);
	checkStatus(frstatus);
	frstatus = rprCompositeCompute(compositeLerp2, frameBufferComposite);
	checkStatus(frstatus);

	// Copy the frame buffer into temporary memory, if
	// required, or directly into the supplied pixel buffer.
	if (useTempData)
		m_tempData.resize(pixelCount);
	RV_PIXEL* data = useTempData ? m_tempData.get() : pixels;
	frstatus = rprFrameBufferGetInfo(frameBufferComposite, RPR_FRAMEBUFFER_DATA, dataSize, &data[0], nullptr);
	checkStatus(frstatus);
	// Copy the region from the temporary
	// buffer into supplied pixel memory.
	if (useTempData)
	{
		copyPixels(pixels, data, width, height, region, flip, true);
	}

	rprObjectDelete(frameBufferComposite);
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

frw::Shader FireRenderContext::GetShader(MObject ob, bool forceUpdate)
{ 
	scope.SetContextInfo(this);

	MFnDependencyNode node(ob);

	frw::Shader shader = scope.GetShader(ob, forceUpdate); 

	shader.SetMaterialName(node.name().asChar());

	return shader;
}
