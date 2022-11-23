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
#include "Context/ContextCreator.h"
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
#include "AnimationExporter.h"
#include "MayaStandardNodesSupport/FileNodeConverter.h"

#include <fstream>
#include <regex>

#ifdef __linux__
	#include <../RprLoadStore.h>
#else
	#include <../inc/RprLoadStore.h>
#endif
#include <iomanip>

#include <codecvt>
#include <locale>

#include "Utils/Utils.h"

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
	CHECK_MSTATUS(syntax.addFlag(kFramesFlag, kFramesFlagLong, MSyntax::kBoolean, MSyntax::kLong, MSyntax::kLong, MSyntax::kBoolean, MSyntax::kBoolean, MSyntax::kBoolean));
	CHECK_MSTATUS(syntax.addFlag(kCompressionFlag, kCompressionFlagLong, MSyntax::kString));
	CHECK_MSTATUS(syntax.addFlag(kPadding, kPaddingLong, MSyntax::kString, MSyntax::kLong));
	CHECK_MSTATUS(syntax.addFlag(kSelectedCamera, kSelectedCameraLong, MSyntax::kString));
	CHECK_MSTATUS(syntax.addFlag(kLayerExportFlag, kLayerExportFlagLong, MSyntax::kNoArg));

	return syntax; 
}

std::string GetPluginLibrary(bool isRPR2)
{
	std::string pluginExtension;
	std::string pluginPrefix;

	std::string pluginDll = isRPR2 ? "Northstar64" : "Tahoe64";

#ifdef WIN32
	pluginDll += ".dll";
#else
	pluginDll = "lib" + pluginDll + ".dylib";
#endif

	return pluginDll;
}

bool SaveExportConfig(const std::wstring& filePath, NorthStarContext& ctx, const std::wstring& fileName)
{
	std::wstring configName = std::regex_replace(filePath, std::wregex(L"rpr$"), L"json");

	bool isRPR2 = NorthStarContext::IsGivenContextNorthStar(&ctx);

#ifdef WIN32
	// MSVS added an overload to accommodate using open with wide strings where xcode did not.
	std::wofstream json(configName.c_str());
#else
	// thus different path for xcode is needed
	std::string s_directory = SharedComponentsUtils::ws2s(configName);
	std::wofstream json(s_directory);
#endif

	if (!json)
		return false;

	std::string pluginDll = GetPluginLibrary(isRPR2);

	const std::locale utf8_locale = std::locale(std::locale(), new std::codecvt_utf8<wchar_t>());
	json.imbue(utf8_locale);

	json << "{" << std::endl;

	json << "\"plugin\" : \"" << pluginDll.c_str() << "\",\n";

	std::wstring outputName = std::regex_replace(filePath, std::wregex(L"rpr$"), L"png");
	json << "\"output\" : " << "\"" << outputName.c_str() << "\",\n";

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

	float gammaValue = 1.8f;

	MString renderingSpaceName;
	MStatus colorManagementStatus = MGlobal::executeCommand(MString("colorManagementPrefs -q -otn;"), renderingSpaceName);
	if (colorManagementStatus == MStatus::kSuccess)
	{
		gammaValue = MayaStandardNodeConverters::FileNodeConverter::ColorSpace2Gamma(renderingSpaceName);
	}

	if (ctx.Globals().applyGammaToMayaViews)
	{
		gammaValue = gammaValue * ctx.Globals().displayGamma;
	}
	json << "\"gamma\" : " << gammaValue << ",\n";

	// - aovs
	static std::map<unsigned int, std::wstring> aov2name =
	{
		 {RPR_AOV_COLOR, L"color"}
		,{RPR_AOV_OPACITY, L"opacity" }
		,{RPR_AOV_WORLD_COORDINATE, L"world.coordinate" }
		,{RPR_AOV_UV, L"uv" }
		,{RPR_AOV_MATERIAL_ID, L"material.id" }
		,{RPR_AOV_GEOMETRIC_NORMAL, L"normal.geom" }
		,{RPR_AOV_SHADING_NORMAL, L"normal" }
		,{RPR_AOV_DEPTH, L"depth" }
		,{RPR_AOV_OBJECT_ID, L"object.id" }
		,{RPR_AOV_OBJECT_GROUP_ID, L"group.id" }
		,{RPR_AOV_SHADOW_CATCHER, L"shadow.catcher" }
		,{RPR_AOV_BACKGROUND, L"background" }
		,{RPR_AOV_EMISSION, L"emission" }
		,{RPR_AOV_VELOCITY, L"velocity" }
		,{RPR_AOV_DIRECT_ILLUMINATION, L"direct.illumination" }
		,{RPR_AOV_INDIRECT_ILLUMINATION, L"indirect.illumination"}
		,{RPR_AOV_AO, L"ao" }
		,{RPR_AOV_DIRECT_DIFFUSE, L"direct.diffuse" }
		,{RPR_AOV_DIRECT_REFLECT, L"direct.reflect" }
		,{RPR_AOV_INDIRECT_DIFFUSE, L"indirect.diffuse" }
		,{RPR_AOV_INDIRECT_REFLECT, L"indirect.reflect" }
		,{RPR_AOV_REFRACT, L"refract" }
		,{RPR_AOV_VOLUME, L"volume" }
		,{RPR_AOV_LIGHT_GROUP0, L"light.group0" }
		,{RPR_AOV_LIGHT_GROUP1, L"light.group1" }
		,{RPR_AOV_LIGHT_GROUP2, L"light.group2" }
		,{RPR_AOV_LIGHT_GROUP3, L"light.group3" }
		,{RPR_AOV_DIFFUSE_ALBEDO, L"albedo.diffuse" }
		,{RPR_AOV_VARIANCE, L"variance" }
		,{RPR_AOV_VIEW_SHADING_NORMAL, L"normal.view" }
		,{RPR_AOV_REFLECTION_CATCHER, L"reflection.catcher" }
		,{RPR_AOV_CAMERA_NORMAL, L"camera.normal" }
		,{RPR_AOV_MAX, L"RPR_AOV_MAX" }
	};

	FireRenderGlobalsData globals;
	globals.readFromCurrentScene();
	FireRenderAOVs& aovsGlobal = globals.aovs;
	aovsGlobal.applyToContext(ctx);

	std::vector<std::wstring> aovs;
	for (auto aov = RPR_AOV_OPACITY; aov < RPR_AOV_MAX; aov++)
	{
		auto it = aov2name.find(aov);
		if (it == aov2name.end())
			continue;

		if (!ctx.isAOVEnabled(aov))
			continue;

		// shin request
		if (aov == RPR_AOV_CAMERA_NORMAL && !isRPR2)
			continue;

		aovs.push_back(it->second);
	}

	// aovs
	auto aov = aovs.begin();
	if (aov != aovs.end())
	{
		json << "\"aovs\" : {\n";
		json << "\"" << *aov << "\":\"" << (*aov + L".png") << "\"";
		++aov;

		for (; aov != aovs.end(); ++aov)
		{
			json << ",\n" << "\"" << *aov << "\":\"" << (*aov + L".png") << "\"";
		}
		json << "\n}," << std::endl;
	}

	// - contour
	if (globals.contourIsEnabled)
	{
		json << "\"contour\" : {\n";

		json << "\"object.id\" : " << (globals.contourUseObjectID ? 1 : 0) << ",\n";
		json << "\"material.id\" : " << (globals.contourUseMaterialID ? 1 : 0) << ",\n";
		json << "\"normal\" : " << (globals.contourUseShadingNormal ? 1 : 0) << ",\n";
		json << "\"uv\" : " << (globals.contourUseUV ? 1 : 0) << ",\n";

		json << "\"threshold.normal\" : " << globals.contourNormalThreshold << ",\n";
		json << "\"threshold.uv\" : " << globals.contourUVThreshold << ",\n";

		json << "\"linewidth.objid\" : " << globals.contourLineWidthObjectID << ",\n";
		json << "\"linewidth.matid\" : " << globals.contourLineWidthMaterialID << ",\n";
		json << "\"linewidth.normal\" : " << globals.contourLineWidthShadingNormal << ",\n";
		json << "\"linewidth.uv\" : " << globals.contourLineWidthUV << ",\n";

		json << "\"antialiasing\" : " << globals.contourAntialiasing << ",\n";

		json << "\"debug\" : " << (globals.contourIsDebugEnabled ? 1 : 0);

		json << "\n}," << std::endl;
	}

	// - devices
	std::vector<std::pair<std::wstring, int>> context;
	MIntArray devicesUsing;
	MGlobal::executeCommand("optionVar -q RPR_DevicesSelected", devicesUsing);
	std::vector<HardwareResources::Device> allDevices = HardwareResources::GetAllDevices();
	size_t numDevices = std::min<size_t>(devicesUsing.length(), allDevices.size());
	for (size_t idx = 0; idx < numDevices; ++idx)
	{
		std::wstring device(L"gpu");
		device += std::to_wstring(idx);
		context.emplace_back();
		context.back().first = device;
		context.back().second = (devicesUsing[(unsigned int)idx] != 0);
	}
	context.emplace_back(L"debug", 0);

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

unsigned int SetupExportFlags(bool isExportAsSingleFileEnabled, bool isIncludeTextureCacheEnabled, MString& compressionOption)
{
	unsigned int exportFlags = 0;
	if (!isExportAsSingleFileEnabled)
	{
		exportFlags = RPRLOADSTORE_EXPORTFLAG_EXTERNALFILES;
	}

	if (isIncludeTextureCacheEnabled)
	{
		exportFlags = exportFlags | RPRLOADSTORE_EXPORTFLAG_USE_IMAGE_CACHE;
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

	return exportFlags;
}

MStatus FireRenderExportCmd::doIt(const MArgList& args)
{
	MStatus status;

	MArgDatabase argData(syntax(), args);

	// for the moment, the LoadStore library of RPR only supports the export/import of all scene
	if (!argData.isFlagSet(kAllFlag))
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

	MString processedFilePath; // using MString to convert between char and wchar because why not?
	if (IsUnicodeSystem())
	{
		processedFilePath = ProcessEnvVarsInFilePath<std::wstring, wchar_t>(inFilePath.asWChar()).c_str();
	}
	else
	{
		processedFilePath = ProcessEnvVarsInFilePath<std::string, char>(inFilePath.asChar()).c_str();
	}

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

		NorthStarContextPtr northStarContextPtr = ContextCreator::CreateNorthStarContext();

		northStarContextPtr->setCallbackCreationDisabled(true);

		rpr_int res = northStarContextPtr->initializeContext();
		if (res != RPR_SUCCESS)
		{
			MString msg;
			FireRenderError(res, msg, true);
			return MS::kFailure;
		}

		northStarContextPtr->setCallbackCreationDisabled(true);

		auto shader = northStarContextPtr->GetShader(node);
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
	bool isIncludeTextureCacheEnabled = false;
	bool isAnimationAsSingleFileEnabled = false;

	if (argData.isFlagSet(kFramesFlag))
	{
		argData.getFlagArgument(kFramesFlag, 0, isSequenceExportEnabled);
		argData.getFlagArgument(kFramesFlag, 1, firstFrame);
		argData.getFlagArgument(kFramesFlag, 2, lastFrame);
		argData.getFlagArgument(kFramesFlag, 3, isExportAsSingleFileEnabled);
		argData.getFlagArgument(kFramesFlag, 4, isIncludeTextureCacheEnabled);
		argData.getFlagArgument(kFramesFlag, 5, isAnimationAsSingleFileEnabled);
	}

	bool isAllLayersExportEnabled = argData.isFlagSet(kLayerExportFlag);

	MString compressionOption = "None";
	if (argData.isFlagSet(kCompressionFlag))
	{
		argData.getFlagArgument(kCompressionFlag, 0, compressionOption);
	}

	if (argData.isFlagSet(kAllFlag))
	{
		MObjectArray layers;
		MObject existingRenderLayer = MFnRenderLayer::currentLayer(); // save current layer to restore it after export
		if (isAllLayersExportEnabled)
		{
			MFnRenderLayer::listAllRenderLayers(layers); // will export all layers
		}
		else
		{
			layers.append(existingRenderLayer); // will export only current layer
		}

		// process each layer
		for (MObject layer : layers)
		{
			// setup layer to export
			MFnDependencyNode layerNodeFn(layer);
			MGlobal::executeCommand("editRenderLayerGlobals -currentRenderLayer " + layerNodeFn.name(), false, true);

			// initialize
			MCommonRenderSettingsData settings;
			MRenderUtil::getCommonRenderSettings(settings);

			NorthStarContextPtr northStarContextPtr = ContextCreator::CreateNorthStarContext();
			AnimationExporter animationExporter(false);

			northStarContextPtr->SetRenderType(RenderType::ProductionRender);

			MDagPathArray cameras = GetSceneCameras();
			unsigned int countCameras = cameras.length();

			if (countCameras == 0)
			{
				MGlobal::displayError("Renderable cameras haven't been found! Using default camera!");

				MDagPath cameraPath = getDefaultCamera();
				MString cameraName = getNameByDagPath(cameraPath);
				northStarContextPtr->setCamera(cameraPath, true);
			}
			else  // (cameras.length() >= 1)
			{
				MString selectedCameraName;
				MStatus res = argData.getFlagArgument(kSelectedCamera, 0, selectedCameraName);
				if (res != MStatus::kSuccess)
				{
					northStarContextPtr->setCamera(cameras[0], true);
				}
				else
				{
					unsigned int selectedCameraIdx = 0;
					northStarContextPtr->setCamera(cameras[selectedCameraIdx], true);

					for (; selectedCameraIdx < countCameras; ++selectedCameraIdx)
					{
						MDagPath& cameraPath = cameras[selectedCameraIdx];
						MString cameraName = getNameByDagPath(cameraPath);
						if (selectedCameraName == cameraName)
						{
							northStarContextPtr->setCamera(cameras[selectedCameraIdx], true);
							break;
						}
					}
				}
			}

			northStarContextPtr->buildScene(false, false, false);
			northStarContextPtr->setResolution(settings.width, settings.height, true);

			// setup frame ranges
			if (!isSequenceExportEnabled || isAnimationAsSingleFileEnabled)
			{
				lastFrame = firstFrame;
			}

			// process file path
			std::wstring fileName;
			std::wstring fileExtension = L"rpr";
			std::wstring filePath = processedFilePath.asWChar();

			// Remove extension from file name, because it would be added later
			size_t fileExtensionIndex = filePath.find(L"." + fileExtension);
			bool fileExtensionNotProvided = fileExtensionIndex == -1;

			if (fileExtensionNotProvided)
			{
				fileName = filePath;
			}
			else
			{
				fileName = filePath.substr(0, fileExtensionIndex);
			}

			// append layer name to filename
			// add support for different layer name suffix formats in the future
			if (isAllLayersExportEnabled)
			{
				MString layerSuffix = "_" + layerNodeFn.name();
				fileName = fileName + layerSuffix.asWChar();
			}

			// read file name pattern and padding
			if (isSequenceExportEnabled && !isAnimationAsSingleFileEnabled && !argData.isFlagSet(kPadding))
			{
				MGlobal::displayError("Can't export sequence without setting name pattern and padding!");
				return MS::kFailure;
			}

			MString namePattern;
			argData.getFlagArgument(kPadding, 0, namePattern);
			unsigned int framePadding = 0;
			argData.getFlagArgument(kPadding, 1, framePadding);

			// create rprs context
			frw::RPRSContext rprsContext;

			// process each frame
			for (int frame = firstFrame; frame <= lastFrame; ++frame)
			{
				MString commandPy = "maya.utils.processIdleEvents()";
				MGlobal::executePythonCommand(commandPy);

				// Move the animation to the next frame.
				if (isSequenceExportEnabled && !isAnimationAsSingleFileEnabled)
				{
					MTime time;
					time.setValue(static_cast<double>(frame));
					MStatus isTimeSet = MGlobal::viewFrame(time);
					CHECK_MSTATUS(isTimeSet);
				}

				// Refresh the context so it matches the
				// current animation state and start the render.
				northStarContextPtr->Freshen();

				// update file path
				std::wstring newFilePath;
				if (isSequenceExportEnabled && !isAnimationAsSingleFileEnabled)
				{
					//GetPattern();
					std::wstring name_regex;
					std::wstring frame_regex(L"#");
					std::wstring extension_regex;
					GetUINameFrameExtPattern(name_regex, extension_regex);
					std::wstring pattern = namePattern.asWChar();

					// Replace extension at first, because it shouldn't match name_regex or frame_regex for given .rpr format
					std::wstring result = std::regex_replace(pattern, std::wregex(extension_regex), fileExtension);

					std::wstringstream frameStream;
					frameStream << std::setfill(L'0') << std::setw(framePadding) << frame;
					result = std::regex_replace(result, std::wregex(frame_regex), frameStream.str().c_str());

					// Replace name after all operations, because it could match frame or extension regex
					result = std::regex_replace(result, std::wregex(name_regex), fileName);

					newFilePath = result.c_str();
				}
				else
				{
					newFilePath = fileName + L"." + fileExtension;

					// exporting animation as single file
					if (isSequenceExportEnabled)
					{
						animationExporter.Export(*northStarContextPtr, &cameras, rprsContext);
					}
				}

				// launch export
				rpr_int statusExport = rprsExport(MString(newFilePath.c_str()).asUTF8(), northStarContextPtr->context(), northStarContextPtr->scene(),
					0, 0, 0, 0, 0, 0, SetupExportFlags(isExportAsSingleFileEnabled, isIncludeTextureCacheEnabled, compressionOption),
					rprsContext.Handle());

				// save config
				bool res = SaveExportConfig(newFilePath, *northStarContextPtr, fileName);
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
		}
		// restore existing render layer
		MFnDependencyNode existingLayerNodeFn(existingRenderLayer);
		MGlobal::executeCommand("editRenderLayerGlobals -currentRenderLayer " + existingLayerNodeFn.name(), false, true);

		return MS::kSuccess;
	}

	if (argData.isFlagSet(kSelectionFlag))
	{
		//initialize
		NorthStarContextPtr northStarContextPtr = ContextCreator::CreateNorthStarContext();

		northStarContextPtr->setCallbackCreationDisabled(true);
		northStarContextPtr->buildScene();

		MDagPathArray cameras = GetSceneCameras(true);
		if (cameras.length() >= 1)
		{
			northStarContextPtr->setCamera(cameras[0]);
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

			FireRenderObject* frObject = northStarContextPtr->getRenderObject(getNodeUUid(node));
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
				if (!mesh->Elements().empty())
					shapes.push_back(mesh->Elements().back().shape.Handle());
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
