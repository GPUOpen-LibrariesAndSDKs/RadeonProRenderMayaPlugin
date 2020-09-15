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
#include "GLTFTranslator.h"

#include "Translators/Translators.h"
#include "AnimationExporter.h"
#include "Context/TahoeContext.h"

#include <memory>
#include <array>
#include <thread>
#include <float.h>

#include "ProRenderGLTF.h"

using namespace FireMaya;

GLTFTranslator::GLTFTranslator() = default;
GLTFTranslator::~GLTFTranslator() = default;

void* GLTFTranslator::creator()
{
	return new GLTFTranslator();
}

MStatus	GLTFTranslator::writer(const MFileObject& file,
	const MString& optionsString,
	FileAccessMode mode)
{
	// Create new context and fill it with scene
	std::unique_ptr<TahoeContext> fireRenderContext = std::make_unique<TahoeContext>();
	ContextAutoCleaner contextAutoCleaner(fireRenderContext.get());

	// to be sure RenderProgressBars will be closed upon function exit
	std::unique_ptr<RenderProgressBars> progressBars = std::make_unique<RenderProgressBars>(false);
	m_progressBars = progressBars.get();

	AnimationExporter animationExporter(true);
	animationExporter.SetProgressBars(m_progressBars);

	m_progressBars->SetWindowsTitleText("GLTF Export");
	m_progressBars->SetPreparingSceneText(true);

	fireRenderContext->setCallbackCreationDisabled(true);
	fireRenderContext->SetGLTFExport(true);

	fireRenderContext->SetWorkProgressCallback([this](const ContextWorkProgressData& syncProgressData)
		{
			if (syncProgressData.progressType == ProgressType::ObjectSyncComplete)
			{
				m_progressBars->update(syncProgressData.GetPercentProgress());
			}
		});

	if (!fireRenderContext->buildScene(false, false, true))
	{
		return MS::kFailure;
	}

	// Some resolution should be set. It's requred to retrieve background image.
	FireRenderGlobalsData renderData;
	
	unsigned int width = 800;	// use some defaults
	unsigned int height = 600;
	GetResolutionFromCommonTab(width, height);

	fireRenderContext->setResolution(width, height, true, 0);
	fireRenderContext->ConsiderSetupDenoiser();

	MStatus status;

	MDagPathArray renderableCameras = GetSceneCameras(true);
	if (renderableCameras.length() == 0)
	{
		MGlobal::displayError("No renderable cameras! Please setup at least one to be exported in GLTF!");
		return MS::kFailure;
	}

	fireRenderContext->setCamera(renderableCameras[0]);
	
	fireRenderContext->SetState(FireRenderContext::StateRendering);

	//Populate rpr scene with actual data
	if (!fireRenderContext->Freshen(false))
		return MS::kFailure;

	frw::Scene scene = fireRenderContext->GetScene();
	frw::Context context = fireRenderContext->GetContext();
	frw::MaterialSystem materialSystem = fireRenderContext->GetMaterialSystem();

	if (!scene || !context || !materialSystem)
		return MS::kFailure;

	try
	{
		m_progressBars->SetTextAboveProgress("Preparing Animation...", true);

		animationExporter.Export(*fireRenderContext, &renderableCameras);

		std::vector<rpr_scene> scenes;
		scenes.push_back(scene.Handle());

		m_progressBars->SetTextAboveProgress("Saving to GLTF file", true);
		int err = rprExportToGLTF(
			file.expandedFullName().asChar(),
			context.Handle(),
			materialSystem.Handle(),
			scenes.data(),
			scenes.size(),
			RPRGLTF_EXPORTFLAG_COPY_IMAGES_USING_OBJECTNAME | RPRGLTF_EXPORTFLAG_KHR_LIGHT);

		if (err != GLTF_SUCCESS)
		{
			return MS::kFailure;
		}

		m_progressBars->SetTextAboveProgress("Done.");
	}
	catch (ExportCancelledException)
	{
		MGlobal::displayWarning("GLTF export was cancelled by the user");
		return MS::kFailure;
	}

	return MS::kSuccess;
}

