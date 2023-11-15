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
#include <maya/MFnPlugin.h>
#include <maya/MSwatchRenderRegister.h>
#include <maya/MGlobal.h>
#include <maya/MStatus.h>
#include <maya/MSceneMessage.h>
#include <maya/MFileIO.h>
#include <maya/MNodeClass.h>

#include "common.h"
#include "FireRenderMaterial.h"
#include "FireRenderBlendMaterial.h"
#include "FireRenderVolumeMaterial.h"
#include "FireRenderStandardMaterial.h"
#include "FireRenderPBRMaterial.h"
#include "FireRenderTransparentMaterial.h"
#include "FireRenderMaterialSwatchRender.h"
#include "FireRenderToonMaterial.h"
#include "FireRenderMaterialXMaterial.h"
#include "FireRenderSwatchInstance.h"

#include "FireRenderFresnel.h"
#include "FireMaterialViewRenderer.h"
#include "FireRenderGlobals.h"
#include "FireRenderCmd.h"
#include "FireRenderLocationCmd.h"
#include "EnableSaveIntermediateCmd.h"
#include "FireRenderIBL.h"
#include "FireRenderSkyLocator.h"
#include "Lights/IES/FireRenderIESLight.h"
#include "Lights/PhysicalLight/FireRenderPhysicalLightLocator.h"
#include "Lights/PhysicalLight/FireRenderPhysicalOverride.h"
#include "Volumes/FireRenderVolumeLocator.h"
#include "Volumes/FireRenderVolumeOverride.h"
#include "FireRenderEnvironmentLight.h"
#include "FireRenderOverride.h"
#include "FireRenderViewport.h"
#include "FireRenderViewportManager.h"

#include "FireRenderDisplacement.h"

#include "FireRenderViewportCmd.h"
#include "FireRenderExportCmd.h"
#include "FireRenderImportCmd.h"
#include "FireRenderImportMaterialX.h"
#include "FireRenderConvertVRayCmd.h"
#include "Athena/AthenaWrap.h"
#include "athenaCmd.h"

#include "FireRenderChecker.h"
#include "FireRenderArithmetic.h"
#include "FireRenderDot.h"
#include "FireRenderVoronoi.h"
#include "FireRenderBevel.h"
#include "FireRenderBlendValue.h"
#include "FireRenderGradient.h"
#include "FireRenderLookup.h"
#include "FireRenderTexture.h"
#include "FireRenderDoublesided.h"
#include "FireRenderFresnelSchlick.h"
#include "FireRenderNoise.h"
#include "SubsurfaceMaterial.h"
#include "FireRenderUtils.h"
#include "FireRenderShadowCatcherMaterial.h"
#include "FireRenderAO.h"

#include "FireRenderImportExportXML.h"
#include "FireRenderImageComparing.h"

#include <thread>
#include <sstream>
#include <iomanip>
#include "FireRenderPassthrough.h"
#include "FireRenderBump.h"
#include "FireRenderNormal.h"
#include "FireRenderRamp.h"

#include "FireRenderMaterialSwatchRender.h"

#include "FireRenderSurfaceOverride.h"
#include "FireRenderBlendOverride.h"
#include <maya/MCommonSystemUtils.h>
#include <maya/MNodeClass.h>

#include "FireRenderThread.h"

#include "GLTFTranslator.h"
#include "StartupContextChecker.h"

#ifdef _WIN32
#pragma warning( disable : 4091 )
#include <dbghelp.h>
# pragma comment(lib, "dbghelp.lib")
#pragma warning( default : 4091 )
#endif

using namespace FireMaya;

MCallbackId newSceneCallback;
MCallbackId openSceneCallback;

MCallbackId beforeNewSceneCallback;
MCallbackId beforeOpenSceneCallback;

MCallbackId mayaExitingCallback;

#ifdef _WIN32
static LPTOP_LEVEL_EXCEPTION_FILTER pTopLevelExceptionFilter = nullptr;

LONG WINAPI FrUnhandledExceptionFilter(EXCEPTION_POINTERS * pExceptionPointers)
{
	std::cout << "Exception code: " << std::hex << pExceptionPointers->ExceptionRecord->ExceptionCode << std::endl;

	CHAR filePath[MAX_PATH]{ 0 };

	::GetTempPath(MAX_PATH, filePath);
	sprintf_s(filePath, "%sMaya.dmp", filePath);

	auto hDumpFile = ::CreateFile(filePath,
		GENERIC_READ | GENERIC_WRITE,
		0,
		nullptr,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		nullptr);

	auto miniDumpExceptionInformation = MINIDUMP_EXCEPTION_INFORMATION
	{
		::GetCurrentThreadId(),
		pExceptionPointers,
		FALSE,
	};

	if (!::MiniDumpWriteDump(
		::GetCurrentProcess(),
		::GetCurrentProcessId(),
		hDumpFile,
		MiniDumpNormal,
		&miniDumpExceptionInformation,
		nullptr,
		nullptr))
	{
		auto error = ::GetLastError();

		CHAR message[MAX_PATH]{ 0 };
		sprintf_s(message, "Failed to create mini-dump: %u", error);
		::OutputDebugString(message);
		std::cout << message << std::endl;
	}
	else
	{
		std::cout << "Mini-dump created: " << filePath << std::endl;
	}

	::CloseHandle(hDumpFile);

	if (pTopLevelExceptionFilter)
		return pTopLevelExceptionFilter(pExceptionPointers);
	else
		return EXCEPTION_CONTINUE_SEARCH;
}

bool RPRInit();
void RPRRelease();

#endif // _WIN32

bool gExitingMaya = false;

void NewSceneBasicSetup(void* data)
{
	MGlobal::executeCommand("source \"common.mel\"; checkRPRGlobalsNode(); workingUnitsScriptJobSetup();");
	MGlobal::executeCommand("source \"AERPRToonMaterialTemplate.mel\"; ConvertLegacyLightLinkedAttribute();");
}

void swapToDefaultRenderOverride(void* data) {
	int numberOf3dViews = M3dView::numberOf3dViews();
	for (size_t i = 0; i < numberOf3dViews; i++)
	{
		M3dView temp;
		M3dView::get3dView((unsigned int)i, temp);
		MString currentROName = temp.renderOverrideName();
		if (currentROName == "FireRenderOverride") {
			temp.setRenderOverrideName("");
		}
	}
}

void mayaExiting(void* data)
{
	DebugPrint("mayaExiting");
	gExitingMaya = true;

	AthenaWrapper::GetAthenaWrapper()->Finalize();

    // Clear ViewportManager. It should be cleared before maya destroys OpenGL context
    // so we can't rely on ViewportManager destructor
    FireRenderViewportManager::instance().clear();

	if (!FireRenderSwatchInstance::IsCleaned())
	{
		FireRenderSwatchInstance::instance().cleanScene();
	}

	// We need to cleanup before terminating thread
	// For some reason Maya willn't call this method if we simply close Maya
	FireRenderCmd::cleanUp();

	FireRenderThread::RunTheThread(false);
	std::this_thread::yield();
}

void PluginUpdater()
{
	// TODO need a proper .json path for maya, the below path is an example
	MString pluginUpdateHttpPath = "https://radeon-prorender.github.io/rpr_renderer_plugin_maya_latest_version.json";

	MString pluginUpdatePy =
		"import urllib\n"
		"import json\n"
		"import maya.cmds as cmds\n"
		"import subprocess\n"
		"import os\n"
		"import urllib2\n"
		"\n"
		"def versionStringToNumbers(versionStr):\n"
		"	result = []\n"
		"	subVer = ''\n"
		"	for i in range(0,len(versionStr)):\n"
		"		if versionStr[i] != '.':\n"
		"			subVer += versionStr[i]\n"
		"\n"
		"	if versionStr[i] == '.' or i == len(versionStr) - 1:\n"
		"		result.append(int(subVer))\n"
		"		subVer = ''\n"
		"	return result\n"
		"\n"
		"def IsUserHaveOlderVersion(oldVersionStr, newVersionStr):\n"
		"	oldVersion = versionStringToNumbers(oldVersionStr)\n"
		"	newVersion = versionStringToNumbers(newVersionStr)\n"
		"\n"
		"	for i in range(0, len(oldVersion)):\n"
		"		oldVersion.append(0)\n"
		"\n"
		"	for i in range(0, len(newVersion)):\n"
		"		newVersion.append(0)\n"
		"\n"
		"	needUpdate = False\n"
		"	for i in range(0, len(oldVersion)):\n"
		"		if oldVersion[i] < newVersion[i]:\n"
		"			needUpdate = True\n"
		"			break\n"
		"		elif oldVersion[i] > newVersion[i]:\n"
		"			needUpdate = False\n"
		"			break\n"
		"	return needUpdate\n"
		"\n"
		"def progress(count, blockSize, totalSize):\n"
		"	cmds.progressWindow(edit = True, progress = int(count * blockSize * 100 / totalSize))\n"
		"	if cmds.progressWindow(query = True, isCancelled = True):\n"
		"		cmds.progressWindow(edit = True, endProgress = True)\n"
		"		sys.exit()\n"
		"\n"
		"try:\n"
		"	urlData = urllib.urlopen('" + pluginUpdateHttpPath + "').read();\n"
		"	jsonData = json.loads(urlData)\n"
		"except IOError:\n"
		"	sys.exit()\n"
		"\n"
		"if IsUserHaveOlderVersion('" + PLUGIN_VERSION + "', jsonData['version']):\n"
		"	text = 'New version of Radeon ProRender available.\\n\\nVersion:\\t%s \\nDate:\\t%s\\nChanges:\\t%s ' % (jsonData['version'], jsonData['date'], jsonData['changes'])\n"
		"	if jsonData['mustUpdate']:\n"
		"		result = cmds.confirmDialog(title = 'Update', message = text, button = ['Download Now'], defaultButton = 'Download Now')\n"
		"	else:\n"
		"		result = cmds.confirmDialog(title = 'Update', message = text, button = ['Download Now', 'Ask me later'], defaultButton = 'Download Now', cancelButton = 'Ask me later', dismissString = 'Ask me later')\n"
		"\n"
		"	if result == 'Download Now':\n"
		"		downloadPath = jsonData['url']\n"
		"		splits = downloadPath.split('/')\n"
		"		fileName = splits[len(splits) - 1]\n"
		"\n"
		"		ret = urllib2.urlopen(downloadPath)\n"
		"		if ret.code == 200:\n"
		"			cmds.progressWindow(title = 'Downloading...', progress = 0, status = '', isInterruptable = True)\n"
		"\n"
		"			urllib.urlretrieve(downloadPath, fileName, reporthook = progress)\n"
		"\n"
		"			cmds.progressWindow(edit = True, endProgress = True)\n"
		"\n"
		"			subprocess.call([fileName], shell = True)\n"
		"			os.remove(fileName)\n";

	MGlobal::executePythonCommand(pluginUpdatePy);
}

void DebugCallback(const char *sz)
{
	std::stringstream ss;
	ss << std::setbase(16) << std::setw(4) << std::this_thread::get_id() << ": " << sz << std::endl;

#ifdef _WIN32
	OutputDebugStringA(ss.str().c_str());
#elif __linux__
	MGlobal::displayInfo(ss.str().c_str());
#endif
}

void InfoCallback(const char *sz)
{
	MGlobal::displayInfo(sz);
}

class FireRenderRenderPass : public MPxNode {
public:

	static void* creator()
	{
		return new FireRenderRenderPass();
	}

	static MStatus initialize()
	{
		return MS::kSuccess;
	}

	static  MTypeId id;
};

MTypeId FireRenderRenderPass::id(FireMaya::TypeId::FireRenderRenderPass);

// Forward declaration for this function is in FireRenderCmd.cpp
// Wasn't able to implement it there due to link problems when including
// MFnPlugin from a few places
void RprExportsGLTF(bool enable)
{
	auto handle = MFnPlugin::findPlugin("RadeonProRender");

	if (handle != MObject::kNullObj)
	{
		MFnPlugin plugin;
		CHECK_MSTATUS(plugin.setObject(handle));
		MString translatorTitle = "RPR GLTF";

		if (enable)
		{
			CHECK_MSTATUS(plugin.registerFileTranslator(translatorTitle,
				nullptr, FireMaya::GLTFTranslator::creator, "rprGltfExport"));
		}
		else
		{
			CHECK_MSTATUS(plugin.deregisterFileTranslator(translatorTitle));
		}
	}
}

MStatus registerNodesInPathEditor(void)
{
	MStatus status;

	// add rpr texture node paths to path editor
	{
		std::stringstream reg_command;
		reg_command << "filePathEditor -registerType \"" << FIRE_RENDER_NODE_PREFIX << "Texture.filename\"" << " -typeLabel " << "\"RPRTextures\"";
		status = MGlobal::executeCommand(reg_command.str().c_str());
		CHECK_MSTATUS(status);
	}

	// add rpr IBL paths to path editor
	{
		std::stringstream reg_command;
		reg_command << "filePathEditor -registerType \"" << "RPRIBL.filePath\"" << " -typeLabel " << "\"RPR IBLs\"";
		status = MGlobal::executeCommand(reg_command.str().c_str());
		CHECK_MSTATUS(status);
	}

	// add rpr IES lights paths to path editor
	{
		std::stringstream reg_command;
		reg_command << "filePathEditor -registerType \"" << "RPRIES.iesFile\"" << " -typeLabel " << "\"RPR IES Lights\"";
		status = MGlobal::executeCommand(reg_command.str().c_str());
		CHECK_MSTATUS(status);
	}

	// add rpr volume paths to path editor
	{
		std::stringstream reg_command;
		reg_command << "filePathEditor -registerType \"" << FIRE_RENDER_NODE_PREFIX << "Volume.VdbFilePath\"" << " -typeLabel " << "\"RPR Volume\"";
		status = MGlobal::executeCommand(reg_command.str().c_str());
		CHECK_MSTATUS(status);
	}

	return status;
}

MStatus deRegisterNodesInPathEditor(void)
{
	MStatus status;

	// deregister rpr volume from path editor
	{
		std::stringstream reg_command;
		reg_command << "filePathEditor -deregisterType \"" << FIRE_RENDER_NODE_PREFIX << "Volume.VdbFilePath\"";
		status = MGlobal::executeCommand(reg_command.str().c_str());
		CHECK_MSTATUS(status);
	}

	// deregister IES from path editor
	{
		std::stringstream reg_command;
		reg_command << "filePathEditor -deregisterType \"" << "RPRIES.iesFile\"";
		status = MGlobal::executeCommand(reg_command.str().c_str());
		CHECK_MSTATUS(status);
	}

	// deregister IBL from path editor
	{
		std::stringstream reg_command;
		reg_command << "filePathEditor -deregisterType \"" << "RPRIBL.filePath\"";
		status = MGlobal::executeCommand(reg_command.str().c_str());
		CHECK_MSTATUS(status);
	}

	// deregister RPRTexture node from path editor
	{
		std::stringstream dereg_command;
		dereg_command << "filePathEditor -deregisterType \"" << FIRE_RENDER_NODE_PREFIX << "Texture.filename\"";
		status = MGlobal::executeCommand(dereg_command.str().c_str());
		CHECK_MSTATUS(status);
	}

	return status;	
}

void SetVersionsForMel()
{
	// Set RPR Engine version
	std::ostringstream ossRPRCore;
	ossRPRCore << RPR_VERSION_MAJOR << "." << RPR_VERSION_MINOR << RPR_VERSION_REVISION;

	std::string versionStr = ossRPRCore.str();
	std::string commandStr = "setRprCoreVersion(\"" + versionStr + "\")";

	MGlobal::executeCommand(commandStr.c_str());

	// set RIF version
	std::ostringstream ossRIF;
	ossRIF << RIF_VERSION_MAJOR << "." << RIF_VERSION_MINOR << RIF_VERSION_REVISION;

	versionStr = ossRIF.str();
	commandStr = "setRifVersion(\"" + versionStr + "\")";

	MGlobal::executeCommand(commandStr.c_str());

}

void AddExtensionAttributesCommon()
{
	// Add RPR UI to Maya native nodes
	MFnNumericAttribute nAttr;
	MStatus status;

	// nhair
	MNodeClass hairSystemClass("hairSystem");

	MObject hairMaterialAttr = nAttr.createColor("rprHairMaterial", "rhm");
	status = hairSystemClass.addExtensionAttribute(hairMaterialAttr);

	MObject hairCastShadows = nAttr.create("castsShadows", "csss", MFnNumericData::kBoolean, true, &status);
	nAttr.setNiceNameOverride("Hair Casts Shadows");
	status = hairSystemClass.addExtensionAttribute(hairCastShadows);

	MObject nhairVisible = nAttr.create("nhairIsVisible", "nhvi", MFnNumericData::kBoolean, true, &status);
	nAttr.setNiceNameOverride("Is Visible");
	status = hairSystemClass.addExtensionAttribute(nhairVisible);

	MObject nhairVisibleInReflections = nAttr.create("nhairVisibleInReflections", "nhrf", MFnNumericData::kBoolean, true, &status);
	nAttr.setNiceNameOverride("Visible In Reflections");
	status = hairSystemClass.addExtensionAttribute(nhairVisibleInReflections);

	MObject nhairVisibleInRefractions = nAttr.create("nhairVisibleInRefractions", "nhrr", MFnNumericData::kBoolean, true, &status);
	nAttr.setNiceNameOverride("Visible In Refractions");
	status = hairSystemClass.addExtensionAttribute(nhairVisibleInRefractions);

	// Adding RPRObjectId to all transforms
	MObject objectIdAttr = nAttr.create("RPRObjectId", "roi", MFnNumericData::kLong, 0);

	nAttr.setNiceNameOverride("RPR Object Id");
	nAttr.setMin(0);
	MNodeClass transformNodeClass("transform");
	transformNodeClass.addExtensionAttribute(objectIdAttr);

	// Material Idx

	// Adding visibility in contour render mode
	MObject contourVisibilityAttr = nAttr.create("RPRContourVisibility", "covs", MFnNumericData::kBoolean, true, &status);
	nAttr.setNiceNameOverride("RPR Visible in Contour mode");
	transformNodeClass.addExtensionAttribute(contourVisibilityAttr);

	////// light group attributes for Light Group AOVs

	MString lightClassNames[] = { "RPRPhysicalLight", "RPRIBL", "pointLight", "directionalLight", "spotLight", "areaLight" };

	for (MString className : lightClassNames)
	{
		MNodeClass lightClass(className);

		MObject lightGroupAttr = nAttr.create("RPRLightGroup", "lg", MFnNumericData::kLong, -1);

		nAttr.setNiceNameOverride("RPR Light Group");
		nAttr.setMin(-1);
		nAttr.setMax(3);

		lightClass.addExtensionAttribute(lightGroupAttr);
	}

	/// Add emitter attribute to locator node
	MNodeClass locatorClass("locator");

	MObject emitterAttr = nAttr.create("RPRIsEmitter", "iem", MFnNumericData::kBoolean, false);
	nAttr.setNiceNameOverride("RPR Is Emitter");
	locatorClass.addExtensionAttribute(emitterAttr);


	// Add shadow color attribute to geometry objects
	MNodeClass nurbsClass("nurbsSurface");
	MNodeClass meshClass("mesh");

	const MObject nurbsShadowColorAttr = nAttr.createColor("RPRShadowColor", "shc");
	const MObject meshShadowColorAttr = nAttr.createColor("RPRShadowColor", "shc");
	nurbsClass.addExtensionAttribute(nurbsShadowColorAttr);
	meshClass.addExtensionAttribute(meshShadowColorAttr);
}

void AddExtensionAttributesForMaterials()
{
	MFnNumericAttribute nAttr;

	MNodeClass materialClass("shadingEngine");

	MObject materialIdAttribute = nAttr.create("RPRMaterialId", "rmi", MFnNumericData::kLong, 0);

	nAttr.setNiceNameOverride("RPR Material Id");
	nAttr.setMin(0);

	MStatus status = materialClass.addExtensionAttribute(materialIdAttribute);
	assert(status == MStatus::kSuccess);
}

void AddPreferences()
{
	// Allow locking and unlocking edits on referenced attributes
	MString melCommand = "optionVar -iv \"refLockEditable\" 1; ";

	MStatus status = MGlobal::executeCommand(melCommand);
	assert(status == MStatus::kSuccess);
}

MStatus initializePlugin(MObject obj)
//
//	Description:
//		this method is called when the plug-in is loaded into Maya.  It
//		registers all of the services that this plug-in provides with
//		Maya.
//
//	Arguments:
//		obj - a handle to the plug-in object (use MFnPlugin to access it)
//
{
#ifdef _WIN32
	pTopLevelExceptionFilter = ::SetUnhandledExceptionFilter(FrUnhandledExceptionFilter);
#endif

	// We have legacy updater here which does not work. Comment this code for now becaue it breaks Maya 2022 startup.
	//PluginUpdater();

	// Added for Linux:
	Logger::AddCallback(InfoCallback, Logger::LevelInfo);

	FireMaya::gMainThreadId = std::this_thread::get_id();
	FireRenderThread::RunTheThread(true);

#ifdef OSMac_
	auto tracePath = std::getenv("FR_TRACE_OUTPUT");
	if (tracePath)
	{
		if (true)
		{
			frw::Context::TraceOutput(tracePath);
		}
	}

#elif _WIN32

	RPRInit();

#ifdef NT_PLUGIN

	Logger::AddCallback(DebugCallback, Logger::LevelDebug);

	if (auto path = std::getenv("FR_TRACE_OUTPUT"))
	{
		if (CreateDirectoryA(path, nullptr) || (ERROR_ALREADY_EXISTS == GetLastError()))
		{
			frw::Context::TraceOutput(path);
		}
	}
#endif

	Logger::AddCallback(InfoCallback, Logger::LevelInfo);

	LogPrint("RPR plugin initialization");

	// Check if OpenCL.dll is available on the system.
	HMODULE openCL = LoadLibrary("OpenCL.dll");
	if (openCL == NULL)
	{
		DebugPrint("initializePlugin - error: unable to load OpenCL.dll");
		MGlobal::displayError("Radeon ProRender plugin was unable to load OpenCL.dll. You may need to update your graphics drivers.");
		return MStatus::kFailure;
	}
	else
		FreeLibrary(openCL);

#endif

	MStatus   status;
	MString pluginVersion = PLUGIN_VERSION;
	MFnPlugin plugin(obj, PLUGIN_VENDOR, pluginVersion.asChar(), "Any");

	MString UserClassify("rendernode/firerender/shader/surface:shader/surface");
	MString UserVolumeClassify("rendernode/firerender/shader/volume:shader/volume");
	MString UserUtilityClassify("rendernode/firerender/utility:utility/general");
	MString UserTextureClassify("rendernode/firerender/texture/2d:texture/2d");

	MString UserDisplacementClassify("rendernode/firerender/shader/displacement:shader/disaplacement");

	glewInit();

	StartupContextChecker::CheckContexts();
	if (!StartupContextChecker::IsRprSupported())
	{
		return MStatus::kFailure;
	}

	if (!StartupContextChecker::IsMLDenoiserSupportedCPU())
	{
		MGlobal::displayWarning("Machine learning denoiser is not supported by current CPU");
	}

	CHECK_MSTATUS(plugin.registerNode("RadeonProRenderGlobals", FireRenderGlobals::FRTypeID(),
		FireRenderGlobals::creator,
		FireRenderGlobals::initialize,
		MPxNode::kDependNode));

	MString setCachePathString = "import fireRender.fireRenderUtils as fru\nfru.setShaderCachePathEnvironment(\"" + pluginVersion + "\")";
	MGlobal::executePythonCommand(setCachePathString);

	MString iblClassification = FireRenderIBL::drawDbClassification;
	MString skyClassification = FireRenderSkyLocator::drawDbClassification;
	MString iesClassification = FireRenderIESLightLocator::drawDbClassification;
	MString physicalClassification = FireRenderPhysicalLightLocator::drawDbClassification;
	MString envLightClassification = FireRenderEnvironmentLight::drawDbClassification;
	MString volumeClassification = FireRenderVolumeLocator::drawDbClassification;
	
	static const MString swatchName("swatchFireRenderMaterial");
	if (MGlobal::mayaState() != MGlobal::kBatch)
	{
		MSwatchRenderRegister::registerSwatchRender(swatchName, FireRenderMaterialSwatchRender::creator);
		UserClassify += ":swatch/"_ms + swatchName;
		UserUtilityClassify += ":swatch/"_ms + swatchName;
		UserTextureClassify += ":swatch/"_ms + swatchName;

		iblClassification += ":swatch/"_ms + swatchName;
		skyClassification += ":swatch/"_ms + swatchName;
		iesClassification += ":swatch/"_ms + swatchName;
		envLightClassification += ":swatch/"_ms + swatchName;

		CHECK_MSTATUS(plugin.registerRenderer(FIRE_RENDER_NAME, FireMaterialViewRenderer::creator));
	}

	CHECK_MSTATUS(plugin.registerCommand("fireRender", FireRenderCmd::creator, FireRenderCmd::newSyntax));
	CHECK_MSTATUS(plugin.registerCommand("fireRenderViewport", FireRenderViewportCmd::creator, FireRenderViewportCmd::newSyntax));
	CHECK_MSTATUS(plugin.registerCommand("fireRenderExport", FireRenderExportCmd::creator, FireRenderExportCmd::newSyntax));
	CHECK_MSTATUS(plugin.registerCommand("fireRenderImport", FireRenderImportCmd::creator, FireRenderImportCmd::newSyntax));
	CHECK_MSTATUS(plugin.registerCommand("fireRenderLocation", FireRenderLocationCmd::creator, FireRenderLocationCmd::newSyntax));
	CHECK_MSTATUS(plugin.registerCommand("fireRenderConvertVRay", FireRenderConvertVRayCmd::creator, FireRenderConvertVRayCmd::newSyntax));
	CHECK_MSTATUS(plugin.registerCommand("athenaEnable", AthenaEnableCmd::creator, AthenaEnableCmd::newSyntax));

	CHECK_MSTATUS(plugin.registerCommand("enableSaveIntermediate", EnableSaveIntermediateCmd::creator, EnableSaveIntermediateCmd::newSyntax));

	MString namePrefix(FIRE_RENDER_NODE_PREFIX);

	////
	CHECK_MSTATUS(plugin.registerCommand(namePrefix + "XMLExport", FireRenderXmlExportCmd::creator, FireRenderXmlExportCmd::newSyntax));

	CHECK_MSTATUS(plugin.registerCommand(namePrefix + "XMLImport", FireRenderXmlImportCmd::creator, FireRenderXmlImportCmd::newSyntax));

	CHECK_MSTATUS(plugin.registerCommand(namePrefix + "MaterialXImport", FireRenderMaterialXImportCmd::creator, FireRenderMaterialXImportCmd::newSyntax));
	////

	CHECK_MSTATUS(plugin.registerCommand(namePrefix + "ImageComparing", FireRenderImageComparing::creator, FireRenderImageComparing::newSyntax));

	CHECK_MSTATUS(plugin.registerNode(namePrefix + "IBL", FireRenderIBL::id,
		FireRenderIBL::creator, FireRenderIBL::initialize,
		MPxNode::kLocatorNode, &iblClassification));

	CHECK_MSTATUS(MHWRender::MDrawRegistry::registerGeometryOverrideCreator(
		FireRenderIBL::drawDbGeomClassification,
		FireRenderIBL::drawRegistrantId,
		FireRenderIBLOverride::Creator));

	CHECK_MSTATUS(plugin.registerNode(namePrefix + "Sky", FireRenderSkyLocator::id,
		FireRenderSkyLocator::creator, FireRenderSkyLocator::initialize,
		MPxNode::kLocatorNode, &skyClassification));

	CHECK_MSTATUS(MHWRender::MDrawRegistry::registerGeometryOverrideCreator(
		FireRenderSkyLocator::drawDbGeomClassification,
		FireRenderSkyLocator::drawRegistrantId,
		FireRenderSkyLocatorOverride::Creator));

	CHECK_MSTATUS(plugin.registerNode(namePrefix + "IES", FireRenderIESLightLocator::id,
		FireRenderIESLightLocator::creator, FireRenderIESLightLocator::initialize,
		MPxNode::kLocatorNode, &iesClassification));

	CHECK_MSTATUS(MHWRender::MDrawRegistry::registerGeometryOverrideCreator(
		FireRenderIESLightLocator::drawDbGeomClassification,
		FireRenderIESLightLocator::drawRegistrantId,
		FireRenderIESLightLocatorOverride::Creator));

	CHECK_MSTATUS(plugin.registerNode(namePrefix + "PhysicalLight", FireRenderPhysicalLightLocator::id,
		FireRenderPhysicalLightLocator::creator, FireRenderPhysicalLightLocator::initialize,
		MPxNode::kLocatorNode, &physicalClassification));

	CHECK_MSTATUS(MHWRender::MDrawRegistry::registerGeometryOverrideCreator(
		FireRenderPhysicalLightLocator::drawDbGeomClassification,
		FireRenderPhysicalLightLocator::drawRegistrantId,
		FireRenderPhysicalOverride::Creator));

	CHECK_MSTATUS(plugin.registerNode(namePrefix + "EnvLight", FireRenderEnvironmentLight::id,
		FireRenderEnvironmentLight::creator, FireRenderEnvironmentLight::initialize,
		MPxNode::kLocatorNode, &envLightClassification));

	CHECK_MSTATUS(plugin.registerNode(namePrefix + "Volume", FireRenderVolumeLocator::id,
		FireRenderVolumeLocator::creator, FireRenderVolumeLocator::initialize,
		MPxNode::kLocatorNode, &volumeClassification));

	CHECK_MSTATUS(MHWRender::MDrawRegistry::registerGeometryOverrideCreator(
		FireRenderVolumeLocator::drawDbClassification,
		FireRenderVolumeLocator::drawRegistrantId,
		FireRenderVolumeOverride::Creator));

	MString renderPassClassification("rendernode/firerender/renderpass");
	CHECK_MSTATUS(plugin.registerNode(namePrefix + "RenderPass", FireRenderRenderPass::id,
		FireRenderRenderPass::creator, FireRenderRenderPass::initialize,
		MPxNode::kDependNode, &renderPassClassification));

	NewSceneBasicSetup(NULL);

	beforeNewSceneCallback = MSceneMessage::addCallback(MSceneMessage::kBeforeNew, swapToDefaultRenderOverride, NULL, &status);
	CHECK_MSTATUS(status);
	beforeOpenSceneCallback = MSceneMessage::addCallback(MSceneMessage::kBeforeOpen, swapToDefaultRenderOverride, NULL, &status);
	CHECK_MSTATUS(status);

	mayaExitingCallback = MSceneMessage::addCallback(MSceneMessage::kMayaExiting, mayaExiting, NULL, &status);
	CHECK_MSTATUS(status);

	newSceneCallback = MSceneMessage::addCallback(MSceneMessage::kAfterNew, NewSceneBasicSetup, NULL, &status);
	CHECK_MSTATUS(status);
	openSceneCallback = MSceneMessage::addCallback(MSceneMessage::kAfterOpen, NewSceneBasicSetup, NULL, &status);
	CHECK_MSTATUS(status);

	auto mlDenoiserSupportedCPU = static_cast<int>(StartupContextChecker::IsMLDenoiserSupportedCPU());
	MString mlSupportCPU = MString(std::to_string(mlDenoiserSupportedCPU).c_str());

	MString registerCmd = MString("registerFireRender(" + mlSupportCPU + ")");
	MGlobal::executeCommand(registerCmd);

	MGlobal::executeCommand("setupFireRenderNodeClassification()");

	AddExtensionAttributesCommon();
	MGlobal::executeCommand("setupFireRenderExtraUI()");

	// GLTF
	MGlobal::executeCommand("rprExportsGLTF(1)");

	// register shaders

	// RPR Material
	static const MString FireRenderSurfacesDrawDBClassification("drawdb/shader/surface/" + namePrefix + "Material");
	static const MString FireRenderSurfacesRegistrantId(namePrefix + "MaterialRegistrantId");
	static const MString FireRenderSurfacesFullClassification = "rendernode/firerender/shader/surface:shader/surface:" + FireRenderSurfacesDrawDBClassification + ":swatch/" + swatchName;
	CHECK_MSTATUS(plugin.registerNode(namePrefix + "Material", FireMaya::Material::FRTypeID(),
		FireMaya::Material::creator,
		FireMaya::Material::initialize,
		MPxNode::kDependNode, &FireRenderSurfacesFullClassification));
	MHWRender::MDrawRegistry::registerSurfaceShadingNodeOverrideCreator(FireRenderSurfacesDrawDBClassification, FireRenderSurfacesRegistrantId, FireRenderMaterialNodeOverride::creator);

	// RPR Blend Material
	static const MString FireRenderBlendDrawDBClassification("drawdb/shader/surface/" + namePrefix + "BlendMaterial");
	static const MString FireRenderBlendRegistrantId(namePrefix + "BlendMaterialRegistrantId");
	static const MString FireRenderBlendFullClassification = "rendernode/firerender/shader/surface:shader/surface:" + FireRenderBlendDrawDBClassification + ":swatch/" + swatchName;
	CHECK_MSTATUS(plugin.registerNode(namePrefix + "BlendMaterial", FireMaya::BlendMaterial::FRTypeID(),
		FireMaya::BlendMaterial::creator,
		FireMaya::BlendMaterial::initialize,
		MPxNode::kDependNode, &FireRenderBlendFullClassification));
	CHECK_MSTATUS(MHWRender::MDrawRegistry::registerSurfaceShadingNodeOverrideCreator(FireRenderBlendDrawDBClassification, FireRenderBlendRegistrantId, FireMaya::BlendMaterial::Override::creator));

	CHECK_MSTATUS(plugin.registerNode(namePrefix + "VolumeMaterial", FireMaya::VolumeMaterial::FRTypeID(),
		FireMaya::VolumeMaterial::creator,
		FireMaya::VolumeMaterial::initialize,
		MPxNode::kDependNode, &UserVolumeClassify));

	CHECK_MSTATUS(plugin.registerNode(namePrefix + "SubsurfaceMaterial", FireMaya::SubsurfaceMaterial::FRTypeID(),
		FireMaya::SubsurfaceMaterial::creator,
		FireMaya::SubsurfaceMaterial::initialize,
		MPxNode::kDependNode, &UserClassify));

	CHECK_MSTATUS(plugin.registerNode(namePrefix + "PbrMaterial", FireMaya::FireRenderPBRMaterial::FRTypeID(),
		FireMaya::FireRenderPBRMaterial::creator,
		FireMaya::FireRenderPBRMaterial::initialize,
		MPxNode::kDependNode, &UserClassify));

	// Flat Color (pass through) shader
	CHECK_MSTATUS(plugin.registerNode(namePrefix + "FlatColorMaterial", FireMaya::Passthrough::FRTypeID(),
		FireMaya::Passthrough::creator,
		FireMaya::Passthrough::initialize,
		MPxNode::kDependNode, &UserClassify));

	static const MString FireRenderUberDrawDBClassification("drawdb/shader/surface/" + namePrefix + "UberMaterial");
	static const MString FireRenderUberRegistrantId(namePrefix + "UberMaterialRegistrantId");
	static const MString FireRenderUberFullClassification = "rendernode/firerender/shader/surface:shader/surface:" + FireRenderUberDrawDBClassification + ":swatch/" + swatchName;
	CHECK_MSTATUS(plugin.registerNode(namePrefix + "UberMaterial", FireMaya::StandardMaterial::FRTypeID(),
		FireMaya::StandardMaterial::creator,
		FireMaya::StandardMaterial::initialize,
		MPxNode::kDependNode, &FireRenderUberFullClassification));
	MHWRender::MDrawRegistry::registerSurfaceShadingNodeOverrideCreator(FireRenderUberDrawDBClassification, FireRenderUberRegistrantId, FireRenderStandardMaterialNodeOverride::creator);

	CHECK_MSTATUS(plugin.registerNode(namePrefix + "TransparentMaterial", FireMaya::TransparentMaterial::FRTypeID(),
		FireMaya::TransparentMaterial::creator,
		FireMaya::TransparentMaterial::initialize,
		MPxNode::kDependNode, &UserClassify));

	CHECK_MSTATUS(plugin.registerNode(namePrefix + "MatteMaterial", FireMaya::ShadowCatcherMaterial::FRTypeID(),
		FireMaya::ShadowCatcherMaterial::creator,
		FireMaya::ShadowCatcherMaterial::initialize,
		MPxNode::kDependNode, &UserClassify));

	CHECK_MSTATUS(plugin.registerNode(namePrefix + "DoubleSided", FireMaya::RPRDoubleSided::FRTypeID(),
		FireMaya::RPRDoubleSided::creator,
		FireMaya::RPRDoubleSided::initialize,
		MPxNode::kDependNode, &UserClassify));

	CHECK_MSTATUS(plugin.registerNode(namePrefix + "ToonMaterial", FireMaya::ToonMaterial::FRTypeID(),
		FireMaya::ToonMaterial::creator,
		FireMaya::ToonMaterial::initialize,
		MPxNode::kDependNode, &UserClassify));
	// force load shader UI template to use callback functions
	MGlobal::executeCommand("source AERPRToonMaterialTemplate");

	CHECK_MSTATUS(plugin.registerNode(namePrefix + "MaterialXMaterial", FireMaya::MaterialXMaterial::FRTypeID(),
		FireMaya::MaterialXMaterial::creator,
		FireMaya::MaterialXMaterial::initialize,
		MPxNode::kDependNode, &UserClassify));
	
	CHECK_MSTATUS(status);

	CHECK_MSTATUS(plugin.registerNode(namePrefix + "Displacement", FireMaya::Displacement::FRTypeID(),
		FireMaya::Displacement::creator,
		FireMaya::Displacement::initialize,
		MPxNode::kDependNode, &UserDisplacementClassify));

	// register maps

	static const MString FireRenderFresnelDrawDBClassification("drawdb/shader/surface/" + namePrefix + "Fresnel");
	static const MString FireRenderFresnelRegistrantId(namePrefix + "FresnelMaterialRegistrantId");
	static const MString FireRenderFresnelFullClassification = "rendernode/firerender/utility:utility/general:" + FireRenderFresnelDrawDBClassification + ":swatch/" + swatchName;
	CHECK_MSTATUS(plugin.registerNode(namePrefix + "Fresnel", FireMaya::Fresnel::FRTypeID(),
		FireMaya::Fresnel::creator,
		FireMaya::Fresnel::initialize,
		MPxNode::kDependNode, &FireRenderFresnelFullClassification));
	CHECK_MSTATUS(MHWRender::MDrawRegistry::registerShadingNodeOverrideCreator(FireRenderFresnelDrawDBClassification, FireRenderFresnelRegistrantId, FireMaya::Fresnel::Override::creator));

	CHECK_MSTATUS(plugin.registerNode(namePrefix + "Checker", FireMaya::Checker::FRTypeID(),
		FireMaya::Checker::creator,
		FireMaya::Checker::initialize,
		MPxNode::kDependNode, &UserUtilityClassify));

	CHECK_MSTATUS(plugin.registerNode(namePrefix + "Arithmetic", FireMaya::Arithmetic::FRTypeID(),
		FireMaya::Arithmetic::creator,
		FireMaya::Arithmetic::initialize,
		MPxNode::kDependNode, &UserUtilityClassify));

	CHECK_MSTATUS(plugin.registerNode(namePrefix + "Dot", FireMaya::Dot::FRTypeID(),
		FireMaya::Dot::creator,
		FireMaya::Dot::initialize,
		MPxNode::kDependNode, &UserUtilityClassify));

	CHECK_MSTATUS(plugin.registerNode(namePrefix + "Voronoi", FireMaya::Voronoi::FRTypeID(),
		FireMaya::Voronoi::creator,
		FireMaya::Voronoi::initialize,
		MPxNode::kDependNode, &UserTextureClassify));

	CHECK_MSTATUS(plugin.registerNode(namePrefix + "Bevel", FireMaya::Bevel::FRTypeID(),
		FireMaya::Bevel::creator,
		FireMaya::Bevel::initialize,
		MPxNode::kDependNode, &UserUtilityClassify));

	CHECK_MSTATUS(plugin.registerNode(namePrefix + "BlendValue", FireMaya::BlendValue::FRTypeID(),
		FireMaya::BlendValue::creator,
		FireMaya::BlendValue::initialize,
		MPxNode::kDependNode, &UserUtilityClassify));

	CHECK_MSTATUS(plugin.registerNode(namePrefix + "Gradient", FireMaya::Gradient::FRTypeID(),
		FireMaya::Gradient::creator,
		FireMaya::Gradient::initialize,
		MPxNode::kDependNode, &UserUtilityClassify));

	CHECK_MSTATUS(plugin.registerNode(namePrefix + "Lookup", FireMaya::Lookup::FRTypeID(),
		FireMaya::Lookup::creator,
		FireMaya::Lookup::initialize,
		MPxNode::kDependNode, &UserUtilityClassify));

	CHECK_MSTATUS(plugin.registerNode(namePrefix + "Texture", FireMaya::Texture::FRTypeID(),
		FireMaya::Texture::creator,
		FireMaya::Texture::initialize,
		MPxNode::kDependNode, &UserTextureClassify));

	CHECK_MSTATUS(plugin.registerNode(namePrefix + "FresnelSchlick", FireMaya::FresnelSchlick::FRTypeID(),
		FireMaya::FresnelSchlick::creator,
		FireMaya::FresnelSchlick::initialize,
		MPxNode::kDependNode, &UserUtilityClassify));

	CHECK_MSTATUS(plugin.registerNode(namePrefix + "Noise", FireMaya::Noise::FRTypeID(),
		FireMaya::Noise::creator,
		FireMaya::Noise::initialize,
		MPxNode::kDependNode, &UserUtilityClassify));

	CHECK_MSTATUS(plugin.registerNode(namePrefix + "Bump", FireMaya::Bump::FRTypeID(),
		FireMaya::Bump::creator,
		FireMaya::Bump::initialize,
		MPxNode::kDependNode, &UserUtilityClassify));

	CHECK_MSTATUS(plugin.registerNode(namePrefix + "Normal", FireMaya::Normal::FRTypeID(),
		FireMaya::Normal::creator,
		FireMaya::Normal::initialize,
		MPxNode::kDependNode, &UserUtilityClassify));

    CHECK_MSTATUS(plugin.registerNode(namePrefix + "AmbientOcclusion", FireMaya::FireRenderAO::FRTypeID(),
        FireMaya::FireRenderAO::creator,
        FireMaya::FireRenderAO::initialize,
        MPxNode::kDependNode, &UserUtilityClassify));

	CHECK_MSTATUS(plugin.registerNode(namePrefix + "Ramp", FireMaya::RPRRamp::FRTypeID(),
		FireMaya::RPRRamp::creator,
		FireMaya::RPRRamp::initialize,
		MPxNode::kDependNode, &UserTextureClassify));

	// Initialize the viewport render override.
	FireRenderOverride::instance()->initialize();

	//load main menu
	if (MGlobal::mayaState() != MGlobal::kBatch)
	{
		MGlobal::executePythonCommand("import fireRender.fireRenderMenu\nfireRender.fireRenderMenu.createFireRenderMenu()");
	}

	CHECK_MSTATUS(registerNodesInPathEditor());

	SetVersionsForMel();

	// If we in file open process we need to remove default RadeonProRenderGlobals node
	if (MFileIO::isOpeningFile())
	{
		MGlobal::executeCommand("delete RadeonProRenderGlobals");
	}

	// Reload Hypershade window
	// - After plugin is loaded if Hypershade window was open, it becomes bugged,
	// - so we close and then open Hypershade (if Hypershade window was opened)
	{
		MString command =
			"string $panels[] = `getPanel - vis`;\n"
			"if (stringArrayContains(\"hyperShadePanel1\", $panels)) {\n"
			"deleteUI -panel \"hyperShadePanel1\";\n"
			"HypershadeWindow;\n"
			"} \n";
		status = MGlobal::executeCommandOnIdle(command);
		CHECK_MSTATUS(status);
	}

	// Set values to special Maya environmental variables
	// - path to RPR Templates folder
#if not defined(__APPLE__) // setenv command doesn't work on MAC; we set enviromental variable value via .mod file instead
	{
		MString command =
			"string $pathToPlugin = `pluginInfo - query - path RadeonProRender`; \n"
			"string $tail = \"/plug-ins/20XX/RadeonProRender.mll\"; \n"
			"int $len = size($pathToPlugin); \n"
			"int $newLen = $len - size($tail); \n"
			"string $pathToTemplates = `substring $pathToPlugin 1 $newLen`; \n"
			"$pathToTemplates = $pathToTemplates + \"/scripts/Templates\"; \n"
			"string $currPathValue =  `getenv MAYA_CUSTOM_TEMPLATE_PATH`; \n"
			"string $newPathValue = \";\" + $pathToTemplates; \n"
			"$newPathValue = $currPathValue + $newPathValue; \n"
			"putenv \"MAYA_CUSTOM_TEMPLATE_PATH\" $newPathValue; \n";
		status = MGlobal::executeCommandOnIdle(command);
		CHECK_MSTATUS(status);
	}
#endif

	AddExtensionAttributesForMaterials();

	AddPreferences();

	return status;
}

MStatus uninitializePlugin(MObject obj)
//
//	Description:
//		this method is called when the plug-in is unloaded from Maya. It
//		unregisters all of the services that it was providing.
//
//	Arguments:
//		obj - a handle to the plug-in object (use MFnPlugin to access it)
//
{
	MStatus   status;
	MFnPlugin plugin(obj);

	FireRenderViewportManager::instance().clear();
	FireRenderThread::RunTheThread(false);
	std::this_thread::yield();

	CHECK_MSTATUS(plugin.deregisterCommand("fireRender"));
	CHECK_MSTATUS(plugin.deregisterCommand("fireRenderViewport"));
	CHECK_MSTATUS(plugin.deregisterCommand("fireRenderExport"));
	CHECK_MSTATUS(plugin.deregisterCommand("fireRenderImport"));
	CHECK_MSTATUS(plugin.deregisterCommand("fireRenderConvertVRay"));

	CHECK_MSTATUS(plugin.deregisterCommand("enableSaveIntermediate"));

	CHECK_MSTATUS(deRegisterNodesInPathEditor());

	CHECK_MSTATUS(plugin.deregisterNode(FireMaya::Material::FRTypeID()));
	CHECK_MSTATUS(plugin.deregisterNode(FireMaya::BlendMaterial::FRTypeID()));

	CHECK_MSTATUS(plugin.deregisterNode(FireMaya::StandardMaterial::FRTypeID()));
	CHECK_MSTATUS(plugin.deregisterNode(FireMaya::Displacement::FRTypeID()));


	CHECK_MSTATUS(plugin.deregisterNode(FireMaya::Fresnel::FRTypeID()));
	CHECK_MSTATUS(plugin.deregisterNode(FireMaya::Arithmetic::FRTypeID()));
	CHECK_MSTATUS(plugin.deregisterNode(FireMaya::Checker::FRTypeID()));
	CHECK_MSTATUS(plugin.deregisterNode(FireMaya::Dot::FRTypeID()));
	CHECK_MSTATUS(plugin.deregisterNode(FireMaya::Voronoi::FRTypeID()));
	CHECK_MSTATUS(plugin.deregisterNode(FireMaya::BlendValue::FRTypeID()));
	CHECK_MSTATUS(plugin.deregisterNode(FireMaya::Gradient::FRTypeID()));
	CHECK_MSTATUS(plugin.deregisterNode(FireMaya::Lookup::FRTypeID()));
	CHECK_MSTATUS(plugin.deregisterNode(FireMaya::Texture::FRTypeID()));
	CHECK_MSTATUS(plugin.deregisterNode(FireMaya::FresnelSchlick::FRTypeID()));
	CHECK_MSTATUS(plugin.deregisterNode(FireMaya::Noise::FRTypeID()));
	CHECK_MSTATUS(plugin.deregisterNode(FireMaya::Passthrough::FRTypeID()));
	CHECK_MSTATUS(plugin.deregisterNode(FireMaya::Bump::FRTypeID()));
	CHECK_MSTATUS(plugin.deregisterNode(FireMaya::Normal::FRTypeID()));
    CHECK_MSTATUS(plugin.deregisterNode(FireMaya::FireRenderAO::FRTypeID()));
    CHECK_MSTATUS(plugin.deregisterNode(FireMaya::RPRRamp::FRTypeID()));

	CHECK_MSTATUS(plugin.deregisterNode(FireMaya::FireRenderPBRMaterial::FRTypeID()));
	CHECK_MSTATUS(plugin.deregisterNode(FireMaya::RPRDoubleSided::FRTypeID()));
	CHECK_MSTATUS(plugin.deregisterNode(FireMaya::ShadowCatcherMaterial::FRTypeID()));
	CHECK_MSTATUS(plugin.deregisterNode(FireMaya::SubsurfaceMaterial::FRTypeID()));
	CHECK_MSTATUS(plugin.deregisterNode(FireMaya::VolumeMaterial::FRTypeID()));
	CHECK_MSTATUS(plugin.deregisterNode(FireMaya::TransparentMaterial::FRTypeID()));

	CHECK_MSTATUS(plugin.deregisterNode(FireRenderGlobals::FRTypeID()));

	CHECK_MSTATUS(plugin.deregisterNode(FireRenderRenderPass::id));
	CHECK_MSTATUS(plugin.deregisterNode(FireRenderIBL::id));
	CHECK_MSTATUS(plugin.deregisterNode(FireRenderSkyLocator::id));
	CHECK_MSTATUS(plugin.deregisterNode(FireRenderEnvironmentLight::id));

	CHECK_MSTATUS(MHWRender::MDrawRegistry::deregisterGeometryOverrideCreator(FireRenderIBL::drawDbClassification, FireRenderIBL::drawRegistrantId));
	CHECK_MSTATUS(MHWRender::MDrawRegistry::deregisterGeometryOverrideCreator(FireRenderSkyLocator::drawDbClassification, FireRenderSkyLocator::drawRegistrantId));

	//
	MString namePrefix(FIRE_RENDER_NODE_PREFIX);
	CHECK_MSTATUS(plugin.deregisterCommand(namePrefix + "ImageComparing"));
	//

	CHECK_MSTATUS(plugin.deregisterCommand(namePrefix + "MaterialXImport"));

	if (MGlobal::mayaState() != MGlobal::kBatch)
	{
		CHECK_MSTATUS(MSwatchRenderRegister::unregisterSwatchRender("swatchFireRenderMaterial"));
		CHECK_MSTATUS(plugin.deregisterRenderer(FIRE_RENDER_NAME));
	}

	MMessage::removeCallback(newSceneCallback);
	MMessage::removeCallback(openSceneCallback);

	MMessage::removeCallback(beforeNewSceneCallback);
	MMessage::removeCallback(beforeOpenSceneCallback);

	// Delete the viewport render override.
	FireRenderOverride::deleteInstance();

	// Clean up the FireRender command.
	FireRenderCmd::cleanUp();

	//remove main menu
	if (MGlobal::mayaState() != MGlobal::kBatch)
	{
		MGlobal::executePythonCommand("import fireRender.fireRenderMenu\nfireRender.fireRenderMenu.removeFireRenderMenu()");
	}

#ifdef OSMac_
	//
#elif __linux__
	//
#else
	RPRRelease();
#endif

	return status;
}
