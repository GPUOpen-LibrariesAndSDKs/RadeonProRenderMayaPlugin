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

#ifdef _WIN32
	// force using NorthStar for material viewer on Windows
	m_context.SetPluginEngine(TahoePluginVersion::RPR2);
#endif

	rpr_int res;
	if (!m_context.createContextEtc(createFlags, true, false, &res))
	{
		MString msg;
		FireRenderError errorToShow(res, msg, true);
	}

// MacOS only for now for tahoe resolve
#ifndef _WIN32
	m_context.normalization = frw::PostEffect(m_context.GetContext(), frw::PostEffectTypeNormalization);
	m_context.GetContext().Attach(m_context.normalization);
#endif
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
	switch (m_threadCmd)
	{
		case ThreadCommand::RENDER_IMAGE: {
			RPR::AutoMutexLock lock(m_renderData.m_mutex);
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
	FireRenderContext& inContext = m_renderData.m_context;
	frw::Context context = inContext.GetContext();
	rpr_context frcontext = context.Handle();

	rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_MAX_RECURSION, defaultMaterialViewRayDepth);
	rprContextSetParameterByKey1f(frcontext, RPR_CONTEXT_PDF_THRESHOLD, 0.0000f);
	rprContextSetParameterByKey1u(frcontext, RPR_CONTEXT_Y_FLIP, 0);
}

MStatus FireMaterialViewRenderer::startAsync(const JobParams& params)
{
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

	RPR::AutoMutexLock lock(m_renderData.m_mutex);

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
	m_renderData.m_mutex.lock();
	return MS::kSuccess;
}

MStatus FireMaterialViewRenderer::translateMesh(const MUuid& id, const MObject& node)
{
	return FireRenderThread::RunOnceAndWait<MStatus>([this, id, node]()
	{
		MFnDependencyNode nodeFn(node);

		if (m_renderData.m_shape)
			m_renderData.m_context.GetScene().Detach(m_renderData.m_shape);

		m_renderData.m_shape.Reset();

		std::vector<int> faceMaterialIndices;
		const std::vector<frw::Shape> shapes = FireMaya::MeshTranslator::TranslateMesh(m_renderData.m_context.GetContext(), node, faceMaterialIndices);
		if (!shapes.empty())
		{
			m_renderData.m_shape = shapes[0];
			if (m_renderData.m_shape)
				m_renderData.m_context.GetScene().Attach(m_renderData.m_shape);
		}

		if (m_renderData.m_shape && std::get<frw::Shader>(m_renderData.m_surfaceShader))
		{
			m_renderData.m_shape.SetShader(std::get<frw::Shader>(m_renderData.m_surfaceShader));
			m_renderData.m_shape.SetVolumeShader(m_renderData.m_volumeShader);
		}

		m_meshId = id;

		return MS::kSuccess;
	});
}

MStatus FireMaterialViewRenderer::translateLightSource(const MUuid& id, const MObject& node)
{
	return FireRenderThread::RunOnceAndWait<MStatus>([this, id, node]()
	{
		MFnDependencyNode nodeFn(node);

		FrLight& light = m_renderData.m_lights[id.asString().asChar()];
		m_renderData.m_lightsNames[id.asString().asChar()] = nodeFn.name().asChar();
		if (light.isAreaLight)
		{
			if (light.areaLight)
			{
				m_renderData.m_context.GetScene().Detach(light.areaLight);
				light.areaLight.Reset();
				light.emissive.Reset();
			}
		}
		else
		{
			if (light.light)
			{
				m_renderData.m_context.GetScene().Detach(light.light);
				light.light.Reset();
			}
		}

		FireMaya::translateLight(light, m_renderData.m_context.GetScope(), m_renderData.m_context.GetContext(), node, MMatrix());
		if (!m_renderData.m_endLight)
		{
			if (light.isAreaLight)
			{
				if (light.areaLight)
				{
					m_renderData.m_context.GetScene().Attach(light.areaLight);
					light.areaLight.SetVisibility(true);
				}
			}
			else
			{
				if (light.light)
					m_renderData.m_context.GetScene().Attach(light.light);
			}
		}
		return MS::kSuccess;
	});
}

MStatus FireMaterialViewRenderer::translateCamera(const MUuid& id, const MObject& node)
{
	return FireRenderThread::RunOnceAndWait<MStatus>([this, id, node]()
	{
		m_cameraId = id;
		MFnDependencyNode nodeFn(node);

		m_renderData.m_camera = m_renderData.m_context.GetContext().CreateCamera();
		m_renderData.m_context.GetScene().SetCamera(m_renderData.m_camera);

		MMatrix mtx;
		FireMaya::translateCamera(m_renderData.m_camera, node, mtx, true);

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
		rpr_int frstatus;

		if (childId == m_cameraId)
		{
			if (m_renderData.m_camera)
			{
				MPoint eye = MPoint(0, 0, 0, 1) * matrix;
				// convert eye and lookat from cm to m
				eye = eye * 0.01;
				MVector viewDir = MVector::zNegAxis * matrix;
				MVector upDir = MVector::yAxis * matrix;
				MPoint  lookat = eye + viewDir;
				frstatus = rprCameraLookAt(m_renderData.m_camera.Handle(),
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
			scaleM[0][0] = scaleM[1][1] = scaleM[2][2] = 0.01;
			m *= scaleM;
			float mfloats[4][4];
			m.get(mfloats);

			if (m_renderData.m_shape)
				m_renderData.m_shape.SetTransform((rpr_float*)mfloats);
		}


		std::map<std::string, FrLight>::iterator it = m_renderData.m_lights.find(childId.asString().asChar());

		if (it != m_renderData.m_lights.end())
		{
			MMatrix m = matrix;
			MMatrix scaleM;
			scaleM.setToIdentity();
			scaleM[0][0] = scaleM[1][1] = scaleM[2][2] = 0.01;
			if (m_renderData.m_lightsNames[childId.asString().asChar()].find("Fill") != std::string::npos)
			{
				float mf[4][4] = { {0.023902599999999993f, 0.0, 0.9997140000000001f, 0.0f},
				{0.03361340000000001f, 0.9994350000000001f, -0.0008036780000000049f, 0.0f},
				{0.9991490402513146f, -0.03362300135452265f, -0.0238891009623866f, 0.0f},
				{3.91f, 0.843f, -3.061f, 1.0f} };
				m = MMatrix(mf);
			}
			if (m_renderData.m_lightsNames[childId.asString().asChar()].find("Rim") != std::string::npos)
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
		std::get<frw::Shader>(m_renderData.m_surfaceShader) = m_renderData.m_context.GetShader(node);
		std::get<FireMaya::NodeId>(m_renderData.m_surfaceShader) = getNodeUUid(node);

		m_renderData.m_volumeShader = m_renderData.m_context.GetVolumeShader(node);

		if (m_renderData.m_shape && std::get<frw::Shader>(m_renderData.m_surfaceShader))
		{
			m_renderData.m_shape.SetShader(std::get<frw::Shader>(m_renderData.m_surfaceShader));
		}

		if (m_renderData.m_shape && m_renderData.m_volumeShader)
		{
			m_renderData.m_shape.SetVolumeShader(m_renderData.m_volumeShader);
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
		if ((id == m_envId) && (name == "imageFile"))
		{
			if (m_renderData.m_endLight)
			{
				m_renderData.m_context.GetScene().Detach(m_renderData.m_endLight);
				m_renderData.m_endLight.Reset();
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
					m_renderData.m_endLight = m_renderData.m_context.GetContext().CreateEnvironmentLight();

					m_renderData.m_envImage = frw::Image(m_renderData.m_context.GetContext(), newPath.asChar());

					if (m_renderData.m_envImage)
						m_renderData.m_endLight.SetImage(m_renderData.m_envImage);

					m_renderData.m_endLight.SetLightIntensityScale(1.0f);
					m_renderData.m_context.GetScene().Attach(m_renderData.m_endLight);

					MMatrix scaleM;
					scaleM.setToIdentity();
					scaleM[0][0] = scaleM[1][1] = scaleM[2][2] = 2.0;
					float mfloats[4][4];
					scaleM.get(mfloats);
					m_renderData.m_endLight.SetTransform((rpr_float*)mfloats);
				}

				//remove lights
				for (auto it : m_renderData.m_lights)
				{
					FrLight& light = it.second;
					if (light.isAreaLight)
					{
						if (light.areaLight)
							m_renderData.m_context.GetScene().Detach(light.areaLight);
					}
					else
					{
						if (light.light)
							m_renderData.m_context.GetScene().Detach(light.light);
					}
				}
			}
			else
			{
				//add lights
				for (auto it : m_renderData.m_lights)
				{
					FrLight& light = it.second;
					if (light.isAreaLight)
					{
						if (light.areaLight)
						{
							m_renderData.m_context.GetScene().Attach(light.areaLight);
							light.areaLight.SetVisibility(true);
						}
					}
					else
					{
						if (light.light)
							m_renderData.m_context.GetScene().Attach(light.light);
					}
				}
			}
		}
		return MS::kSuccess;
	});
}

MStatus FireMaterialViewRenderer::setShader(const MUuid& id, const MUuid& shaderId)
{
	if (m_renderData.m_shape && std::get<frw::Shader>(m_renderData.m_surfaceShader))
	{
		m_renderData.m_shape.SetShader(std::get<frw::Shader>(m_renderData.m_surfaceShader));
	}

	if (m_renderData.m_shape && m_renderData.m_volumeShader)
	{
		m_renderData.m_shape.SetVolumeShader(m_renderData.m_volumeShader);
	}

	return MS::kSuccess;
}

MStatus FireMaterialViewRenderer::setResolution(unsigned int width, unsigned int height)
{
	m_renderData.m_width = width;
	m_renderData.m_height = height;

	m_renderData.m_pixels.resize(m_renderData.m_width * m_renderData.m_height);

	return MS::kSuccess;
}

MStatus FireMaterialViewRenderer::endSceneUpdate()
{
	return FireRenderThread::RunOnceAndWait<MStatus>([this]() -> MStatus
	{
		rpr_framebuffer_format fmt = { 4, RPR_COMPONENT_TYPE_FLOAT32 };

		m_renderData.m_framebufferColor = frw::FrameBuffer(m_renderData.m_context.GetContext(), m_renderData.m_width, m_renderData.m_height, fmt);
		m_renderData.m_framebufferColor.Clear();

		m_renderData.m_framebufferResolved = frw::FrameBuffer(m_renderData.m_context.GetContext(), m_renderData.m_width, m_renderData.m_height, fmt);
		m_renderData.m_framebufferResolved.Clear();

		m_renderData.m_context.GetContext().SetAOV(m_renderData.m_framebufferColor, RPR_AOV_COLOR);

		m_numIteration = 0;
		m_threadCmd = ThreadCommand::RENDER_IMAGE;

		m_renderData.m_mutex.unlock();

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
	auto context = m_renderData.m_context.GetContext();

	context.SetParameter(RPR_CONTEXT_ITERATIONS, 1);
	context.SetParameter(RPR_CONTEXT_FRAMECOUNT, m_numIteration);


	try 
	{
		context.Render();
	}
	catch (const FireRenderException&) 
	{
		//Prevents view freezing after close material view with invalid material selected
		auto& shader = std::get<frw::Shader>(m_renderData.m_surfaceShader);
		shader.Reset();

		//Prevents view freezing after selecting invalid material and then selecting valid
		auto& nodeId = std::get<FireMaya::NodeId>(m_renderData.m_surfaceShader);
		m_renderData.m_context.GetScope().SetCachedShader(nodeId, nullptr);

		m_threadCmd = ThreadCommand::BEGIN_UPDATE;
		return;
	}

	m_renderData.m_framebufferColor.Resolve(m_renderData.m_framebufferResolved, false);

	size_t dataSize = 0;
	frstatus = rprFrameBufferGetInfo(m_renderData.m_framebufferResolved.Handle(), RPR_FRAMEBUFFER_DATA, 0, NULL, &dataSize);
	checkStatus(frstatus);

	size_t WidthHeight = m_renderData.m_width * m_renderData.m_height;

	if (m_renderData.m_pixels.empty())
		m_renderData.m_pixels.resize(WidthHeight);

	frstatus = rprFrameBufferGetInfo(m_renderData.m_framebufferResolved.Handle(), RPR_FRAMEBUFFER_DATA, dataSize, m_renderData.m_pixels.data(), nullptr);
	checkStatus(frstatus);

	m_numIteration++;

	MPxRenderer::RefreshParams parameters;
	parameters.height = m_renderData.m_height;
	parameters.width = m_renderData.m_width;
	parameters.top = m_renderData.m_height - 1;
	parameters.bottom = 0;
	parameters.left = 0;
	parameters.right = m_renderData.m_width - 1;
	parameters.channels = 4;
	parameters.bytesPerChannel = sizeof(float);
	parameters.data = m_renderData.m_pixels.data();
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
