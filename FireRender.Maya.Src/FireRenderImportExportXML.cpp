#include "FireRenderImportExportXML.h"

#include <maya/MArgDatabase.h>
#include <maya/MItDag.h>
#include <maya/MDagPath.h>
#include <maya/MGlobal.h>
#include <maya/MSelectionList.h>
#include <maya/MObjectArray.h>
#include <maya/MArgList.h>
#include <maya/MStatus.h>
#include <maya/MDagPathArray.h>
#include <maya/MFileObject.h>
#include <maya/MCommonSystemUtils.h>

#include "FireRenderContext.h"
#include "FireRenderUtils.h"

#include <vector>
#include <set>
#include <string>
#ifdef _WIN32
#include <tchar.h>
#else
#include <wchar.h>
#endif

#include <iostream>
#include <fstream>
#include <sstream>

#include "common.h"
#include "FireRenderMaterial.h"
#include <iterator>
#include <maya/MItDependencyNodes.h>

#include "FileSystemUtils.h"
#include "FireRenderError.h"

#ifdef _WIN32
#define PATH_SEPARATOR "\\"
#else
#define PATH_SEPARATOR "/"
#endif

namespace
{
	std::string SanitizeNameForFile(const char* sz)
	{
		std::string ret(sz);
		for (int i = 0; i < ret.size(); i++)
		{
			if (!strchr("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-", ret[i]))
				ret[i] = '_';
		}
		return ret;
	}

	std::string ParentFolder(const char* sz)
	{
		auto a = strrchr(sz, '\\');
		auto b = strrchr(sz, '/');
		if (a < b)
			a = b;
		if (a)
			return std::string(sz, a - sz);
		return ".";
	}
}

FireRenderXmlExportCmd::FireRenderXmlExportCmd()
{
}

FireRenderXmlExportCmd::~FireRenderXmlExportCmd()
{
}

void * FireRenderXmlExportCmd::creator()
{
	return new FireRenderXmlExportCmd;
}

MSyntax FireRenderXmlExportCmd::newSyntax()
{
	MStatus status;
	MSyntax syntax;

	CHECK_MSTATUS(syntax.addFlag(kMaterialFlag, kMaterialFlagLong, MSyntax::kString));
	CHECK_MSTATUS(syntax.addFlag(kFilePathFlag, kFilePathFlagLong, MSyntax::kString));
	CHECK_MSTATUS(syntax.addFlag(kSelectionFlag, kSelectionFlagLong, MSyntax::kNoArg));
	CHECK_MSTATUS(syntax.addFlag(kAllFlag, kAllFlagLong, MSyntax::kNoArg));

	return syntax;
}

void AddMaterialsToList(MSelectionList& list, MObject node)
{
	MFnDependencyNode depNode(node);
	if (depNode.name() == "initialShadingGroup" || depNode.name() == "initialParticleSE")
		return;

	DebugPrint("Adding %s for export (%s)", node.apiTypeStr(), depNode.name().asUTF8());
	if (node.hasFn(MFn::kDagNode))
	{
		MFnDagNode dagNode(node);
		if (node.hasFn(MFn::kTransform))
		{
			for (uint i = 0; i < dagNode.childCount(); i++)
				AddMaterialsToList(list, dagNode.child(i));
		}
		else if (isGeometry(node))
		{
			for (auto shadingEngine : GetShadingEngines(dagNode, 0))
				list.add(shadingEngine);
		}
	}
	else // try adding it anyway, whatever can resolve to a shader is exports
	{
		list.add(node);
	}
}

MStatus FireRenderXmlExportCmd::doIt(const MArgList & args)
{
	MStatus status;

	MArgDatabase argData(syntax(), args);

	MString filePath;
	if (argData.isFlagSet(kFilePathFlag))
	{
		argData.getFlagArgument(kFilePathFlag, 0, filePath);
	}
	else
	{
		MGlobal::displayError("File path is missing, use -file flag");
		return MS::kFailure;
	}

	MSelectionList sList;

	if (argData.isFlagSet(kMaterialFlag))
	{
		MString materialName;
		argData.getFlagArgument(kMaterialFlag, 0, materialName);
		sList.add(materialName);
	}
	else if (argData.isFlagSet(kSelectionFlag))
	{
		MSelectionList list;

		status = MGlobal::getActiveSelectionList(list);
		if (status != MStatus::kSuccess)
			return status;

		for (uint i = 0; i < list.length(); i++)
		{
			MObject node;
			if (MStatus::kSuccess == list.getDependNode(i, node))
				AddMaterialsToList(sList, node);
		}
	}
	else // export ALL materials
	{
		MItDependencyNodes itDep(MFn::kShadingEngine);
		while (!itDep.isDone())
		{
			AddMaterialsToList(sList, itDep.item());
			itDep.next();
		}
	}

	FireRenderContext context;
	context.setCallbackCreationDisabled(true);
	context.initializeContext();
	context.setCallbackCreationDisabled(true);

	std::map<std::string, int> namesUsed;
	std::set<frw::Shader> objectsProcessed;

	for (uint i = 0; i < sList.length(); i++)
	{
		MObject node;
		sList.getDependNode(i, node);
		if (node.isNull())
			continue;

		if (node.hasFn(MFn::kShadingEngine))
			node = getSurfaceShader(node);

		MFnDependencyNode depNode(node);

		auto shader = context.GetShader(node);

		if (!shader)
		{
			ErrorPrint("Invalid shader");
			continue;
		}

		if (objectsProcessed.count(shader))
			continue;	// already handled

		objectsProcessed.insert(shader);


		///
		std::set<rpr_material_node> nodes_to_check;
		std::vector<rpr_material_node> nodes_to_save;
		nodes_to_check.insert(shader.Handle());
		//look for all required by sh material nodes
		while (!nodes_to_check.empty())
		{
			rpr_material_node mat = *nodes_to_check.begin(); nodes_to_check.erase(mat);
			nodes_to_save.push_back(mat);

			size_t input_count = 0;
			rprMaterialNodeGetInfo(mat, RPR_MATERIAL_NODE_INPUT_COUNT, sizeof(size_t), &input_count, nullptr);
			for (uint j = 0; j < input_count; ++j)
			{
				rpr_int in_type;
				rprMaterialNodeGetInputInfo(mat, j, RPR_MATERIAL_NODE_INPUT_TYPE, sizeof(in_type), &in_type, nullptr);
				if (in_type == RPR_MATERIAL_NODE_INPUT_TYPE_NODE)
				{
					rpr_material_node ref_node = nullptr;
					rprMaterialNodeGetInputInfo(mat, j, RPR_MATERIAL_NODE_INPUT_VALUE, sizeof(ref_node), &ref_node, nullptr);
					//if node is not null and it's not duplicated
					if (ref_node && std::find(nodes_to_save.begin(), nodes_to_save.end(), ref_node) == nodes_to_save.end())
					{
						nodes_to_check.insert(ref_node);
					}
				}
			}
		}

		auto niceName = SanitizeNameForFile(depNode.name().asUTF8());
		if (namesUsed.find(niceName) != namesUsed.end())
		{
			niceName += "(";
			niceName += ++namesUsed[niceName];
			niceName += ")";
		}
		else
			namesUsed[niceName] = 0;

		std::string pathName = ParentFolder(filePath.asUTF8())
			+ PATH_SEPARATOR
			+ niceName
			+ ".xml"
			;

		ExportMaterials(pathName, nodes_to_save.data(), (int)nodes_to_save.size());
		///
	}

	return MS::kSuccess;
}

////////////////////////////////////////////////////////
////////////////////////////////////////////////////////

#if defined(LINUX) || defined(OSMac_)
#else
extern "C" {
#include "AxfConverterDll.h"
}

#pragma comment (lib, "../ThirdParty/AxfPackage/ReleaseDll/AxfLib/AxFDecoding_r.lib")
#pragma comment (lib, "../ThirdParty/AxfPackage/ReleaseDll/AxfLib/FreeImage.lib")
#pragma comment (lib, "../ThirdParty/AxfPackage/ReleaseDll/AxfLib/libboost_filesystem-vc140-mt-1_62.lib")
#pragma comment (lib, "../ThirdParty/AxfPackage/ReleaseDll/AxfLib/libboost_program_options-vc140-mt-1_62.lib")
#pragma comment (lib, "../ThirdParty/AxfPackage/ReleaseDll/AxfLib/libboost_regex-vc140-mt-1_62.lib")
#pragma comment (lib, "../ThirdParty/AxfPackage/ReleaseDll/AxfLib/libboost_system-vc140-mt-1_62.lib")

#endif

const bool doesAxfConverterDllExists() {
#if defined(LINUX) || defined(OSMac_)
	return false;
#else

	HINSTANCE hGetProcIDDLL = LoadLibrary(_T("AxfConverter.dll"));

	if (hGetProcIDDLL)
	{
		FreeLibrary(hGetProcIDDLL);
		return true;
	}
	else
	{
		return false;
	}
#endif
}

FireRenderAxfDLLExists::FireRenderAxfDLLExists()
{
}

FireRenderAxfDLLExists::~FireRenderAxfDLLExists()
{
}

void * FireRenderAxfDLLExists::creator()
{
	return new FireRenderAxfDLLExists;
}

MSyntax FireRenderAxfDLLExists::newSyntax()
{
	MStatus status;
	MSyntax syntax;

	CHECK_MSTATUS(syntax.addFlag(kAllFlag, kAllFlagLong, MSyntax::kNoArg));

	return syntax;
}

MStatus FireRenderAxfDLLExists::doIt(const MArgList& args) {
	return MS::kSuccess;
}

FireRenderXmlImportCmd::FireRenderXmlImportCmd() :
	m_importImages(false)
{
}

FireRenderXmlImportCmd::~FireRenderXmlImportCmd()
{
}

void * FireRenderXmlImportCmd::creator()
{
	return new FireRenderXmlImportCmd;
}

MSyntax FireRenderXmlImportCmd::newSyntax()
{
	MStatus status;
	MSyntax syntax;

	CHECK_MSTATUS(syntax.addFlag(kFilePathFlag, kFilePathFlagLong, MSyntax::kString));
	CHECK_MSTATUS(syntax.addFlag(kMaterialNameFlag, kMaterialNameFlagLong, MSyntax::kString));
	CHECK_MSTATUS(syntax.addFlag(kAllFlag, kAllFlagLong, MSyntax::kNoArg));
	CHECK_MSTATUS(syntax.addFlag(kImportImages, kImportImagesLong, MSyntax::kBoolean));

	return syntax;
}

MStatus FireRenderXmlImportCmd::doIt(const MArgList & args)
{
	nodeGroup.clear();
	directoryPath = "";

	MStatus status;

	MArgDatabase argData(syntax(), args);

	MString filePath;
	if (argData.isFlagSet(kFilePathFlag))
	{
		argData.getFlagArgument(kFilePathFlag, 0, filePath);
	}
	else
	{
		MGlobal::displayError("File path is missing, use -file flag");
		return MS::kFailure;
	}

	std::string userDefinedMaterialName = "";
	if (argData.isFlagSet(kMaterialNameFlag))
	{
		MString temp;
		argData.getFlagArgument(kMaterialNameFlag, 0, temp);
		userDefinedMaterialName = temp.asChar();
	}

#if defined(LINUX) || defined(OSMac_)
#else
	std::string filePathStr(filePath.asChar());
	std::transform(filePathStr.begin(), filePathStr.end(), filePathStr.begin(), ::tolower);

	bool isAxf = filePathStr.find(".axf") != -1;

	if (isAxf) {
		//convert axf to rpr xml using conversion dll:
		HINSTANCE hGetProcIDDLL = LoadLibrary(_T("AxfConverter.dll"));

		if (hGetProcIDDLL) {

			AxfConvertFile convertFile = (AxfConvertFile)GetProcAddress(hGetProcIDDLL, "convertAxFFile");

			char xmlFilePath[255];

			convertFile(filePathStr.c_str(), xmlFilePath);

			FreeLibrary(hGetProcIDDLL);

			bool xmlCreated = strlen(xmlFilePath) != 0;

			if (xmlCreated) {
				filePath = xmlFilePath;
			}
			else {
				return MS::kFailure;
			}
		}
		else {
			return MS::kFailure;
		}
	}
#endif

	// Import textures if required.
	if (argData.isFlagSet(kImportImages))
		argData.getFlagArgument(kImportImages, 0, m_importImages);

	std::string directory = getDirectory(filePath.asChar());

	directoryPath = MString(directory.c_str());

	ImportMaterials(filePath.asChar(), nodeGroup);

	for (auto node : nodeGroup)
	{
		std::string name = node.first;
		if (userDefinedMaterialName.length() != 0) {
			name = userDefinedMaterialName;
		}

		if (node.second.root) {
			//root of the ShadingEngine/ShadingGroup:
			MaterialNode matNode = node.second;
			parseMaterialNode(matNode);

			if (!matNode.parsedObject.isNull())
			{
				MFnDependencyNode nodeFn(matNode.parsedObject);
				nodeFn.setName(name.c_str());
				// TODO:

				MGlobal::executeCommand("$LastExportedRPRMaterialName = \"" + nodeFn.name() + "\"");

				MString sgNode = MGlobal::executeCommandStringResult("sets -renderable true -noSurfaceShader true -empty -name " + nodeFn.name() + "SG");
				if (sgNode != "")
				{
					MGlobal::executeCommand("connectAttr -f " + nodeFn.name() + ".outColor " + sgNode + ".surfaceShader");
				}
			}
			break;
		}
	}

	nodeGroup.clear();
	directoryPath = "";

	return MS::kSuccess;
}

int FireRenderXmlImportCmd::getAttrType(std::string attrTypeStr) {
	//FR_MATERIAL_NODE_INPUT_TYPE_FLOAT4 0x1
	//FR_MATERIAL_NODE_INPUT_TYPE_UINT 0x2
	//FR_MATERIAL_NODE_INPUT_TYPE_NODE 0x3
	//FR_MATERIAL_NODE_INPUT_TYPE_IMAGE 0x4
	//
	int attrType = -1;
	if (attrTypeStr == "float4") { attrType = RPR_MATERIAL_NODE_INPUT_TYPE_FLOAT4; }
	else if (attrTypeStr == "uint") { attrType = RPR_MATERIAL_NODE_INPUT_TYPE_UINT; }
	else if (attrTypeStr == "connection") { attrType = RPR_MATERIAL_NODE_INPUT_TYPE_NODE; }
	else if (attrTypeStr == "file_path") { attrType = RPR_MATERIAL_NODE_INPUT_TYPE_IMAGE; }

	return attrType;
}

void parseFloat4(std::string data, float fvalue[4]) {
	std::replace(data.begin(), data.end(), ',', ' '); //replace comas by spaces to simplify string splitting

	std::istringstream iss(data);
	std::vector<std::string> tokens{ std::istream_iterator<std::string>{iss}, std::istream_iterator<std::string>{} };
	int i = 0;
	for (const auto& val : tokens)
	{
		fvalue[i] = std::stof(val);
		++i;
	}
}

void FireRenderXmlImportCmd::parseAttributeParam(MObject shaderNode, std::map<std::string, std::string> &attributeMapper, std::string attrName, Param &attrParam) {
	MFnDependencyNode nodeFn(shaderNode);

	//
	std::string		attrT = attrParam.type;
	int				attrType = getAttrType(attrT);
	std::string		attrValue = attrParam.value;
	//

	MPlug attributePlug = nodeFn.findPlug(attributeMapper[attrName].c_str());


	switch (attrType)
	{
	case RPR_MATERIAL_NODE_INPUT_TYPE_FLOAT4:
	{
		float fvalue[4] = { 0.f, 0.f, 0.f, 0.f };
		parseFloat4(attrValue, fvalue);

		if (attrName.find("color") != std::string::npos)
		{
			float multiplier = 1.0;
			MPlug redPlug = attributePlug.child(0);
			MPlug bluePlug = attributePlug.child(1);
			MPlug greenPlug = attributePlug.child(2);

			redPlug.setValue(fvalue[0] * multiplier);
			bluePlug.setValue(fvalue[1] * multiplier);
			greenPlug.setValue(fvalue[2] * multiplier);
		}
		else
		{
			attributePlug.setValue(fvalue[0]);
		}
		break;
	}
	case RPR_MATERIAL_NODE_INPUT_TYPE_UINT:
	{
		int value = std::stoi(attrValue);
		attributePlug.setValue(value);
		break;
	}
	case RPR_MATERIAL_NODE_INPUT_TYPE_NODE:
	{
		MaterialNode node = nodeGroup[attrValue];
		MObject inputShader = parseMaterialNode(node);

		MFnDependencyNode inputNodeFn(inputShader);
		MPlug outPlug = inputNodeFn.findPlug("out");
		if (outPlug.isNull())
		{
			outPlug = inputNodeFn.findPlug("outColor");
		}
		if (outPlug.isNull())
		{
			outPlug = inputNodeFn.findPlug("output");
		}


		if (outPlug.isNull())
		{
			MGlobal::displayError("Error while reading " + MString(attrValue.c_str()) + " value");
			return;
		}

		int outChildNumber = outPlug.numChildren();
		int attributeChildNumber = attributePlug.numChildren();

		if (outChildNumber == attributeChildNumber) {
			MDGModifier modifier;
			modifier.connect(outPlug, attributePlug);
			modifier.doIt();
		}
		else {
			int connectionNumber = (outChildNumber > attributeChildNumber) ? attributeChildNumber : outChildNumber;

			if (connectionNumber == 0)
			{
				MPlug out_valueChildPlug = outPlug.child(0);
				MDGModifier modifier;
				modifier.connect(out_valueChildPlug, attributePlug);
				modifier.doIt();
			}
			else {
				MDGModifier modifier;
				for (size_t i = 0; i < connectionNumber; i++)
				{
					MPlug attr_valueChildPlug = attributePlug.child((unsigned int)i);
					MPlug out_valueChildPlug = outPlug.child((unsigned int)i);

					modifier.connect(out_valueChildPlug, attr_valueChildPlug);
				}
				modifier.doIt();
			}


		}
		break;
	}
	case RPR_MATERIAL_NODE_INPUT_TYPE_IMAGE:
	{
		MString newImage = directoryPath + MString(attrValue.c_str());

		MFileObject fileObject;
		fileObject.setRawFullName(newImage);

		// Locate the image - either in the material
		// directory, or in the shared maps directory.
		if (!fileObject.exists())
		{
			newImage = directoryPath + "../" + MString(attrValue.c_str());
			fileObject.setRawFullName(newImage);

			if (!fileObject.exists())
			{
				// The image was not found in either location.
				MGlobal::displayError("Unable to find image " + newImage);
				return;
			}
		}

		// Import the image if required, or reference it directly
		// at the location where the material library is installed.
		if (m_importImages)
		{
			MString path = importImageFile(fileObject);
			attributePlug.setValue(path);
		}
		else
			attributePlug.setValue(newImage);

		break;
	}
	default:
	{
		if (attrName.compare("gamma") != 0) {
			//gamma is known, just not supported in Maya as of yet.
			MGlobal::displayError("Unknown shader attribute type");
		}

		return;
		break;
	}
	}
}

MString FireRenderXmlImportCmd::importImageFile(const MFileObject& file) const
{
	// Get the path to the imported file.
	MString sourcePath = file.rawFullName();
	MString destDirectory = getSourceImagesDirectory(sourcePath);
	MString destPath = destDirectory + file.name();

	// Check if the imported file already exists.
	MFileObject destFile;
	destFile.setRawFullName(destPath);

	// Copy the file unless it already exists.
	if (!destFile.exists())
	{
		try
		{
			if (!copyFile(sourcePath.asChar(), destPath.asChar()))
				return importImageFileError(sourcePath, destDirectory);
		}
		catch (std::fstream::failure e)
		{
			return importImageFileError(sourcePath, destDirectory);
		}
	}

	return destPath;
}

MString FireRenderXmlImportCmd::importImageFileError(const MString& sourcePath, const MString& destDirectory) const
{
	FireRenderError("Image Import Error", "Unable to copy " + sourcePath + " to " + destDirectory);
	return sourcePath;
}

MString FireRenderXmlImportCmd::getSourceImagesDirectory(const MString& filePath) const
{
	// Get the name of the material library
	// directory that contains the image file.
	MString containingDirectory = getContainingDirectory(filePath, true);

	// Create a matching directory in
	// the project source images directory.
	MString sourceImagesDirectory = getSourceImagesPath();
	sourceImagesDirectory += "/";
	sourceImagesDirectory += containingDirectory;

	// Replace backslashes with forward slashes.
	sourceImagesDirectory.substitute("\\", "/");

	// Ensure the directory exists.
	MCommonSystemUtils::makeDirectory(sourceImagesDirectory);

	return sourceImagesDirectory;
}

void FireRenderXmlImportCmd::attachToPlace2dTextureNode(MObject objectToConnectTo)
{
	MaterialNode& placementNode = nodeGroup[Place2dNodeName];
	if (!placementNode.parsed)
	{
		placementNode.parsedObject = createPlace2dTextureNode(placementNode.name);
		placementNode.parsed = true;
	}
	connectPlace2dTextureNodeTo(placementNode.parsedObject, objectToConnectTo);
}

MObject FireRenderXmlImportCmd::createPlace2dTextureNode(std::string name)
{
	MString executeCommand = "shadingNode place2dTexture  -asUtility -n \""_ms + name.c_str() + "\""_ms;

	MString shaderName = MGlobal::executeCommandStringResult(executeCommand);
	MSelectionList sList;
	sList.add(shaderName);
	MObject shaderNode;
	sList.getDependNode(0, shaderNode);
	if (shaderNode.isNull())
	{
		MGlobal::displayError("Unable to create place2dTexture");
		return shaderNode;
	}

	return shaderNode;
}

void FireRenderXmlImportCmd::connectPlace2dTextureNodeTo(MObject place2dTextureNode, MObject objectToConnectTo)
{
	MFnDependencyNode outputNodeFn(place2dTextureNode);
	MFnDependencyNode inputNodeFn(objectToConnectTo);

	MPlug outPlug = outputNodeFn.findPlug("o");
	if (outPlug.isNull())
	{
		outPlug = outputNodeFn.findPlug("out");

		if (outPlug.isNull())
			outPlug = outputNodeFn.findPlug("output");

		if (outPlug.isNull())
		{
			MGlobal::displayError("Error while trying to find output plug");
			return;
		}
	}

	MPlug uvInputPlug = inputNodeFn.findPlug("uv");
	if (uvInputPlug.isNull())
	{
		MGlobal::displayError("Error while trying to find input uv plug");
		return;
	}

	MDGModifier modifier;
	modifier.connect(outPlug, uvInputPlug);
	modifier.doIt();

}

MObject FireRenderXmlImportCmd::createShadingNode(MString materialName, std::map<std::string, std::string> &attributeMapper, std::map<std::string, Param> &params, ShadingNodeType shadingNodeType, frw::ShaderType shaderType)
{
	MString as;
	switch (shadingNodeType)
	{
	case ShadingNodeType::asLight:
		as = "asLight";
		break;
	case ShadingNodeType::asPostProcess:
		as = "asPostProcess";
		break;
	case ShadingNodeType::asRendering:
		as = "asRendering";
		break;
	case ShadingNodeType::asTexture:
		as = "asTexture";
		break;
	case ShadingNodeType::asUtility:
		as = "asUtility";
		break;
	default:
	case ShadingNodeType::asShader:
		as = "asShader";
		break;
	}
	MString executeCommand = "shadingNode -" + as + " " + materialName;

	MString shaderName = MGlobal::executeCommandStringResult(executeCommand);
	MSelectionList sList;
	sList.add(shaderName);
	MObject shaderNode;
	sList.getDependNode(0, shaderNode);
	if (shaderNode.isNull())
	{
		MGlobal::displayError("Unable to create diffuse shader");
		return shaderNode;
	}

	MFnDependencyNode nodeFn(shaderNode);

	if (shaderType != frw::ShaderTypeInvalid)
	{
		auto matType = FireMaya::Material::kDiffuse;
		switch (shaderType)
		{
		case frw::ShaderTypeDiffuse:
			matType = FireMaya::Material::kDiffuse;
			break;
		case frw::ShaderTypeMicrofacet:
			matType = FireMaya::Material::kMicrofacet;
			break;
		case frw::ShaderTypeReflection:
			matType = FireMaya::Material::kReflect;
			break;
		case frw::ShaderTypeRefraction:
			matType = FireMaya::Material::kRefract;
			break;
		case frw::ShaderTypeMicrofacetRefraction:
			matType = FireMaya::Material::kMicrofacetRefract;
			break;
		case frw::ShaderTypeTransparent:
			matType = FireMaya::Material::kTransparent;
			break;
		case frw::ShaderTypeEmissive:
			matType = FireMaya::Material::kEmissive;
			break;
		case frw::ShaderTypeWard:
			matType = FireMaya::Material::kWard;
			break;
		case frw::ShaderTypeOrenNayer:
			matType = FireMaya::Material::kOrenNayar;
			break;
		case frw::ShaderTypeDiffuseRefraction:
			matType = FireMaya::Material::kDiffuseRefraction;
			break;
		case frw::ShaderTypeInvalid: break;
		case frw::ShaderTypeBlend: break;
		case frw::ShaderTypeStandard: break;
		case frw::ShaderTypeAdd: break;
		default: break;
		}
		MPlug nodeTypePlug = nodeFn.findPlug("type");
		if (!nodeTypePlug.isNull())
			nodeTypePlug.setValue(matType);
	}

	for (auto param : params)
	{
		parseAttributeParam(shaderNode, attributeMapper, param.first, param.second);
	}

	return shaderNode;
}

MObject FireRenderXmlImportCmd::loadFireRenderMaterialShader(frw::ShaderType shaderType, std::map<std::string, Param> &params)
{
	std::map<std::string, std::string> attributeMapper;
	attributeMapper["color"] = "color";
	attributeMapper["normal"] = "normalMap";
	attributeMapper["roughness"] = "roughness";
	attributeMapper["ior"] = "refractiveIndex";
	attributeMapper["roughnessx"] = "roughnessX";
	attributeMapper["roughnessy"] = "roughnessY";
	attributeMapper["roughness_x"] = "roughnessX";
	attributeMapper["roughness_y"] = "roughnessY";
	attributeMapper["rotation"] = "rotation";

	MString materialName = MString(FIRE_RENDER_NODE_PREFIX) + "Material";

	return createShadingNode(materialName, attributeMapper, params, ShadingNodeType::asShader, shaderType);
}

MObject FireRenderXmlImportCmd::loadFireRenderAddMaterial(std::map<std::string, Param> &params) {
	std::map<std::string, std::string> attributeMapper;
	attributeMapper["color0"] = "color0";
	attributeMapper["color1"] = "color1";

	MString materialName = MString(FIRE_RENDER_NODE_PREFIX) + "AddMaterial";

	return createShadingNode(materialName, attributeMapper, params, ShadingNodeType::asUtility);
}

MObject FireRenderXmlImportCmd::loadFireRenderBlendMaterial(std::map<std::string, Param> &params) {
	std::map<std::string, std::string> attributeMapper;
	attributeMapper["color0"] = "color0";
	attributeMapper["color1"] = "color1";
	attributeMapper["weight"] = "weight";

	MString materialName = MString(FIRE_RENDER_NODE_PREFIX) + "BlendMaterial";

	return createShadingNode(materialName, attributeMapper, params, ShadingNodeType::asShader);
}

MObject FireRenderXmlImportCmd::loadFireRenderStandardMaterial(std::map<std::string, Param> &params)
{
	std::map<std::string, std::string> attributeMapper;

	attributeMapper["diffuse.color"] = "diffuseColor";
	attributeMapper["diffuse.normal"] = "diffuseNormal";
	attributeMapper["weights.glossy2diffuse"] = "reflectIOR";
	attributeMapper["glossy.color"] = "reflectColor";
	attributeMapper["glossy.roughness_x"] = "reflectRoughnessX";
	attributeMapper["glossy.roughness_y"] = "reflectRoughnessY";
	attributeMapper["glossy.normal"] = "reflectNormal";
	attributeMapper["weights.clearcoat2glossy"] = "coatIOR";
	attributeMapper["clearcoat.color"] = "coatColor";
	attributeMapper["clearcoat.normal"] = "coatNormal";
	attributeMapper["refraction.color"] = "refractColor";
	attributeMapper["refraction.normal"] = "refNormal";
	attributeMapper["refraction.ior"] = "refractIOR";
	attributeMapper["refraction.roughness"] = "refractRoughness";
	attributeMapper["weights.diffuse2refraction"] = "refraction";
	attributeMapper["transparency.color"] = "transparencyColor";
	attributeMapper["weights.transparency"] = "transparencyLevel";

	MString materialName = MString(FIRE_RENDER_NODE_PREFIX) + "StandardMaterial";

	return createShadingNode(materialName, attributeMapper, params, ShadingNodeType::asShader);
}
MObject FireRenderXmlImportCmd::loadFireRenderArithmeticMaterial(std::map<std::string, Param> &params) {
	std::map<std::string, std::string> attributeMapper;
	attributeMapper["color0"] = "inputA";
	attributeMapper["color1"] = "inputB";
	attributeMapper["op"] = "operation";

	MString materialName = MString(FIRE_RENDER_NODE_PREFIX) + "Arithmetic";

	return createShadingNode(materialName, attributeMapper, params, ShadingNodeType::asUtility);
}

MObject FireRenderXmlImportCmd::loadFireRenderFresnel(std::map<std::string, Param> &params) {
	std::map<std::string, std::string> attributeMapper;
	attributeMapper["ior"] = "ior";
	attributeMapper["invec"] = "inVec";
	attributeMapper["normal"] = "normalMap";
	attributeMapper["n"] = "normalMap";

	MString materialName = MString(FIRE_RENDER_NODE_PREFIX) + "Fresnel";

	return createShadingNode(materialName, attributeMapper, params, ShadingNodeType::asUtility);
}

MObject FireRenderXmlImportCmd::loadFireRenderFresnelSchlick(std::map<std::string, Param> &params) {
	std::map<std::string, std::string> attributeMapper;
	attributeMapper["reflectance"] = "reflectance";
	attributeMapper["normal"] = "normalMap";
	attributeMapper["n"] = "normalMap";
	attributeMapper["invec"] = "inVec";

	MString materialName = MString(FIRE_RENDER_NODE_PREFIX) + "FresnelSchlick";

	return createShadingNode(materialName, attributeMapper, params, ShadingNodeType::asUtility);
}

MObject FireRenderXmlImportCmd::loadFireRenderNormal(std::map<std::string, Param> &params) {
	std::map<std::string, std::string> attributeMapper;

	attributeMapper["data"] = "color";
	attributeMapper["bumpscale"] = "strength";

	MString materialName = MString(FIRE_RENDER_NODE_PREFIX) + "Normal";

	return createShadingNode(materialName, attributeMapper, params, ShadingNodeType::asTexture);
}

MObject FireRenderXmlImportCmd::loadFireRenderImageTextureNode(std::map<std::string, Param> &params) {
	std::map<std::string, std::string> attributeMapper;
	//TODO: UVCOORD: CHECK-Test IT
	attributeMapper["uv"] = "uvCoord";
	attributeMapper["path"] = "filename";

	MString materialName = MString(FIRE_RENDER_NODE_PREFIX) + "Texture";

	return createShadingNode(materialName, attributeMapper, params, ShadingNodeType::asTexture);
}

MObject FireRenderXmlImportCmd::loadFireRenderNoiseNode(std::map<std::string, Param> &params) {
	std::map<std::string, std::string> attributeMapper;
	//TODO: UVCOORD: CHECK-Test IT
	attributeMapper["uv"] = "uvCoord";
	attributeMapper["color"] = "color";

	MString materialName = MString(FIRE_RENDER_NODE_PREFIX) + "Noise";

	return createShadingNode(materialName, attributeMapper, params, ShadingNodeType::asTexture);
}

MObject FireRenderXmlImportCmd::loadFireRenderDotNode(std::map<std::string, Param> &params) {
	std::map<std::string, std::string> attributeMapper;
	//TODO: UVCOORD: CHECK-Test IT
	attributeMapper["uv"] = "uvCoord";
	//range is actually not supported in export:
	//attributeMapper["range"] = "range";
	attributeMapper["uv_scale"] = "uvScale";


	MString materialName = MString(FIRE_RENDER_NODE_PREFIX) + "Dot";

	return createShadingNode(materialName, attributeMapper, params, ShadingNodeType::asTexture);
}

MObject FireRenderXmlImportCmd::loadFireRenderGradientNode(std::map<std::string, Param> &params) {
	std::map<std::string, std::string> attributeMapper;
	//TODO: UVCOORD: CHECK-Test IT
	attributeMapper["uv"] = "uvCoord";
	attributeMapper["color0"] = "color0";
	attributeMapper["color1"] = "color1";

	MString materialName = MString(FIRE_RENDER_NODE_PREFIX) + "Gradient";

	return createShadingNode(materialName, attributeMapper, params, ShadingNodeType::asTexture);
}

MObject FireRenderXmlImportCmd::loadFireRenderCheckerNode(std::map<std::string, Param> &params) {
	std::map<std::string, std::string> attributeMapper;
	//TODO: UVCOORD: CHECK-Test IT
	attributeMapper["uv"] = "uvCoord";

	MString materialName = MString(FIRE_RENDER_NODE_PREFIX) + "Checker";

	return createShadingNode(materialName, attributeMapper, params, ShadingNodeType::asTexture);
}

MObject FireRenderXmlImportCmd::loadFireRenderLookupNode(std::map<std::string, Param> &params) {
	std::map<std::string, std::string> attributeMapper;
	attributeMapper["value"] = "type";

	MString materialName = MString(FIRE_RENDER_NODE_PREFIX) + "Lookup";

	return createShadingNode(materialName, attributeMapper, params, ShadingNodeType::asUtility);
}

MObject FireRenderXmlImportCmd::loadFireRenderBlendValueNode(std::map<std::string, Param> &params) {
	std::map<std::string, std::string> attributeMapper;
	attributeMapper["color0"] = "inputA";
	attributeMapper["color1"] = "inputB";
	attributeMapper["weight"] = "weight";

	MString materialName = MString(FIRE_RENDER_NODE_PREFIX) + "BlendValue";

	return createShadingNode(materialName, attributeMapper, params, ShadingNodeType::asUtility);
}

MObject FireRenderXmlImportCmd::loadFireRenderPassthroughNode(std::map<std::string, Param> &params) {
	std::map<std::string, std::string> attributeMapper;
	attributeMapper["color"] = "color";

	MString materialName = MString(FIRE_RENDER_NODE_PREFIX) + "Passthrough";

	return createShadingNode(materialName, attributeMapper, params, ShadingNodeType::asUtility);
}

MObject FireRenderXmlImportCmd::loadFireRenderBumpNode(std::map<std::string, Param> &params) {
	std::map<std::string, std::string> attributeMapper;

	attributeMapper["data"] = "color";
	attributeMapper["bumpscale"] = "strength";

	MString materialName = MString(FIRE_RENDER_NODE_PREFIX) + "Bump";

	return createShadingNode(materialName, attributeMapper, params, ShadingNodeType::asTexture);
}

int FireRenderXmlImportCmd::getMatType(std::string materialType) {
	int matType = -1;

	if (materialType == "DIFFUSE") { matType = RPR_MATERIAL_NODE_DIFFUSE; }
	else if (materialType == "MICROFACET") { matType = RPR_MATERIAL_NODE_MICROFACET; }
	else if (materialType == "REFLECTION") { matType = RPR_MATERIAL_NODE_REFLECTION; }
	else if (materialType == "REFRACTION") { matType = RPR_MATERIAL_NODE_REFRACTION; }
	else if (materialType == "MICROFACET_REFRACTION") { matType = RPR_MATERIAL_NODE_MICROFACET_REFRACTION; }
	else if (materialType == "TRANSPARENT") { matType = RPR_MATERIAL_NODE_TRANSPARENT; }
	else if (materialType == "EMISSIVE") { matType = RPR_MATERIAL_NODE_EMISSIVE; }
	else if (materialType == "WARD") { matType = RPR_MATERIAL_NODE_WARD; }
	else if (materialType == "ADD") { matType = RPR_MATERIAL_NODE_ADD; }
	else if (materialType == "BLEND") { matType = RPR_MATERIAL_NODE_BLEND; }
	else if (materialType == "ARITHMETIC") { matType = RPR_MATERIAL_NODE_ARITHMETIC; }
	else if (materialType == "FRESNEL") { matType = RPR_MATERIAL_NODE_FRESNEL; }
	else if (materialType == "NORMAL_MAP") { matType = RPR_MATERIAL_NODE_NORMAL_MAP; }
	else if (materialType == "IMAGE_TEXTURE") { matType = RPR_MATERIAL_NODE_IMAGE_TEXTURE; }
	else if (materialType == "INPUT_TEXTURE") { matType = RPR_MATERIAL_NODE_IMAGE_TEXTURE; }
	else if (materialType == "NOISE2D_TEXTURE") { matType = RPR_MATERIAL_NODE_NOISE2D_TEXTURE; }
	else if (materialType == "DOT_TEXTURE") { matType = RPR_MATERIAL_NODE_DOT_TEXTURE; }
	else if (materialType == "GRADIENT_TEXTURE") { matType = RPR_MATERIAL_NODE_GRADIENT_TEXTURE; }
	else if (materialType == "CHECKER_TEXTURE") { matType = RPR_MATERIAL_NODE_CHECKER_TEXTURE; }
	else if (materialType == "CONSTANT_TEXTURE") { matType = RPR_MATERIAL_NODE_CONSTANT_TEXTURE; }
	else if (materialType == "INPUT_LOOKUP") { matType = RPR_MATERIAL_NODE_INPUT_LOOKUP; }
	else if (materialType == "STANDARD") { matType = RPR_MATERIAL_NODE_STANDARD; }
	else if (materialType == "BLEND_VALUE") { matType = RPR_MATERIAL_NODE_BLEND_VALUE; }
	else if (materialType == "PASSTHROUGH") { matType = RPR_MATERIAL_NODE_PASSTHROUGH; }
	else if (materialType == "ORENNAYAR") { matType = RPR_MATERIAL_NODE_ORENNAYAR; }
	else if (materialType == "FRESNEL_SCHLICK") { matType = RPR_MATERIAL_NODE_FRESNEL_SCHLICK; }
	else if (materialType == "DIFFUSE_REFRACTION") { matType = RPR_MATERIAL_NODE_DIFFUSE_REFRACTION; }
	else if (materialType == "BUMP_MAP") { matType = RPR_MATERIAL_NODE_BUMP_MAP; }

	return matType;
}

MObject FireRenderXmlImportCmd::parseMaterialNode(MaterialNode &matNode)
{
	int matType = getMatType(matNode.type);
	if (!matNode.parsed)
	{
		matNode.parsed = true;
		matNode.parsedObject = parseShader(matType, matNode.params);
		MFnDependencyNode node(matNode.parsedObject);
		node.setName(matNode.name.c_str());
	}
	return matNode.parsedObject;
}

MObject FireRenderXmlImportCmd::parseShader(int &matType, std::map<std::string, Param> &params)
{
	MObject shaderNode;

	switch (matType)
	{
	case RPR_MATERIAL_NODE_DIFFUSE:
	case RPR_MATERIAL_NODE_DIFFUSE_REFRACTION:
	case RPR_MATERIAL_NODE_MICROFACET:
	case RPR_MATERIAL_NODE_REFLECTION:
	case RPR_MATERIAL_NODE_REFRACTION:
	case RPR_MATERIAL_NODE_MICROFACET_REFRACTION:
	case RPR_MATERIAL_NODE_TRANSPARENT:
	case RPR_MATERIAL_NODE_EMISSIVE:
	case RPR_MATERIAL_NODE_WARD:
	case RPR_MATERIAL_NODE_ORENNAYAR:
		shaderNode = loadFireRenderMaterialShader(frw::ShaderType(matType), params);
		break;

	case RPR_MATERIAL_NODE_ADD:
	{
		shaderNode = loadFireRenderAddMaterial(params);
		break;
	}
	case RPR_MATERIAL_NODE_BLEND:
	{
		shaderNode = loadFireRenderBlendMaterial(params);
		break;
	}
	case RPR_MATERIAL_NODE_ARITHMETIC:
	{
		shaderNode = loadFireRenderArithmeticMaterial(params);
		break;
	}
	case RPR_MATERIAL_NODE_FRESNEL:
	{
		shaderNode = loadFireRenderFresnel(params);
		break;
	}
	case RPR_MATERIAL_NODE_FRESNEL_SCHLICK:
	{
		shaderNode = loadFireRenderFresnelSchlick(params);
		break;
	}
	case RPR_MATERIAL_NODE_NORMAL_MAP:
	{
		shaderNode = loadFireRenderNormal(params);

		break;
	}
	case RPR_MATERIAL_NODE_IMAGE_TEXTURE:
	{
		if (params.find("data") == params.end()) {
			//xml file has the image node separated into two levels
			//this use case has it one level, due to changes done to normal map,
			//hence we don't need to grab the data from the second level ("level below")
			shaderNode = loadFireRenderImageTextureNode(params);

			attachToPlace2dTextureNode(shaderNode);
		}
		else {
			Param tempParam = params["data"];
			MaterialNode textureNode = nodeGroup[tempParam.value];

			std::map<std::string, Param> paramsToParse;
			for (auto param : params)
			{
				if (param.first != "data")
				{
					paramsToParse[param.first] = param.second;
				}
			}
			for (auto param : textureNode.params)
			{
				paramsToParse[param.first] = param.second;
			}

			shaderNode = loadFireRenderImageTextureNode(paramsToParse);
			textureNode.parsed = true;
			textureNode.parsedObject = shaderNode;

			attachToPlace2dTextureNode(shaderNode);
		}

		break;
	}
	case RPR_MATERIAL_NODE_NOISE2D_TEXTURE:
	{
		shaderNode = loadFireRenderNoiseNode(params);

		attachToPlace2dTextureNode(shaderNode);
		break;
	}
	case RPR_MATERIAL_NODE_DOT_TEXTURE:
	{
		shaderNode = loadFireRenderDotNode(params);

		attachToPlace2dTextureNode(shaderNode);

		break;
	}
	case RPR_MATERIAL_NODE_GRADIENT_TEXTURE:
	{
		shaderNode = loadFireRenderGradientNode(params);

		attachToPlace2dTextureNode(shaderNode);

		break;
	}
	case RPR_MATERIAL_NODE_CHECKER_TEXTURE:
	{
		shaderNode = loadFireRenderCheckerNode(params);

		attachToPlace2dTextureNode(shaderNode);

		break;
	}
	case RPR_MATERIAL_NODE_CONSTANT_TEXTURE:
	{
		break;
	}
	case RPR_MATERIAL_NODE_INPUT_LOOKUP:
	{
		shaderNode = loadFireRenderLookupNode(params);
		break;
	}
	case RPR_MATERIAL_NODE_STANDARD:
	{
		shaderNode = loadFireRenderStandardMaterial(params);
		break;
	}
	case RPR_MATERIAL_NODE_BLEND_VALUE:
	{
		shaderNode = loadFireRenderBlendValueNode(params);
		break;
	}
	case RPR_MATERIAL_NODE_PASSTHROUGH:
	{
		shaderNode = loadFireRenderPassthroughNode(params);
		break;
	}
	case RPR_MATERIAL_NODE_BUMP_MAP:
	{
		shaderNode = loadFireRenderBumpNode(params);

		break;
	}
	default:
		break;
	}

	return shaderNode;
}
