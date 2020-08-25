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
#include "FireRenderExportCmd.h"
#include "Context/TahoeContext.h"
#include "FireRenderUtils.h"
#include "RenderStampUtils.h"

#include <maya/MArgDatabase.h>
#include <maya/MItDag.h>
#include <maya/MDagPath.h>
#include <maya/MGlobal.h>
#include <maya/MSelectionList.h>
#include <maya/MObjectArray.h>
#include <maya/MArgList.h>
#include <maya/MStatus.h>
#include <maya/MDagPathArray.h>
#include <maya/MTime.h>
#include <maya/MAnimControl.h>
#include <maya/MRenderUtil.h>
#include <maya/MCommonRenderSettingsData.h>
#include <maya/MFnRenderLayer.h>

#include <fstream>
#include <regex>

#ifdef __linux__
	#include <../RprLoadStore.h>
#else
	#include <../inc/RprLoadStore.h>
#endif
#include <iomanip>

FireRenderExportCmd::FireRenderExportCmd()
{
}

FireRenderExportCmd::~FireRenderExportCmd()
{
}

void * FireRenderExportCmd::creator()
{
	return new FireRenderExportCmd;
}

MSyntax FireRenderExportCmd::newSyntax()
{
	MStatus status;
	MSyntax syntax;

	CHECK_MSTATUS(syntax.addFlag(kMaterialFlag, kMaterialFlagLong, MSyntax::kString));
	CHECK_MSTATUS(syntax.addFlag(kFilePathFlag, kFilePathFlagLong, MSyntax::kString));
	CHECK_MSTATUS(syntax.addFlag(kSelectionFlag, kSelectionFlagLong, MSyntax::kNoArg));
	CHECK_MSTATUS(syntax.addFlag(kAllFlag, kAllFlagLong, MSyntax::kNoArg));
	CHECK_MSTATUS(syntax.addFlag(kFramesFlag, kFramesFlagLong, MSyntax::kBoolean, MSyntax::kLong, MSyntax::kLong, MSyntax::kBoolean));
	CHECK_MSTATUS(syntax.addFlag(kCompressionFlag, kCompressionFlagLong, MSyntax::kString));
	CHECK_MSTATUS(syntax.addFlag(kPadding, kPaddingLong, MSyntax::kString, MSyntax::kLong));
	CHECK_MSTATUS(syntax.addFlag(kSelectedCamera, kSelectedCameraLong, MSyntax::kString));

	return syntax;
}

bool SaveExportConfig(std::string filePath, TahoeContext& ctx, std::string fileName)
{
	// get directory path and name of generated files
	std::string directory = filePath;
	const size_t lastIdx = filePath.rfind('/');
	if (std::string::npos != lastIdx)
	{
		directory = filePath.substr(0, lastIdx);
	}
	fileName.erase(0, directory.length() + 1);
	directory += "/config.json";
	std::ofstream json(directory);
	if (!json)
		return false;

	json << "{" << std::endl;

	json << "\"output\" : " << "\"" << fileName << ".png\",\n";

	json << "\"output.json\" : \"output.json\",\n";

	// write file data fields
	json << "\"width\" : " << ctx.width() << ",\n";
	json << "\"height\" : " << ctx.height() << ",\n";

	bool isUnlimitedIterations = ctx.getCompletionCriteria().isUnlimitedIterations();
	if (isUnlimitedIterations)
	{
		json << "\"iterations\" : " << 100 << ",\n";
	}
	else
	{
		json << "\"iterations\" : " << ctx.getCompletionCriteria().completionCriteriaMaxIterations << ",\n";
	}

	json << "\"gamma\" : " << 1 << ",\n";

	// - aovs
	static std::map<unsigned int, std::string> aov2name =
	{
		 {RPR_AOV_COLOR, "color"}
		,{RPR_AOV_OPACITY, "opacity" }
		,{RPR_AOV_WORLD_COORDINATE, "world.coordinate" }
		,{RPR_AOV_UV, "uv" }
		,{RPR_AOV_MATERIAL_IDX, "material.id" }
		,{RPR_AOV_GEOMETRIC_NORMAL, "normal.geom" }
		,{RPR_AOV_SHADING_NORMAL, "normal" }
		,{RPR_AOV_DEPTH, "depth" }
		,{RPR_AOV_OBJECT_ID, "object.id" }
		,{RPR_AOV_OBJECT_GROUP_ID, "group.id" }
		,{RPR_AOV_SHADOW_CATCHER, "shadow.catcher" }
		,{RPR_AOV_BACKGROUND, "background" }
		,{RPR_AOV_EMISSION, "emission" }
		,{RPR_AOV_VELOCITY, "velocity" }
		,{RPR_AOV_DIRECT_ILLUMINATION, "direct.illumination" }
		,{RPR_AOV_INDIRECT_ILLUMINATION, "indirect.illumination"}
		,{RPR_AOV_AO, "ao" }
		,{RPR_AOV_DIRECT_DIFFUSE, "direct.diffuse" }
		,{RPR_AOV_DIRECT_REFLECT, "direct.reflect" }
		,{RPR_AOV_INDIRECT_DIFFUSE, "indirect.diffuse" }
		,{RPR_AOV_INDIRECT_REFLECT, "indirect.reflect" }
		,{RPR_AOV_REFRACT, "refract" }
		,{RPR_AOV_VOLUME, "volume" }
		,{RPR_AOV_LIGHT_GROUP0, "light.group0" }
		,{RPR_AOV_LIGHT_GROUP1, "light.group1" }
		,{RPR_AOV_LIGHT_GROUP2, "light.group2" }
		,{RPR_AOV_LIGHT_GROUP3, "light.group3" }
		,{RPR_AOV_DIFFUSE_ALBEDO, "albedo.diffuse" }
		,{RPR_AOV_VARIANCE, "variance" }
		,{RPR_AOV_VIEW_SHADING_NORMAL, "normal.view" }
		,{RPR_AOV_REFLECTION_CATCHER, "reflection.catcher" }
		,{RPR_AOV_MAX, "RPR_AOV_MAX" }
	};

	FireRenderGlobalsData globals;
	globals.readFromCurrentScene();
	FireRenderAOVs& aovsGlobal = globals.aovs;
	aovsGlobal.applyToContext(ctx);

	std::vector<std::string> aovs;
	for (auto aov = RPR_AOV_OPACITY; aov != RPR_AOV_MAX; aov++)
	{
		auto it = aov2name.find(aov);
		if (it == aov2name.end())
			continue;

		if (!ctx.isAOVEnabled(aov))
			continue;

		aovs.push_back(it->second);
	}

	// aovs
	auto aov = aovs.begin();
	if (aov != aovs.end())
	{
		json << "\"aovs\" : {\n";
		json << "\"" << *aov << "\":\"" << (*aov + ".png") << "\"";
		++aov;

		for (; aov != aovs.end(); ++aov)
		{
			json << ",\n" << "\"" << *aov << "\":\"" << (*aov + ".png") << "\"";
		}
		json << "\n}," << std::endl;
	}

	// - devices
	std::vector<std::pair<std::string, int>> context;
	MIntArray devicesUsing;
	MGlobal::executeCommand("optionVar -q RPR_DevicesSelected", devicesUsing);
	std::vector<HardwareResources::Device> allDevices = HardwareResources::GetAllDevices();
	size_t numDevices = std::min<size_t>(devicesUsing.length(), allDevices.size());
	for (size_t idx = 0; idx < numDevices; ++idx)
	{
		std::string device("gpu");
		device += std::to_string(idx);
		context.emplace_back();
		context.back().first = device;
		context.back().second = (devicesUsing[(unsigned int)idx] != 0);
	}
	context.emplace_back("debug", 0);

	json << "\"context\" : {\n";
	auto it = context.begin();
	json << "\"" << it->first << "\":" << it->second;
	++it;
	for (; it != context.end(); ++it)
	{
		json << ",\n" << "\"" << it->first << "\":" << it->second;
	}
	json << "\n}" << std::endl;

	json << "}" << std::endl;
	json.close();

	return true;
}

MStatus FireRenderExportCmd::doIt(const MArgList & args)
{
	MStatus status;

	MArgDatabase argData(syntax(), args);

	// for the moment, the LoadStore library of RPR only supports the export/import of all scene
	if (  !argData.isFlagSet(kAllFlag)  )
	{
		MGlobal::displayError("This feature is not supported.");
		return MS::kFailure;
	}

	MString inFilePath;
	if (argData.isFlagSet(kFilePathFlag))
	{
		argData.getFlagArgument(kFilePathFlag, 0, inFilePath);
	}
	else
	{
		MGlobal::displayError("File path is missing, use -file flag");
		return MS::kFailure;
	}

	std::string filePath = ProcessEnvVarsInFilePath(inFilePath);

	MString materialName;
	if (argData.isFlagSet(kMaterialFlag))
	{
		argData.getFlagArgument(kMaterialFlag, 0, materialName);

		MSelectionList sList;
		sList.add(materialName);

		MObject node;
		sList.getDependNode(0, node);
		if (node.isNull())
		{
			MGlobal::displayError("Invalid shader");
			return MS::kFailure;
		}

		TahoeContext context;
		context.setCallbackCreationDisabled(true);

		rpr_int res = context.initializeContext();
		if (res != RPR_SUCCESS)
		{
			MString msg;
			FireRenderError(res, msg, true);
			return MS::kFailure;
		}

		context.setCallbackCreationDisabled(true);

		auto shader = context.GetShader(node);
		if (!shader)
		{
			MGlobal::displayError("Invalid shader");
			return MS::kFailure;
		}

		MGlobal::displayError("This feature is not supported.\n");
		return MS::kFailure;
	}

	bool isSequenceExportEnabled = false;
	int firstFrame = 1;
	int lastFrame = 1;
	bool isExportAsSingleFileEnabled = false;
	if (argData.isFlagSet(kFramesFlag))
	{
		argData.getFlagArgument(kFramesFlag, 0, isSequenceExportEnabled);
		argData.getFlagArgument(kFramesFlag, 1, firstFrame);
		argData.getFlagArgument(kFramesFlag, 2, lastFrame);
		argData.getFlagArgument(kFramesFlag, 3, isExportAsSingleFileEnabled);
	}

	MString compressionOption = "None";
	if (argData.isFlagSet(kCompressionFlag))
	{
		argData.getFlagArgument(kCompressionFlag, 0, compressionOption);
	}

	if (argData.isFlagSet(kAllFlag))
	{
		// initialize
		MCommonRenderSettingsData settings;
		MRenderUtil::getCommonRenderSettings(settings);

		TahoeContext context;
		context.SetRenderType(RenderType::ProductionRender);
		context.buildScene();

		context.setResolution(settings.width, settings.height, true);
		context.ConsiderSetupDenoiser();

		MDagPathArray cameras = GetSceneCameras();
		unsigned int countCameras = cameras.length();

		if (countCameras == 0)
		{
			MGlobal::displayError("Renderable cameras haven't been found! Using default camera!");

			MDagPath cameraPath = getDefaultCamera();
			MString cameraName = getNameByDagPath(cameraPath);
			context.setCamera(cameraPath, true);
		}
		else  // (cameras.length() >= 1)
		{
			MString selectedCameraName;
			MStatus res = argData.getFlagArgument(kSelectedCamera, 0, selectedCameraName);
			if (res != MStatus::kSuccess)
			{
				context.setCamera(cameras[0], true);
			}
			else
			{
				unsigned int selectedCameraIdx = 0;
				context.setCamera(cameras[selectedCameraIdx], true);

				for (; selectedCameraIdx < countCameras; ++selectedCameraIdx)
				{
					MDagPath& cameraPath = cameras[selectedCameraIdx];
					MString cameraName = getNameByDagPath(cameraPath);
					if (selectedCameraName == cameraName)
					{
						context.setCamera(cameras[selectedCameraIdx], true);
						break;
					}
				}
			}
		}

		// setup frame ranges
		if (!isSequenceExportEnabled)
		{
			firstFrame = lastFrame = 1;
		}

		// process file path
		std::string fileName;
		std::string fileExtension = "rpr";

		// Remove extension from file name, because it would be added later
		size_t fileExtensionIndex = filePath.find("." + fileExtension);
		bool fileExtensionNotProvided = fileExtensionIndex == -1;

		if (fileExtensionNotProvided)
		{
			fileName = filePath;
		}
		else
		{
			fileName = filePath.substr(0, fileExtensionIndex);
		}

		// read file name pattern and padding
		if (isSequenceExportEnabled && !argData.isFlagSet(kPadding))
		{
			MGlobal::displayError("Can't export sequence without setting name pattern and padding!");
			return MS::kFailure;
		}

		MString namePattern;
		argData.getFlagArgument(kPadding, 0, namePattern);
		unsigned int framePadding = 0;
		argData.getFlagArgument(kPadding, 1, framePadding);

		// process each frame
		for (int frame = firstFrame; frame <= lastFrame; ++frame)
		{
			// Move the animation to the next frame.
			MTime time;
			time.setValue(static_cast<double>(frame));
			MStatus isTimeSet = MGlobal::viewFrame(time);
			CHECK_MSTATUS(isTimeSet);

			// Refresh the context so it matches the
			// current animation state and start the render.
			context.Freshen();

			// update file path
			std::string newFilePath;
			if (isSequenceExportEnabled)
			{
				std::regex name_regex("name");
				std::regex frame_regex("#");
				std::regex extension_regex("ext");

				std::string pattern = namePattern.asChar();

				// Replace extension at first, because it shouldn't match name_regex or frame_regex for given .rpr format
				std::string result = std::regex_replace(pattern, extension_regex, fileExtension);

				std::stringstream frameStream;
				frameStream << std::setfill('0') << std::setw(framePadding) << frame;
				result = std::regex_replace(result, frame_regex, frameStream.str().c_str());

				// Replace name after all operations, because it could match frame or extension regex
				result = std::regex_replace(result, name_regex, fileName);

				newFilePath = result.c_str();
			}
			else
			{
				newFilePath = fileName + "." + fileExtension;
			}

			unsigned int exportFlags = 0;
			if (!isExportAsSingleFileEnabled)
			{
				exportFlags = RPRLOADSTORE_EXPORTFLAG_EXTERNALFILES;
			}

			if (compressionOption == "None")
			{
				// don't set any flag
				// this line exists for better logic readibility; also maybe will need to set flag here in the future
			}
			else if (compressionOption == "Level 1")
			{
				exportFlags = exportFlags | RPRLOADSTORE_EXPORTFLAG_COMPRESS_IMAGE_LEVEL_1;
			}
			else if (compressionOption == "Level 2")
			{
				exportFlags = exportFlags | RPRLOADSTORE_EXPORTFLAG_COMPRESS_IMAGE_LEVEL_2;
			}
			else if (compressionOption == "Level 3")
			{
				exportFlags = exportFlags | RPRLOADSTORE_EXPORTFLAG_COMPRESS_IMAGE_LEVEL_2 | RPRLOADSTORE_EXPORTFLAG_COMPRESS_FLOAT_TO_HALF_NORMALS | RPRLOADSTORE_EXPORTFLAG_COMPRESS_FLOAT_TO_HALF_UV;
			}

			#if RPR_VERSION_MAJOR_MINOR_REVISION >= 0x00103404 
			// Always using this flag by default doesn't hurt :
			// If rprObjectSetName(<path to image file>) has been called on all rpr_image, the performance of export is really better ( ~100x faster )
			// If <path to image file> has not been set, or doesn't exist, then data from RPR is used to export the image.
			exportFlags = exportFlags | RPRLOADSTORE_EXPORTFLAG_EMBED_FILE_IMAGES_USING_OBJECTNAME;
			#endif

			// launch export
			rpr_int statusExport = rprsExport(newFilePath.c_str(), context.context(), context.scene(),
				0, 0, 0, 0, 0, 0, exportFlags);

			// save config
			bool res = SaveExportConfig(filePath.c_str(), context, fileName.c_str());
			if (!res)
			{
				MGlobal::displayError("Unable to export render config!\n");
			}

			if (statusExport != RPR_SUCCESS)
			{
				MGlobal::displayError("Unable to export fire render scene\n");
				return MS::kFailure;
			}
		}

		return MS::kSuccess;
	}

	if (argData.isFlagSet(kSelectionFlag))
	{
		//initialize
		TahoeContext context;
		context.setCallbackCreationDisabled(true);
		context.buildScene();

		MDagPathArray cameras = GetSceneCameras(true);
		if ( cameras.length() >= 1 )
		{
			context.setCamera(cameras[0]);
		}

		MSelectionList sList;
		std::vector<rpr_shape> shapes;
		std::vector<rpr_light> lights;
		rpr_light envLight = nullptr;

		MGlobal::getActiveSelectionList(sList);
		for (unsigned int i = 0; i < sList.length(); i++)
		{
			MDagPath path;

			sList.getDagPath(i, path);
			path.extendToShape();

			MObject node = path.node();
			if (node.isNull())
				continue;

			MFnDependencyNode nodeFn(node);

			FireRenderObject* frObject = context.getRenderObject(getNodeUUid(node));
			if (!frObject)
				continue;

			if (auto light = dynamic_cast<FireRenderLight*>(frObject))
			{
				if (light->data().isAreaLight && light->data().areaLight)
				{
					shapes.push_back(light->data().areaLight.Handle());
				}
				else
				{
					if (light->data().light)
					{
						lights.push_back(light->data().light.Handle());
					}
				}
			}

			else if (auto mesh = dynamic_cast<FireRenderMesh*>(frObject))
			{
				for (auto element : mesh->Elements())
				{
					shapes.push_back(element.shape.Handle());
				}
			}
			else if (auto frEnvLight = static_cast<FireRenderEnvLight*>(frObject))
			{
				envLight = frEnvLight->data().Handle();
			}

		}

		MGlobal::displayError("This feature is not supported.\n");
		return MS::kFailure;
	}

	return MS::kFailure;
}
