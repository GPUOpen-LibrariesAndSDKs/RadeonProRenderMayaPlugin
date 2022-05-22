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

#include <map>

using namespace FireMaya;

GLTFTranslator::GLTFTranslator() = default;
GLTFTranslator::~GLTFTranslator() = default;

void* GLTFTranslator::creator()
{
	return new GLTFTranslator();
}

typedef std::map<std::string, std::string> OptionMap;

void ParseOptionStringValues(OptionMap& optionMap, const MString& optionsString)
{
	MStringArray options;
	optionsString.split(';', options);

	MStringArray oneOption;

	for (unsigned int i = 0; i < options.length(); ++i)
	{
		options[i].split('=', oneOption);

		unsigned int len = oneOption.length();

		if (len != 2)
		{
			assert(false);
			continue;
		}

		optionMap[oneOption[0].asChar()] = oneOption[1].asChar();
	}
}

MStatus	GLTFTranslator::writer(const MFileObject& file,
	const MString& optionsString,
	FileAccessMode mode)
{
	// Create new context and fill it with scene
	std::unique_ptr<NorthStarContext> fireRenderContext = std::make_unique<NorthStarContext>();
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
	fireRenderContext->TryCreateDenoiserImageFilters();

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

	// create rprs context
	frw::RPRSContext rprsContext;

	try
	{
		m_progressBars->SetTextAboveProgress("Preparing Animation...", true);

		animationExporter.Export(*fireRenderContext, &renderableCameras, rprsContext);

		std::vector<rpr_scene> scenes;
		scenes.push_back(scene.Handle());

		OptionMap optionMap;
		ParseOptionStringValues(optionMap, optionsString);

		unsigned int gltfFlags = RPRGLTF_EXPORTFLAG_COPY_IMAGES_USING_OBJECTNAME | RPRGLTF_EXPORTFLAG_KHR_LIGHT;

		OptionMap::const_iterator it = optionMap.find("BuildPbrImages");
		if (it != optionMap.end())
		{
			if (MString(it->second.c_str()).asInt() > 0)
			{
				gltfFlags |= RPRGLTF_EXPORTFLAG_BUILD_STD_PBR_IMAGES;
			}
		}

		m_progressBars->SetTextAboveProgress("Saving to GLTF file", true);
		int err = rprExportToGLTF(
			file.expandedFullName().asChar(),
			context.Handle(),
			materialSystem.Handle(),
			scenes.data(),
			scenes.size(),
			gltfFlags, rprsContext.Handle());

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

