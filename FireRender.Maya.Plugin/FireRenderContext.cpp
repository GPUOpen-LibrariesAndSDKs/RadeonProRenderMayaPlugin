#include <GL/glew.h>
#include <maya/M3dView.h>
#include "FireRenderContext.h"
#include "FireRenderUtils.h"
#include <cassert>
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
#include <maya/MFnTransform.h>
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MItDependencyNodes.h>
#include <maya/MAnimControl.h>
#include <maya/MFnMatrixData.h>
#include <maya/MQuaternion.h>
#include <maya/MUserEventMessage.h>
#include "AutoLock.h"
#include "VRay.h"
#include <chrono>

#include "RprComposite.h"

#include "FireRenderThread.h"

using namespace RPR;
using namespace FireMaya;

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

#include <FireRenderPortableUtils.h>

rpr_int g_tahoePluginID = -1;

FireRenderContext::FireRenderContext() :
	m_transparent(NULL),
	m_width(0),
	m_height(0),
	m_useRegion(false),
	m_motionBlur(false),
	m_cameraAttributeChanged(false),
	m_startTime(0),
	m_completionType(0),
	m_completionIterations(0),
	m_completionTime(0),
	m_currentIteration(0),
	m_progress(0),
	m_lastIterationTime(0),
	m_timeIntervalForOutputUpdate(0.1),
	m_interactive(false),
	m_lastRayCastEpsilon(0),
	m_camera(this, MDagPath()),
	m_glInteropActive(false),
	m_globalsChanged(false),
	m_renderLayersChanged(false),
	m_cameraDirty(false)
{
	DebugPrint("FireRenderContext::FireRenderContext()");
	state = StateUpdating;
	m_dirty = false;
}

FireRenderContext::~FireRenderContext()
{
	DebugPrint("FireRenderContext::~FireRenderContext()");
}

void FireRenderContext::initializeContext()
{
	DebugPrint("FireRenderContext::initializeContext()");

	RPR_THREAD_ONLY;

	LOCKMUTEX(this);

	auto createFlags = FireMaya::Options::GetContextDeviceFlags();

	createContextEtc(RPR_CONTEXT_OPENCL, createFlags);
}

void FireRenderContext::resize(unsigned int w, unsigned int h, bool renderView, rpr_GLuint* glTexture)
{
	RPR_THREAD_ONLY;
	// Get global data.
	FireRenderGlobalsData globals;
	globals.readFromCurrentScene();

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

	for (auto& fb : m.framebufferAOV)
		fb.Reset();

	rpr_framebuffer_format fmt = { 4, RPR_COMPONENT_TYPE_FLOAT32 };

	for (int i = 0; i != RPR_AOV_MAX; ++i)
	{
		m.framebufferAOV_resolved[i].Reset();

		if (aovEnabled[i]) {
			m.framebufferAOV[i] = frw::FrameBuffer(context, m_width, m_height, fmt);
			m.framebufferAOV[i].Clear();
			context.SetAOV(m.framebufferAOV[i], i);

			// Create an OpenGL interop resolved frame buffer if
			// required, otherwise, create a standard frame buffer.
			if (m_glInteropActive && glTexture)
				m.framebufferAOV_resolved[i] = frw::FrameBuffer(context, glTexture);
			else if (!m_glInteropActive)
				m.framebufferAOV_resolved[i] = frw::FrameBuffer(context, m_width, m_height, fmt);

			m.framebufferAOV_resolved[i].Clear();
		}
	}
}

void FireRenderContext::enableDisplayGammaCorrection(const FireRenderGlobalsData& globals)
{
	RPR_THREAD_ONLY;
	GetContext().SetParameter("displaygamma", globals.displayGamma);
}

void FireRenderContext::setCamera(MDagPath& cameraPath, bool useVRCamera)
{
	RPR_THREAD_ONLY;
	DebugPrint("FireRenderContext::setCamera(...)");
	m_camera.SetObject(cameraPath.node());

	if (useVRCamera)
	{
		// Read scene globals.
		FireRenderGlobalsData globals;
		globals.readFromCurrentScene();
		m_camera.setType(globals.cameraType);
	}
	else
		m_camera.setType(0);
}

void FireRenderContext::updateLimits(bool animation)
{
	// Read scene globals.
	FireRenderGlobalsData globals;
	globals.readFromCurrentScene();

	// Update limits.
	updateLimitsFromGlobalData(globals, animation);
}

void FireRenderContext::updateLimitsFromGlobalData(const FireRenderGlobalsData& globalData, bool animation, bool batch)
{
	// Get completion type.
	short type = globalData.completionCriteriaType;

	// Get total render time.
	int seconds =
		globalData.completionCriteriaHours * 3600 +
		globalData.completionCriteriaMinutes * 60 +
		globalData.completionCriteriaSeconds;

	// Get render iterations.
	int iterations = globalData.completionCriteriaIterations;

	// Set to a single iteration for animations.
	if (animation)
	{
		iterations = 1;
		seconds = 1;
		type = 0;
	}

	// Default batch renders to use iterations if set to unlimited.
	if (batch && type > 1)
		type = 0;

	// Update completion criteria.
	setCompletionCriteria(type, seconds, iterations);
}

bool FireRenderContext::buildScene(bool animation, bool isViewport, bool glViewport, bool freshen)
{
	RPR_THREAD_ONLY;
	DebugPrint("FireRenderContext::buildScene()");

	FireRenderGlobalsData frGlobalData;
	frGlobalData.readFromCurrentScene();

	auto createFlags = FireMaya::Options::GetContextDeviceFlags();

	{
		LOCKMUTEX(this);

		FireRenderThread::UseTheThread((createFlags & RPR_CREATION_FLAGS_ENABLE_CPU) != 0);

		bool avoidMaterialSystemDeletion_workaround = isViewport && (createFlags & RPR_CREATION_FLAGS_ENABLE_CPU);
		if (!createContextEtc(RPR_CONTEXT_OPENCL, createFlags, !avoidMaterialSystemDeletion_workaround, glViewport))
		{
			// Failed to create context
			if (glViewport)
			{
				// If we attempted to create gl interop context, try again without interop
				if (!createContextEtc(RPR_CONTEXT_OPENCL, createFlags, !avoidMaterialSystemDeletion_workaround, false))
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
				return false;
			}
		}

		m_transparent = frw::TransparentShader(GetMaterialSystem());
		m_transparent.SetValue("color", 1.0f);

		frGlobalData.setupContext(*this);

		updateLimitsFromGlobalData(frGlobalData);

		m_motionBlur = frGlobalData.motionBlur;

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

		UpdateGround(frGlobalData);

		attachCallbacks();
	}

	m_lastRayCastEpsilon = 0;	// Force recalculation on first render pass

	if(freshen)
		Freshen();

	return true;
}


void FireRenderContext::UpdateDefaultLights()
{
	RPR_THREAD_ONLY;
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
		MSelectionList slist;
		slist.add("defaultRenderGlobals");
		MObject renGlobalsObj;
		slist.getDependNode(0, renGlobalsObj);
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
	GetContext().SetParameter("preview", m_interactive ? 1 : 0);
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

void FireRenderContext::initSwatchScene()
{
	FireRenderThread::RunOnceProcAndWait([this]()
	{
		DebugPrint("FireRenderContext::buildSwatchScene(...)");

		auto createFlags = FireMaya::Options::GetContextDeviceFlags();

		createContextEtc(RPR_CONTEXT_OPENCL, createFlags);

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
	});
}

void FireRenderContext::updateSwatchSceneRenderLimits(const MObject& shaderObj)
{
	RPR_THREAD_ONLY;
	MFnDependencyNode nodeFn(shaderObj);

	MPlug iterationPlug = nodeFn.findPlug("swatchIterations");
	if (!iterationPlug.isNull())
		setCompletionCriteria(0, LONG_MAX, iterationPlug.asInt());
	else
		setCompletionCriteria(0, LONG_MAX, 1);
}

// This method will "tweak" precision of the ray-cast renderer based on scene bounding box:
void FireRenderContext::CheckSetRayCastEpsilon()
{
	RPR_THREAD_ONLY;
	MBoundingBox boundingBox;

	for (auto & it : m_sceneObjects)
	{
		if (it.second)
		{
			MFnDagNode node{ it.second->Object() };
			auto nodeBoundingBox = node.boundingBox();
			boundingBox.expand(nodeBoundingBox);
		}
	}

	auto epsilon = FireMaya::RayCastEpsilon::Calculate(boundingBox.width(), boundingBox.height(), boundingBox.depth());

	if (epsilon != m_lastRayCastEpsilon &&
		epsilon * 10 != m_lastRayCastEpsilon &&
		epsilon / 10 != m_lastRayCastEpsilon)	// Avoid changing if not off by factor of 2
	{
		scope.Context().SetParameter("raycastepsilon", epsilon);

		m_lastRayCastEpsilon = epsilon;
	}
}

void FireRenderContext::render(bool lock)
{
	RPR_THREAD_ONLY;
	LOCKMUTEX((lock ? this : nullptr));

	auto context = scope.Context();

	if (!context)
		return;

	//CheckSetRayCastEpsilon();

	if (m_restartRender)
	{
		for (int i = 0; i != RPR_AOV_MAX; ++i) {
			if (aovEnabled[i]){
				m.framebufferAOV[i].Clear();
				m.framebufferAOV_resolved[i].Clear();
			}
		}

		m_restartRender = false;
		m_startTime = clock();
		m_currentIteration = 0;
	}

	if (m_useRegion)
		context.RenderTile(m_region.left, m_region.right, m_height - m_region.top - 1, m_height - m_region.bottom - 1);
	else
		context.Render();

	if (m_currentIteration == 0)
	{
		DebugPrint("RPR GPU Memory used: %dMB", context.GetMemoryUsage() >> 20);
	}

	m_currentIteration++;

	m_cameraAttributeChanged = false;
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

bool FireRenderContext::createContext(rpr_creation_flags createFlags, rpr_context& result)
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
	int res = rprCreateContext(RPR_API_VERSION, plugins, pluginCount, createFlags, NULL, cachePath.asUTF8(), &context);
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


bool FireRenderContext::createContextEtc(rpr_context_type context_type, rpr_creation_flags creation_flags, bool destroyMaterialSystemOnDelete, bool glViewport)
{
	return FireRenderThread::RunOnceAndWait<bool>([this, context_type, &creation_flags, destroyMaterialSystemOnDelete, glViewport]()
	{
		RPR_THREAD_ONLY;

		DebugPrint("FireRenderContext::createContextEtc(%d, %d)", context_type, creation_flags);

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
		bool contextCreated = createContext(creation_flags, handle);
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
		m.framebufferAOV[RPR_AOV_OPACITY]
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

	// Get data from the RPR frame buffer.
	size_t dataSize;
	rpr_int frstatus = rprFrameBufferGetInfo(frameBuffer, RPR_FRAMEBUFFER_DATA, 0, nullptr, &dataSize);
	checkStatus(frstatus);

	// Check that the reported frame buffer size
	// in bytes matches the required dimensions.
	assert(dataSize == (sizeof(RV_PIXEL) * pixelCount));

	// Copy the frame buffer into temporary memory, if
	// required, or directly into the supplied pixel buffer.
	if (useTempData)
		m_tempData.resize(pixelCount);
	RV_PIXEL* data = useTempData ? m_tempData.get() : pixels;
	frstatus = rprFrameBufferGetInfo(frameBuffer, RPR_FRAMEBUFFER_DATA, dataSize, &data[0], nullptr);
	checkStatus(frstatus);

	if (mergeOpacity)
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
	if (useTempData)
	{
		copyPixels(pixels, data, width, height, region, flip, aov == RPR_AOV_COLOR);
	}

	//combine (Opacity to Alpha)
	combineWithOpacity(pixels, region.getArea(), m_opacityData.get());
}

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
			auto dagPath = frNode->DagPath();
			if (!dagPath.isValid() || dagPath.node() == ob || dagPath.transform() == ob)
			{
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
	RPR_THREAD_ONLY;

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
	DebugPrint("FireRenderContext::attachCallbacks()");
	if (getCallbackCreationDisabled())
		return;

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
	if (m_removedNodeCallback)
		MMessage::removeCallback(m_removedNodeCallback);
	if (m_addedNodeCallback)
		MMessage::removeCallback(m_addedNodeCallback);
	if (m_renderGlobalsCallback)
		MMessage::removeCallback(m_renderGlobalsCallback);
	if (m_renderLayerCallback)
		MMessage::removeCallback(m_renderLayerCallback);
}

void FireRenderContext::removedNodeCallback(MObject &node, void *clientData)
{
	if (auto frContext = GetCallbackContext(clientData))
	{
		frContext->m_removedNodes.push_back(node);
		frContext->setDirty();
	}
}

void FireRenderContext::addedNodeCallback(MObject &node, void *clientData)
{
	if (auto frContext = GetCallbackContext(clientData))
	{
		frContext->m_addedNodes.push_back(node);
		frContext->setDirty();
	}
}

void FireRenderContext::addNode(MObject& node)
{
	RPR_THREAD_ONLY;

	if (!node.isNull() && node.hasFn(MFn::kDagNode))
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
	RPR_THREAD_ONLY;

	RemoveRenderObject(node);
}

void FireRenderContext::updateFromGlobals()
{
	FireRenderThread::RunOnceProcAndWait([this]()
	{
		if (m_tonemappingChanged)
		{
			Lock lock(this);

			FireRenderGlobalsData globalsData;
			globalsData.readFromCurrentScene();
			globalsData.updateTonemapping(*this);

			m_tonemappingChanged = false;
		}

		if (!m_globalsChanged)
			return;

		Lock lock(this);

		FireRenderGlobalsData globalsData;
		globalsData.readFromCurrentScene();
		globalsData.setupContext(*this);

		updateLimitsFromGlobalData(globalsData);
		setMotionBlur(globalsData.motionBlur);
		UpdateGround(globalsData);

		m_camera.setType(globalsData.cameraType);

		m_globalsChanged = false;
	});
}

void FireRenderContext::updateRenderLayers()
{
	RPR_THREAD_ONLY;

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

	for (const auto& it : m_sceneObjects)
	{
		if (auto frMesh = dynamic_cast<FireRenderMesh*>(it.second.get()))
			frMesh->setDirty();
	}
}

void FireRenderContext::setInteractive(bool interactive)
{
	m_interactive = interactive;
}

bool FireRenderContext::isInteractive() const
{
	return m_interactive;
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
	RPR_THREAD_ONLY;
	using namespace FireMaya;

	FireRenderObject* ob = nullptr;
	auto node = dagPath.node();

	if (node.hasFn(MFn::kDagNode))
	{
		MFnDagNode dagNode(node);
		if (isGeometry(node))
		{
			ob = CreateSceneObject<FireRenderMesh>(dagPath);
		}
		else if (dagNode.typeId() == TypeId::FireRenderIESLightLocator
			|| isLight(node)
			|| VRay::isNonEnvironmentLight(dagNode))
		{
			if (dagNode.typeName() == "ambientLight") {
				ob = CreateSceneObject<FireRenderEnvLight>(dagPath);
			}
			else
			{
				ob = CreateSceneObject<FireRenderLight>(dagPath);
			}

		}
		else if (dagNode.typeId() == TypeId::FireRenderIBL
			|| dagNode.typeId() == TypeId::FireRenderEnvironmentLight
			|| dagNode.typeName() == "ambientLight"
			|| VRay::isEnvironmentLight(dagNode))
		{
			ob = CreateSceneObject<FireRenderEnvLight>(dagPath);
		}
		else if (dagNode.typeId() == TypeId::FireRenderSkyLocator
			|| VRay::isSkyLight(dagNode))
		{
			ob = CreateSceneObject<FireRenderSky>(dagPath);
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
		m_needRedraw = setToFalseOnExit;

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

	for (int i = 0; i < m_dirtyObjects.size(); i++)
	{
		std::shared_ptr<FireRenderObject> ptr = m_dirtyObjects[i].lock();
		if (ptr && ptr.get() == obj)
			return;		// already in list
	}

	// Find the object in objects list
	auto it = m_sceneObjects.find(obj->uuid());
	if (it != m_sceneObjects.end())
	{
		std::shared_ptr<FireRenderObject> ptr = it->second;
		m_dirtyObjects.push_back(ptr);
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
	RPR_THREAD_ONLY;

	if (!isDirty() || cancelled())
		return false;

	LOCKFORUPDATE((lock ? this : nullptr));

	m_inRefresh = true;

	updateFromGlobals();

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

	while (m_dirtyObjects.size() > 0 &&
		(state==FireRenderContext::StateRendering || state == FireRenderContext::StateUpdating))
	{
		// Request the object with removal it from the dirty list. Use mutex to prevent list's modifications.
		m_dirtyMutex.lock();
		size_t pos = m_dirtyObjects.size() - 1;
		std::shared_ptr<FireRenderObject> ptr = m_dirtyObjects[pos].lock();
		m_dirtyObjects.erase(m_dirtyObjects.begin() + pos);
		m_dirtyMutex.unlock();

		// Now perform update, if object is still valid
		if (ptr)
		{
			ptr->Freshen();
			changed = true;

			if (cancelled())
				return false;
		}
	}

	if (m_cameraDirty)
	{
		m_cameraDirty = false;
		m_camera.Freshen();
		changed = true;
	}

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

bool FireRenderContext::motionBlur()
{
	return m_motionBlur;
}

void FireRenderContext::setCameraAttributeChanged(bool value)
{
	m_cameraAttributeChanged = value;
	if (value)
		m_restartRender = true;
}

void FireRenderContext::setCompletionCriteria(short type, long seconds, int iterations)
{
	m_completionType = type;
	m_completionIterations = iterations;
	m_completionTime = seconds;
}

bool FireRenderContext::isUnlimited()
{
	// Corresponds to the kUnlimited enum in FireRenderGlobals.
	return m_completionType == 2;
}

void FireRenderContext::setStartedRendering()
{
	m_startTime = clock();
	m_currentIteration = 0;
}

bool FireRenderContext::keepRenderRunning()
{
	// Determine running state based on completion type.
	switch (m_completionType)
	{
		// Iterations.
	case 0:
		return m_currentIteration < m_completionIterations;

		// Time (in seconds).
	case 1:
	{
		long numberOfClicks = clock() - m_startTime;
		double secondsSpentRendering = numberOfClicks / (double)CLOCKS_PER_SEC;
		return secondsSpentRendering < m_completionTime;
	}

	// Unlimited.
	case 2:
		return true;

		// This should never be executed.
	default:
		return false;
	}
}

bool FireRenderContext::isFirstIterationAndShadersNOTCached() {
	if (m_currentIteration == 0) {
		return !areShadersCached();
	}
	return false;
}

void FireRenderContext::updateProgress()
{
	// Update progress based on completion type.
	switch (m_completionType)
	{
		// Iterations.
	case 0:
	{
		double iterationState = m_currentIteration / (double)m_completionIterations;
		m_progress = static_cast<int>(ceil(iterationState * 100));
		break;
	}

	// Time (in seconds).
	case 1:
	{
		long numberOfClocks = clock() - m_startTime;
		double secondsSpentRendering = numberOfClocks / (double)CLOCKS_PER_SEC;
		double timeState = secondsSpentRendering / m_completionTime;
		m_progress = static_cast<int>(ceil(timeState * 100));
		break;
	}

	// Unlimited.
	case 2:
	{
		m_progress = 0;
		break;
	}

	// This should never be executed.
	default:
		break;
	}
}

int FireRenderContext::getProgress()
{
	return std::min(m_progress, 100);
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


void FireRenderContext::UpdateGround(const FireRenderGlobalsData& data)
{
	RPR_THREAD_ONLY;

	auto context = GetContext();
	auto scene = GetScene();
	auto ms = GetMaterialSystem();

	if (m.ground.light)
	{
		scene.Detach(m.ground.light);
		m.ground.light = nullptr;
	}
	if (m.ground.reflections)
	{
		scene.Detach(m.ground.reflections);
		m.ground.reflections = nullptr;
	}
	if (m.ground.shadows)
	{
		scene.Detach(m.ground.shadows);
		m.ground.shadows = nullptr;
	}

	if (!data.useGround)
		return;

	MStatus status;
	MItDag itDag(MItDag::kDepthFirst, MFn::kDagNode, &status);
	bool is_sky_or_ibl = false;
	for (; !itDag.isDone(); itDag.next())
	{
		MDagPath dagPath;
		status = itDag.getPath(dagPath);
		if (MStatus::kSuccess != status)
		{
			break;
		}
		FireRenderObject* ob = nullptr;
		auto node = dagPath.node();

		if (node.hasFn(MFn::kDagNode))
		{
			MFnDagNode dagNode(node);
			if (dagNode.typeId() == FireMaya::TypeId::FireRenderIBL ||
				dagNode.typeName() == "ambientLight" ||
				dagNode.typeId() == FireMaya::TypeId::FireRenderEnvironmentLight)
			{
				is_sky_or_ibl = true;
				break;
			}
			else if (dagNode.typeId() == FireMaya::TypeId::FireRenderSkyLocator)
			{
				is_sky_or_ibl = true;
				break;
			}
		}
	}

	float width = data.groundRadius*2.0f;
	float length = data.groundRadius*2.0f;
	float w2 = width * 0.5f;
	float h2 = length * 0.5f;

	float z = data.groundHeight;

	float points[12] = {
		-w2, z, -h2,
		w2, z,-h2,
		w2, z,h2,
		-w2, z,h2
	};

	float points2[12] = {
		-w2, z - 0.0001f, -h2,
		w2, z - 0.0001f, -h2,
		w2, z - 0.0001f, h2,
		-w2, z - 0.0001f, h2
	};

	float normals[6] = {
		0.f, 1.f, 0.f,
		0.f, -1.f, 0.f
	};

	rpr_int indices[] = {
		2, 1, 0,
		3, 2, 0
	};

	rpr_int normal_indices[] = {
		0, 0, 0,
		0, 0, 0
	};

	rpr_int num_face_vertices[] = { 3, 3, 3, 3 };


	if (data.shadows || data.reflections)
	{
		MMatrix tm;
		tm.setToIdentity();
		float mfloats[4][4];
		tm.get(mfloats);

		if (data.reflections)
		{
			m.ground.reflections = context.CreateMesh(
				points, 4, sizeof(float) * 3,
				normals, 2, sizeof(float) * 3,
				nullptr, 0, 0,
				indices, sizeof(rpr_int),
				normal_indices, sizeof(rpr_int),
				nullptr, 0,
				num_face_vertices, 2);
			m.ground.reflections.SetTransform((rpr_float*)mfloats);

			frw::Shader shader(ms, frw::ShaderTypeMicrofacet);
			shader.SetValue("color", data.strength * 2);
			shader.SetValue("roughness", data.roughness);	// plain zero seems to fail

			frw::TransparentShader transparency(ms);
			transparency.SetColor(2);

			shader = ms.ShaderBlend(transparency, shader, 0.5);

			m.ground.reflections.SetShader(shader);
			m.ground.reflections.SetShadowFlag(false);

			scene.Attach(m.ground.reflections);
		}

		if (data.shadows)
		{
			m.ground.shadows = context.CreateMesh(
				points2, 4, sizeof(float) * 3,
				normals, 2, sizeof(float) * 3,
				nullptr, 0, 0,
				indices, sizeof(rpr_int),
				normal_indices, sizeof(rpr_int),
				nullptr, 0,
				num_face_vertices, 2);

			m.ground.shadows.SetTransform((rpr_float*)mfloats);

			frw::Shader shader(ms, frw::ShaderTypeDiffuse);
			shader.SetValue("color", 0.);
			shader = ms.ShaderBlend(frw::TransparentShader(ms), shader, data.strength);

			m.ground.shadows.SetShader(shader);
			m.ground.shadows.SetShadowFlag(false);
			m.ground.shadows.SetShadowCatcherFlag(true);
			scene.Attach(m.ground.shadows);
		}
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

	rpr_framebuffer frameBuffer = frameBufferAOV(RPR_AOV_COLOR);
	rpr_framebuffer opacityFrameBuffer = frameBufferAOV(RPR_AOV_OPACITY);
	rpr_framebuffer shadowCatcherFrameBuffer = frameBufferAOV(RPR_AOV_SHADOW_CATCHER);
	rpr_framebuffer backgroundFrameBuffer = frameBufferAOV(RPR_AOV_BACKGROUND);

	// Get data from the RPR frame buffer.
	size_t dataSize;
	rpr_int frstatus = rprFrameBufferGetInfo(frameBuffer, RPR_FRAMEBUFFER_DATA, 0, nullptr, &dataSize);
	checkStatus(frstatus);

	// Check that the reported frame buffer size
	// in bytes matches the required dimensions.
	assert(dataSize == (sizeof(RV_PIXEL) * pixelCount));

	auto context = GetContext();
	RprComposite compositeBg(context.Handle(), RPR_COMPOSITE_FRAMEBUFFER);
	compositeBg.SetInputFb("framebuffer.input", backgroundFrameBuffer);

	RprComposite compositeColor(context.Handle(), RPR_COMPOSITE_FRAMEBUFFER);
	compositeColor.SetInputFb("framebuffer.input", frameBuffer);

	RprComposite compositeOpacity(context.Handle(), RPR_COMPOSITE_FRAMEBUFFER);
	compositeOpacity.SetInputFb("framebuffer.input", opacityFrameBuffer);

	RprComposite compositeBgNorm(context.Handle(), RPR_COMPOSITE_NORMALIZE);
	compositeBgNorm.SetInputC("normalize.color", compositeBg);

	RprComposite compositeColorNorm(context.Handle(), RPR_COMPOSITE_NORMALIZE);
	compositeColorNorm.SetInputC("normalize.color", compositeColor);

	RprComposite compositeOpacityNorm(context.Handle(), RPR_COMPOSITE_NORMALIZE);
	compositeOpacityNorm.SetInputC("normalize.color", compositeOpacity);

	RprComposite compositeLerp1(context.Handle(), RPR_COMPOSITE_LERP_VALUE);
	compositeLerp1.SetInputC("lerp.color0", compositeBgNorm);
	compositeLerp1.SetInputC("lerp.color1", compositeColorNorm);
	compositeLerp1.SetInputC("lerp.weight", compositeOpacityNorm);

	RprComposite compositeShadowCatcher(context.Handle(), RPR_COMPOSITE_FRAMEBUFFER);
	compositeShadowCatcher.SetInputFb("framebuffer.input", shadowCatcherFrameBuffer);

	RprComposite compositeOne(context.Handle(), RPR_COMPOSITE_CONSTANT);
	compositeOne.SetInput4f("constant.input", 1.0f, 0.0f, 0.0f, 0.0f);

	RprComposite compositeZero(context.Handle(), RPR_COMPOSITE_CONSTANT);
	compositeZero.SetInput4f("constant.input", 0.0f, 0.0f, 0.0f, 1.0f);

	RprComposite compositeShadowCatcherNorm(context.Handle(), RPR_COMPOSITE_NORMALIZE);
	compositeShadowCatcherNorm.SetInputC("normalize.color", compositeShadowCatcher);
	compositeShadowCatcherNorm.SetInputC("normalize.shadowcatcher", compositeOne);
	
	RprComposite compositeLerp2(context.Handle(), RPR_COMPOSITE_LERP_VALUE);
	compositeLerp2.SetInputC("lerp.color0", compositeLerp1);
	compositeLerp2.SetInputC("lerp.color1", compositeZero);
	compositeLerp2.SetInputC("lerp.weight", compositeShadowCatcherNorm);

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

