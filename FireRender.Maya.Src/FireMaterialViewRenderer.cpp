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
#include "FireMaterialViewRenderer.h"
#include <maya/MFnDependencyNode.h>
#include <maya/MDistance.h>
#include <maya/MFileObject.h>

#include "FireRenderIpr.h"
#include "FireRenderUtils.h"
#include "FireRenderError.h"
#include "FireRenderThread.h"
#include "AutoLock.h"

#ifdef MAYA2015
#undef min
#undef max
#endif

#include <imageio.h>

using namespace FireMaya;

#ifndef MAYA2015

// ================================
// FireRenderRenderData
// ================================

const unsigned int defaultMaterialViewRayDepth = 5;

FireRenderRenderData::FireRenderRenderData() :
	m_context(),
	m_width(128),
	m_height(128)
{
	m_context.setCallbackCreationDisabled(true);

	auto createFlags = FireMaya::Options::GetContextDeviceFlags(RenderType::ViewportRender);

	rpr_int res;
	if (!m_context.createContextEtc(createFlags, true, &res))
	{
		MString msg;
		FireRenderError errorToShow(res, msg, true);
	}
}

FireRenderRenderData::~FireRenderRenderData()
{
	m_context.cleanScene();
}

// ================================
// FireMaterialViewRenderer
// ================================

bool FireMaterialViewRenderer::RunOnFireRenderThread()
{
	assert(m_renderDataPtr);

	switch (m_threadCmd)
	{
		case ThreadCommand::RENDER_IMAGE: {
			RPR::AutoMutexLock lock(m_renderDataPtr->m_mutex);
			render();
			return true;
		}

		case ThreadCommand::STOP_THREAD:
			return false;

		case ThreadCommand::BEGIN_UPDATE:

		default:
			return true;
	}
}

FireMaterialViewRenderer::FireMaterialViewRenderer() :
	MPxRenderer(),
	m_isThreadRunning(false),
	m_numIteration(0),
	m_threadCmd(ThreadCommand::BEGIN_UPDATE)
{
}

MStatus FireMaterialViewRenderer::startAsync(const JobParams& params)
{
	if (!m_renderDataPtr)
	{
		m_renderDataPtr = std::make_unique<FireRenderRenderData>();

		FireRenderContext& inContext = m_renderDataPtr->m_context;
		frw::Context context = inContext.GetContext();
		rpr_context frcontext = context.Handle();

		rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_MAX_RECURSION, defaultMaterialViewRayDepth);
		rprContextSetParameterByKey1f(frcontext, RPR_CONTEXT_PDF_THRESHOLD, 0.0000f);
		rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_Y_FLIP, 0);
	}

	MStatus mstatus;
	if (!m_isThreadRunning)
	{
		FireRenderThread::KeepRunning(std::bind(&FireMaterialViewRenderer::RunOnFireRenderThread, this));

		m_isThreadRunning = true;
	}
	return MS::kSuccess;
}

MStatus FireMaterialViewRenderer::stopAsync()
{
	m_threadCmd = ThreadCommand::STOP_THREAD;

	RPR::AutoMutexLock lock(m_renderDataPtr->m_mutex);

	m_isThreadRunning = false;

	return MS::kSuccess;
}

bool FireMaterialViewRenderer::isRunningAsync()
{
	return m_isThreadRunning;
}

MStatus FireMaterialViewRenderer::beginSceneUpdate()
{
	if (!isRunningAsync())
	{
		JobParams params;
		startAsync(params);
	}

	m_threadCmd = ThreadCommand::BEGIN_UPDATE;
	m_renderDataPtr->m_mutex.lock();
	return MS::kSuccess;
}

MStatus FireMaterialViewRenderer::translateMesh(const MUuid& id, const MObject& node)
{
	return FireRenderThread::RunOnceAndWait<MStatus>([this, id, node]()
	{
		assert(m_renderDataPtr);

		MFnDependencyNode nodeFn(node);

		if (m_renderDataPtr->m_shape)
			m_renderDataPtr->m_context.GetScene().Detach(m_renderDataPtr->m_shape);

		m_renderDataPtr->m_shape.Reset();

		std::vector<int> faceMaterialIndices;
		const frw::Shape shape = FireMaya::MeshTranslator::TranslateMesh(m_renderDataPtr->m_context.GetContext(), node, faceMaterialIndices);
		if (shape)
		{
			m_renderDataPtr->m_shape = shape;
			if (m_renderDataPtr->m_shape)
				m_renderDataPtr->m_context.GetScene().Attach(m_renderDataPtr->m_shape);
		}

		if (m_renderDataPtr->m_shape && std::get<frw::Shader>(m_renderDataPtr->m_surfaceShader))
		{
			m_renderDataPtr->m_shape.SetShader(std::get<frw::Shader>(m_renderDataPtr->m_surfaceShader));
			m_renderDataPtr->m_shape.SetVolumeShader(m_renderDataPtr->m_volumeShader);
		}

		m_meshId = id;

		return MS::kSuccess;
	});
}

MStatus FireMaterialViewRenderer::translateLightSource(const MUuid& id, const MObject& node)
{
	return FireRenderThread::RunOnceAndWait<MStatus>([this, id, node]()
	{
		assert(m_renderDataPtr);
		MFnDependencyNode nodeFn(node);

		FrLight& light = m_renderDataPtr->m_lights[id.asString().asChar()];
		m_renderDataPtr->m_lightsNames[id.asString().asChar()] = nodeFn.name().asChar();
		if (light.isAreaLight)
		{
			if (light.areaLight)
			{
				m_renderDataPtr->m_context.GetScene().Detach(light.areaLight);
				light.areaLight.Reset();
				light.emissive.Reset();
			}
		}
		else
		{
			if (light.light)
			{
				m_renderDataPtr->m_context.GetScene().Detach(light.light);
				light.light.Reset();
			}
		}

		FireMaya::translateLight(light, m_renderDataPtr->m_context.GetScope(), m_renderDataPtr->m_context.GetContext(), node, MMatrix());
		if (!m_renderDataPtr->m_endLight)
		{
			if (light.isAreaLight)
			{
				if (light.areaLight)
				{
					m_renderDataPtr->m_context.GetScene().Attach(light.areaLight);
					light.areaLight.SetVisibility(true);
				}
			}
			else
			{
				if (light.light)
					m_renderDataPtr->m_context.GetScene().Attach(light.light);
			}
		}
		return MS::kSuccess;
	});
}

MStatus FireMaterialViewRenderer::translateCamera(const MUuid& id, const MObject& node)
{
	return FireRenderThread::RunOnceAndWait<MStatus>([this, id, node]()
	{
		assert(m_renderDataPtr);

		m_cameraId = id;
		MFnDependencyNode nodeFn(node);

		m_renderDataPtr->m_camera = m_renderDataPtr->m_context.GetContext().CreateCamera();
		m_renderDataPtr->m_context.GetScene().SetCamera(m_renderDataPtr->m_camera);

		MMatrix mtx;
		FireMaya::translateCamera(m_renderDataPtr->m_camera, node, mtx, true);

		return MS::kSuccess;
	});
}

MStatus FireMaterialViewRenderer::translateEnvironment(const MUuid& id, EnvironmentType type)
{
	return FireRenderThread::RunOnceAndWait<MStatus>([this, id, type]()
	{
		m_envId = id;
		return MS::kSuccess;
	});
}

MStatus FireMaterialViewRenderer::translateTransform(const MUuid& id, const MUuid& childId, const MMatrix& matrix)
{
	return FireRenderThread::RunOnceAndWait<MStatus>([this, id, childId, matrix]()
	{
		assert(m_renderDataPtr);
		rpr_int frstatus;

		if (childId == m_cameraId)
		{
			if (m_renderDataPtr->m_camera)
			{
				MPoint eye = MPoint(0, 0, 0, 1) * matrix;
				// convert eye and lookat from cm to m
				eye = eye * GetSceneUnitsConversionCoefficient();
				MVector viewDir = MVector::zNegAxis * matrix;
				MVector upDir = MVector::yAxis * matrix;
				MPoint  lookat = eye + viewDir;
				frstatus = rprCameraLookAt(m_renderDataPtr->m_camera.Handle(),
					static_cast<float>(eye.x), static_cast<float>(eye.y), static_cast<float>(eye.z),
					static_cast<float>(lookat.x), static_cast<float>(lookat.y), static_cast<float>(lookat.z),
					static_cast<float>(upDir.x), static_cast<float>(upDir.y), static_cast<float>(upDir.z));

				checkStatus(frstatus);
			}
		}

		if (childId == m_meshId)
		{
			MMatrix m = matrix;
			MMatrix scaleM;
			scaleM.setToIdentity();
			scaleM[0][0] = scaleM[1][1] = scaleM[2][2] = GetSceneUnitsConversionCoefficient();
			m *= scaleM;
			float mfloats[4][4];
			m.get(mfloats);

			if (m_renderDataPtr->m_shape)
				m_renderDataPtr->m_shape.SetTransform((rpr_float*)mfloats);
		}


		std::map<std::string, FrLight>::iterator it = m_renderDataPtr->m_lights.find(childId.asString().asChar());

		if (it != m_renderDataPtr->m_lights.end())
		{
			MMatrix m = matrix;
			MMatrix scaleM;
			scaleM.setToIdentity();
			scaleM[0][0] = scaleM[1][1] = scaleM[2][2] = GetSceneUnitsConversionCoefficient();
			if (m_renderDataPtr->m_lightsNames[childId.asString().asChar()].find("Fill") != std::string::npos)
			{
				float mf[4][4] = { {0.023902599999999993f, 0.0, 0.9997140000000001f, 0.0f},
				{0.03361340000000001f, 0.9994350000000001f, -0.0008036780000000049f, 0.0f},
				{0.9991490402513146f, -0.03362300135452265f, -0.0238891009623866f, 0.0f},
				{3.91f, 0.843f, -3.061f, 1.0f} };
				m = MMatrix(mf);
			}
			if (m_renderDataPtr->m_lightsNames[childId.asString().asChar()].find("Rim") != std::string::npos)
			{
				float mf[4][4] = { { 0.569934f, 0.0f, -0.8216899999999999f, 0.0f},
				{0.1500699999999999f, -0.9831809999999999f, 0.10408999999999993f, 0.0f},
				{0.8078702580221779f, 0.1826350583310191f, 0.5603481789671746f, 0.0f},
				{-1.339f, 3.744f, 6.811f, 1.0f} };
				m = MMatrix(mf);
			}

			m *= scaleM;

			float mfloats[4][4];
			m.get(mfloats);

			if (it->second.isAreaLight && it->second.areaLight)
				it->second.areaLight.SetTransform(&mfloats[0][0]);
			else if (it->second.light)
				it->second.light.SetTransform(&mfloats[0][0]);
		}

		return MS::kSuccess;
	});
}

MStatus FireMaterialViewRenderer::translateShader(const MUuid& id, const MObject& node)
{
	return FireRenderThread::RunOnceAndWait<MStatus>([this, id, node]()
	{
		assert(m_renderDataPtr);
		std::get<frw::Shader>(m_renderDataPtr->m_surfaceShader) = m_renderDataPtr->m_context.GetShader(node);

		MFnDependencyNode fnShdr(node);
		std::string shdrName = fnShdr.name().asChar(); 
		std::string shaderId = getNodeUUid(node);
		shaderId += shdrName;
		std::get<FireMaya::NodeId>(m_renderDataPtr->m_surfaceShader) = shaderId;

		m_renderDataPtr->m_volumeShader = m_renderDataPtr->m_context.GetVolumeShader(node);

		if (m_renderDataPtr->m_shape && std::get<frw::Shader>(m_renderDataPtr->m_surfaceShader))
		{
			m_renderDataPtr->m_shape.SetShader(std::get<frw::Shader>(m_renderDataPtr->m_surfaceShader));
		}

		if (m_renderDataPtr->m_shape && m_renderDataPtr->m_volumeShader)
		{
			m_renderDataPtr->m_shape.SetVolumeShader(m_renderDataPtr->m_volumeShader);
		}

		return MS::kSuccess;
	});
}

MStatus FireMaterialViewRenderer::setProperty(const MUuid& id, const MString& name, bool value)
{
	return MS::kSuccess;
}

MStatus FireMaterialViewRenderer::setProperty(const MUuid& id, const MString& name, int value)
{
	return MS::kSuccess;
}

MStatus FireMaterialViewRenderer::setProperty(const MUuid& id, const MString& name, float value)
{
	return MS::kSuccess;
}

MStatus FireMaterialViewRenderer::setProperty(const MUuid& id, const MString& name, const MString& value)
{
	return FireRenderThread::RunOnceAndWait<MStatus>([this, id, name, value]()
	{
		assert(m_renderDataPtr);
		if ((id == m_envId) && (name == "imageFile"))
		{
			if (m_renderDataPtr->m_endLight)
			{
				m_renderDataPtr->m_context.GetScene().Detach(m_renderDataPtr->m_endLight);
				m_renderDataPtr->m_endLight.Reset();
			}

			if (value != "")
			{
				MString newPath = value;
				MString mayaPath = MGlobal::executeCommandStringResult("getenv MAYA_LOCATION");
				MString cachePath = getShaderCachePath();
				newPath.substitute(mayaPath + "/presets/Assets/IBL", cachePath);

				std::string fileName = value.asChar();
				std::string outFileName = newPath.asChar();
				MFileObject fileObj2;
				fileObj2.setRawFullName(newPath);
				if (!fileObj2.exists())
				{
					OIIO::ImageInput *imgInput = OIIO::ImageInput::create(fileName);
					if (imgInput)
					{
						OIIO::ImageSpec imgSpec, outSpec;
						imgInput->open(fileName, imgSpec);

						outSpec = imgSpec;
						outSpec.set_format(OIIO::TypeDesc::FLOAT);

						OIIO::ImageOutput *imgOutput = OIIO::ImageOutput::create(outFileName);
						if (imgOutput)
						{
							imgOutput->open(outFileName, outSpec);
							imgOutput->copy_image(imgInput);
							imgOutput->close();
							delete imgOutput;
						}
						imgInput->close();
						delete imgInput;
					}
				}


				MFileObject fileObj;
				fileObj.setRawFullName(newPath);
				if (fileObj.exists())
				{
					m_renderDataPtr->m_endLight = m_renderDataPtr->m_context.GetContext().CreateEnvironmentLight();

					m_renderDataPtr->m_envImage = frw::Image(m_renderDataPtr->m_context.GetContext(), newPath.asChar());

					if (m_renderDataPtr->m_envImage)
						m_renderDataPtr->m_endLight.SetImage(m_renderDataPtr->m_envImage);

					m_renderDataPtr->m_endLight.SetLightIntensityScale(1.0f);
					m_renderDataPtr->m_context.GetScene().Attach(m_renderDataPtr->m_endLight);

					MMatrix scaleM;
					scaleM.setToIdentity();
					scaleM[0][0] = scaleM[1][1] = scaleM[2][2] = 2.0;
					float mfloats[4][4];
					scaleM.get(mfloats);
					m_renderDataPtr->m_endLight.SetTransform((rpr_float*)mfloats);
				}

				//remove lights
				for (auto it : m_renderDataPtr->m_lights)
				{
					FrLight& light = it.second;
					if (light.isAreaLight)
					{
						if (light.areaLight)
							m_renderDataPtr->m_context.GetScene().Detach(light.areaLight);
					}
					else
					{
						if (light.light)
							m_renderDataPtr->m_context.GetScene().Detach(light.light);
					}
				}
			}
			else
			{
				//add lights
				for (auto it : m_renderDataPtr->m_lights)
				{
					FrLight& light = it.second;
					if (light.isAreaLight)
					{
						if (light.areaLight)
						{
							m_renderDataPtr->m_context.GetScene().Attach(light.areaLight);
							light.areaLight.SetVisibility(true);
						}
					}
					else
					{
						if (light.light)
							m_renderDataPtr->m_context.GetScene().Attach(light.light);
					}
				}
			}
		}
		return MS::kSuccess;
	});
}

MStatus FireMaterialViewRenderer::setShader(const MUuid& id, const MUuid& shaderId)
{
	assert(m_renderDataPtr);
	if (m_renderDataPtr->m_shape && std::get<frw::Shader>(m_renderDataPtr->m_surfaceShader))
	{
		m_renderDataPtr->m_shape.SetShader(std::get<frw::Shader>(m_renderDataPtr->m_surfaceShader));
	}

	if (m_renderDataPtr->m_shape && m_renderDataPtr->m_volumeShader)
	{
		m_renderDataPtr->m_shape.SetVolumeShader(m_renderDataPtr->m_volumeShader);
	}

	return MS::kSuccess;
}

MStatus FireMaterialViewRenderer::setResolution(unsigned int width, unsigned int height)
{
	m_renderDataPtr->m_width = width;
	m_renderDataPtr->m_height = height;

	m_renderDataPtr->m_pixels.resize(m_renderDataPtr->m_width * m_renderDataPtr->m_height);

	return MS::kSuccess;
}

MStatus FireMaterialViewRenderer::endSceneUpdate()
{
	return FireRenderThread::RunOnceAndWait<MStatus>([this]() -> MStatus
	{
		assert(m_renderDataPtr);
		rpr_framebuffer_format fmt = { 4, RPR_COMPONENT_TYPE_FLOAT32 };

		m_renderDataPtr->m_framebufferColor = frw::FrameBuffer(m_renderDataPtr->m_context.GetContext(), m_renderDataPtr->m_width, m_renderDataPtr->m_height, fmt);
		m_renderDataPtr->m_framebufferColor.Clear();

		m_renderDataPtr->m_framebufferResolved = frw::FrameBuffer(m_renderDataPtr->m_context.GetContext(), m_renderDataPtr->m_width, m_renderDataPtr->m_height, fmt);
		m_renderDataPtr->m_framebufferResolved.Clear();

		m_renderDataPtr->m_context.GetContext().SetAOV(m_renderDataPtr->m_framebufferColor, RPR_AOV_COLOR);

		m_numIteration = 0;
		m_threadCmd = ThreadCommand::RENDER_IMAGE;

		m_renderDataPtr->m_mutex.unlock();

		return MS::kSuccess;
	});
}

MStatus FireMaterialViewRenderer::destroyScene()
{
	return MS::kSuccess;
}

bool FireMaterialViewRenderer::isSafeToUnload()
{
	return true;
}

void* FireMaterialViewRenderer::creator()
{
	FireMaterialViewRenderer* renderer = new FireMaterialViewRenderer();
	return renderer;
}

void FireMaterialViewRenderer::render()
{
	RPR_THREAD_ONLY;

	rpr_int frstatus;
	auto context = m_renderDataPtr->m_context.GetContext();

	context.SetParameter(RPR_CONTEXT_ITERATIONS, 1);
	context.SetParameter(RPR_CONTEXT_FRAMECOUNT, m_numIteration);


	try 
	{
		context.Render();
	}
	catch (const FireRenderException&) 
	{
		//Prevents view freezing after close material view with invalid material selected
		auto& shader = std::get<frw::Shader>(m_renderDataPtr->m_surfaceShader);
		shader.Reset();

		//Prevents view freezing after selecting invalid material and then selecting valid
		auto& nodeId = std::get<FireMaya::NodeId>(m_renderDataPtr->m_surfaceShader);
		m_renderDataPtr->m_context.GetScope().SetCachedShader(nodeId, nullptr);

		m_threadCmd = ThreadCommand::BEGIN_UPDATE;
		return;
	}

	m_renderDataPtr->m_framebufferColor.Resolve(m_renderDataPtr->m_framebufferResolved, false);

	size_t dataSize = 0;
	frstatus = rprFrameBufferGetInfo(m_renderDataPtr->m_framebufferResolved.Handle(), RPR_FRAMEBUFFER_DATA, 0, NULL, &dataSize);
	checkStatus(frstatus);

	size_t WidthHeight = m_renderDataPtr->m_width * m_renderDataPtr->m_height;

	if (m_renderDataPtr->m_pixels.empty())
		m_renderDataPtr->m_pixels.resize(WidthHeight);

	frstatus = rprFrameBufferGetInfo(m_renderDataPtr->m_framebufferResolved.Handle(), RPR_FRAMEBUFFER_DATA, dataSize, m_renderDataPtr->m_pixels.data(), nullptr);
	checkStatus(frstatus);

	m_numIteration++;

	MPxRenderer::RefreshParams parameters;
	parameters.height = m_renderDataPtr->m_height;
	parameters.width = m_renderDataPtr->m_width;
	parameters.top = m_renderDataPtr->m_height - 1;
	parameters.bottom = 0;
	parameters.left = 0;
	parameters.right = m_renderDataPtr->m_width - 1;
	parameters.channels = 4;
	parameters.bytesPerChannel = sizeof(float);
	parameters.data = m_renderDataPtr->m_pixels.data();
	refresh(parameters);

	if (m_numIteration >= FireRenderGlobalsData::getThumbnailIterCount())
	{
		ProgressParams params;
		params.progress = 1.0;
		progress(params);

		m_threadCmd = ThreadCommand::BEGIN_UPDATE;
	}
}

#endif
