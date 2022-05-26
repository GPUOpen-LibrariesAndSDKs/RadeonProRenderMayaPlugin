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

#include "Context/TahoeContext.h"
#include "FireRenderUtils.h"

#include <vector>
#include <set>
#include <string>
#include <regex>
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

#include "XMLMaterialExport/XMLMaterialExportCommon.h"

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

	NorthStarContext context;
	context.setCallbackCreationDisabled(true);

	rpr_int res = context.initializeContext();
	if (res != RPR_SUCCESS)
	{
		MString msg;
		FireRenderError(res, msg, true);
		return MS::kFailure;
	}

	context.setCallbackCreationDisabled(true);

	std::vector<std::string> namesUsed;
	std::set<frw::Shader> objectsProcessed;
	InitParamList();

	for (uint i = 0; i < sList.length(); i++)
	{
		MObject node;
		sList.getDependNode(i, node);
		if (node.isNull())
			continue;

		MObject shadingEngine;
		if (node.hasFn(MFn::kShadingEngine))
		{
			shadingEngine = node;
			node = getSurfaceShader(node);
		}

		MFnDependencyNode depNode(node);
		std::string material_name ( depNode.name().asChar() );

		frw::Shader shader = context.GetShader(node, shadingEngine);

		if (!shader)
		{
			ErrorPrint("Invalid shader");
			continue;
		}

		if (objectsProcessed.count(shader))
			continue;	// already handled

		objectsProcessed.insert(shader);

		rpr_material_node mat_rprx_context = nullptr;
		std::set<rpr_material_node> nodeList;
		std::unordered_map<rpr_image, RPR_MATERIAL_XML_EXPORT_TEXTURE_PARAM> textureParameter;

		std::string folderpath(filePath.asChar());
		folderpath += "/";

		bool exportImageUV = false; // ignore UV for IMAGE_TEXTURE nodes.  Because UV is exported with "tiling_u" & "tiling_v" in XML

		rpr_material_node materialNodeRPR = shader.Handle();

		bool success = ParseRPR(
			materialNodeRPR,
			nodeList,
			folderpath,
			false,
			textureParameter,
			material_name,
			exportImageUV);

		if (!success)
			return MStatus::kFailure;
		

		std::string curr_material_name;
		if (std::find(namesUsed.begin(), namesUsed.end(), material_name) == namesUsed.end())
		{
			namesUsed.push_back(material_name);
			curr_material_name = material_name;
		}
		else
		{
			curr_material_name = material_name;
			curr_material_name += "_";
			curr_material_name += std::to_string(i);
		}

		std::string fileExtension = ".xml";
		std::string res_filename = folderpath + curr_material_name + fileExtension;
		std::map<rpr_image, EXTRA_IMAGE_DATA> extraImageData;
		ExportMaterials(
			res_filename,
			nodeList,
			textureParameter,
			materialNodeRPR,
			material_name,
			extraImageData,
			exportImageUV
		);
	}

	return MS::kSuccess;
}


////////////////////////////////////////////////////////

FireRenderXmlImportCmd::FireRenderXmlImportCmd()
	: m_importImages(false)
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

#ifndef useNewImport
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
#endif

#ifdef useNewImport
MStatus FireRenderXmlImportCmd::doIt(const MArgList & args)
{
	MStatus result = MS::kSuccess;

	// Get path to file describing material
	MString filePath;
	MArgDatabase argData(syntax(), args);
	std::tie(result, filePath) = GetFilePath(argData);
	if (MS::kSuccess != result)
		return result;

	MGlobal::displayInfo("Began loading material from material library, path to material is: " + filePath);

	// Get directory name
	std::string directory = getDirectory(filePath.asChar());
	m_directoryPath = MString(directory.c_str());

	// Get Material Name
	std::string userDefinedMaterialName;
	if (argData.isFlagSet(kMaterialNameFlag))
	{
		MString temp;
		argData.getFlagArgument(kMaterialNameFlag, 0, temp);
		userDefinedMaterialName = temp.asChar();
	}

	// Import textures if required.
	if (argData.isFlagSet(kImportImages))
		argData.getFlagArgument(kImportImages, 0, m_importImages);

	// Read nodes from .xml
	std::string materialName;
	nodeGroup.clear();
	result = ImportMaterials(filePath.asChar(), nodeGroup, materialName) ? MS::kSuccess : MS::kFailure;

	if (MS::kSuccess == result)
	{
		MGlobal::displayInfo("Succesfully loaded material " + MString(materialName.c_str()) + "from material library!\n");
	}
	else
	{
		MGlobal::displayError("Failed to load material " + MString(materialName.c_str()) + "from material library!\n");
	}

	if (MS::kSuccess != result)
		return result;

	// Get root node
	// - get node containing Uber Material data
	auto it = nodeGroup.begin();
	for (; it != nodeGroup.end() && !it->second.IsUber(); it++) {}
	if (nodeGroup.end() == it)
	{
		// no uber node found => maybe its blend or diffuse or reflective material
		for (it = nodeGroup.begin(); it != nodeGroup.end() && !it->second.IsSupportedMaterial(); it++) {}
		if (nodeGroup.end() == it)
		{
			// not a blend => ivalid .xml then
			nodeGroup.clear();
			return MS::kFailure;
		}
	}
	MaterialNode& rootNode = it->second;

	// Set material name
	if (userDefinedMaterialName.length() == 0)
	{
		rootNode.name = materialName;
	}
	else
	{
		rootNode.name = userDefinedMaterialName;
	}

	// Parse nodes
	// - should traverse the node graph starting from root (Uber material node)
	parseMaterialNode(rootNode);

	// Success!
	return MS::kSuccess;
}
#endif

int FireRenderXmlImportCmd::getAttrType(std::string attrTypeStr) {
	//FR_MATERIAL_NODE_INPUT_TYPE_FLOAT4 0x1
	//FR_MATERIAL_NODE_INPUT_TYPE_UINT 0x2
	//FR_MATERIAL_NODE_INPUT_TYPE_NODE 0x3
	//FR_MATERIAL_NODE_INPUT_TYPE_IMAGE 0x4
	//
	int attrType = -1;
	if ((attrTypeStr == "float4") || (attrTypeStr == "float") ) { attrType = RPR_MATERIAL_NODE_INPUT_TYPE_FLOAT4; }
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

void enableMaterialFlagByAttr(const std::string& plugName, MFnDependencyNode& nodeFn, bool isMap, float* floatData)
{
	if (!isMap && floatData == nullptr)
		return;

	bool isAttrSet = isMap;

	if (!isMap)
	{
		const float* fvalue = static_cast<const float*>(floatData);
		isAttrSet = fvalue[0] + fvalue[1] + fvalue[2] > 0.0f;
	}

	if (isAttrSet)
	{
		MPlug enablePlug = nodeFn.findPlug(plugName.c_str());
		enablePlug.setValue(true);
	}
	else
	{
		MPlug enablePlug = nodeFn.findPlug(plugName.c_str());
		enablePlug.setValue(false);
	}
}

void disableMaterialFlagByAttr(const std::string& plugName, MFnDependencyNode& nodeFn, bool isMap, void* floatData)
{
	if (!isMap && floatData == nullptr)
		return;

	bool isAttrSet = isMap;

	if (!isMap)
	{
		const float* fvalue = static_cast<const float*>(floatData);
		isAttrSet = fvalue[0] + fvalue[1] + fvalue[2] > 0.0f;
	}

	if (isAttrSet)
	{
		MPlug enablePlug = nodeFn.findPlug(plugName.c_str());
		enablePlug.setValue(false);
	}
	else
	{
		MPlug enablePlug = nodeFn.findPlug(plugName.c_str());
		enablePlug.setValue(true);
	}
}

void setMetallness(const std::string& plugName, MFnDependencyNode& nodeFn, bool, void* floatData)
{
	if (floatData == nullptr)
		return;

	const float* fvalue = static_cast<const float*>(floatData);
		
	MPlug metallnessPlug = nodeFn.findPlug(plugName.c_str());
	metallnessPlug.setValue(fvalue[0]);
}

using setter_func = std::function<void(const std::string&, MFnDependencyNode&, bool isMap, float* data)>;

static const std::map<std::string, std::tuple<std::string /*plugName*/, setter_func /*func*/>> AttrsSpecialCases =
{
	{"diffuse.weight",		{"diffuse",					enableMaterialFlagByAttr	} },
	{"reflection.weight",	{"reflections",				enableMaterialFlagByAttr	} },
	{"refraction.weight",	{"refraction",				enableMaterialFlagByAttr	} },
	{"coating.weight",		{"clearCoat",				enableMaterialFlagByAttr	} },
	{"emission.weight",		{"emissive",				enableMaterialFlagByAttr	} },
	{"sss.weight",			{"sssEnable",				enableMaterialFlagByAttr	} },
	{"transparency",		{"transparencyEnable",		enableMaterialFlagByAttr	} },
	{"displacement",		{"displacementEnable",		enableMaterialFlagByAttr	} },
	{"diffuse.normal",		{"useShaderNormal",			disableMaterialFlagByAttr	} },
	{"reflection.normal",	{"reflectUseShaderNormal",	disableMaterialFlagByAttr	} },
	{"coating.normal",		{"coatUseShaderNormal",		disableMaterialFlagByAttr	} },
	{"reflection.ior",		{"reflectMetalness",		setMetallness				} }
};

void ConnectPlugToAttribute(MPlug& outPlug, MPlug& attributePlug, std::string& attrValue)
{
	const int outChildNumber = outPlug.numChildren();
	const int attributeChildNumber = attributePlug.numChildren();

	if (outChildNumber == attributeChildNumber)
	{
		MDGModifier modifier;
		MStatus result = modifier.connect(outPlug, attributePlug);
		if (result != MS::kSuccess)
		{
			// report failure
			MGlobal::displayError("Error while trying to connect " + MString(attrValue.c_str()) + " value");
			return;
		}
		modifier.doIt();
	}

	else
	{
		const int connectionNumber = (outChildNumber > attributeChildNumber) ? attributeChildNumber : outChildNumber;

		if (connectionNumber == 0)
		{
			MPlug out_valueChildPlug = outPlug.child(0);
			MDGModifier modifier;
			MStatus result = modifier.connect(out_valueChildPlug, attributePlug);
			if (result != MS::kSuccess)
			{
				// report failure
				MGlobal::displayError("Error while trying to connect " + MString(attrValue.c_str()) + " value");
				return;
			}
			modifier.doIt();
		}
		else
		{
			MDGModifier modifier;
			for (size_t i = 0; i < connectionNumber; i++)
			{
				MPlug attr_valueChildPlug = attributePlug.child((unsigned int)i);
				MPlug out_valueChildPlug = outPlug.child((unsigned int)i);

				MStatus result = modifier.connect(out_valueChildPlug, attr_valueChildPlug);
				if (result != MS::kSuccess)
				{
					// report failure
					MGlobal::displayError("Error while trying to connect " + MString(attrValue.c_str()) + " value");
					return;
				}
			}
			modifier.doIt();
		}
	}
}

void FireRenderXmlImportCmd::parseAttributeParam(MObject shaderNode, 
	std::map<const std::string, std::string> &attributeMapper, 
	const std::string attrName, 
	const Param &attrParam) 
{
	MFnDependencyNode nodeFn(shaderNode);

	std::string		attrT = attrParam.type;
	int				attrType = getAttrType(attrT);
	std::string		attrValue = attrParam.value;

	MPlug attributePlug = nodeFn.findPlug(attributeMapper[attrName].c_str());
	if (attributePlug.isNull())
	{
		// report failure
#ifdef _DEBUG
		MGlobal::displayError("Can't find parameter '" + MString(attrName.c_str()) + "' in created Node");
#endif
		return;
	}

	switch (attrType)
	{
		case RPR_MATERIAL_NODE_INPUT_TYPE_FLOAT4:
		{
			float fvalue[4] = { 0.f, 0.f, 0.f, 0.f };
			parseFloat4(attrValue, fvalue);

			bool isColorValue = (attrName.find("color") != std::string::npos)
				|| (attrName.find("Color") != std::string::npos)
				|| (attrName == "sss.scatterDistance")
				|| (attrName == "sss.scatterColor");

			if (isColorValue)
			{
				MPlug redPlug = attributePlug.child(0);
				MPlug bluePlug = attributePlug.child(1);
				MPlug greenPlug = attributePlug.child(2);

				if (attrName != "coating.transmissionColor")
				{
					redPlug.setValue(fvalue[0]);
					bluePlug.setValue(fvalue[1]);
					greenPlug.setValue(fvalue[2]);
				}
				else
				{
					redPlug.setValue(1.0f - fvalue[0]);
					bluePlug.setValue(1.0f - fvalue[1]);
					greenPlug.setValue(1.0f - fvalue[2]);
				}
				
			}
			else
			{
				attributePlug.setValue(fvalue[0]);
			}

			// handle special case (for uber)
			auto it = AttrsSpecialCases.find(attrName);
			if (it != AttrsSpecialCases.end())
			{
				std::string plugName;
				setter_func func;
				std::tie(plugName, func) = it->second;
				func(plugName, nodeFn, false, fvalue);
			}

			break;
		}
		case RPR_MATERIAL_NODE_INPUT_TYPE_UINT:
		{
			int value = std::stoi(attrValue);

			// handle special case (for uber)
			if (attrName == "reflection.mode")
			{
				if (value == RPR_UBER_MATERIAL_IOR_MODE_PBR)
				{
					MPlug metallnessPlug = nodeFn.findPlug("reflectMetalness");
					if (!metallnessPlug.isNull())
					{
						DisconnectFromPlug(metallnessPlug);
					}

					attributePlug.setValue(false);
				}
				else if (value == RPR_UBER_MATERIAL_IOR_MODE_METALNESS)
				{
					MPlug iorPlug = nodeFn.findPlug("reflectIOR");
					if (!iorPlug.isNull())
					{
						DisconnectFromPlug(iorPlug);
					}

					attributePlug.setValue(true);
				}

				break;
			}
			else if (attrName == "emission.mode")
			{
				if (value == RPR_UBER_MATERIAL_EMISSION_MODE_SINGLESIDED)
					attributePlug.setValue(false);
				else if (value == RPR_UBER_MATERIAL_EMISSION_MODE_DOUBLESIDED)
					attributePlug.setValue(true);
			}
			else
			{
				attributePlug.setValue(value);
			}

			break;
		}
		case RPR_MATERIAL_NODE_INPUT_TYPE_NODE:
		{
			MaterialNode& node = nodeGroup[attrValue];

			MObject inputShader = parseMaterialNode(node); // may return null if trying to parse fake lookUp node

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
#ifdef _DEBUG
				MGlobal::displayError("Error while reading " + MString(attrValue.c_str()) + " value");
#endif
				return;
			}

			// handle special case for metalness
			if (attrName == "reflection.ior")
			{
				MPlug metallnessPlug = nodeFn.findPlug("reflectMetalness");
				ConnectPlugToAttribute(outPlug, metallnessPlug, attrValue);
			}

			ConnectPlugToAttribute(outPlug, attributePlug, attrValue);

			// handle special case (for uber)
			auto it = AttrsSpecialCases.find(attrName);
			if (it != AttrsSpecialCases.end())
			{
				std::string plugName;
				setter_func func;
				std::tie(plugName, func) = it->second;
				func(plugName, nodeFn, true, nullptr);
			}

			break;
		}
		case RPR_MATERIAL_NODE_INPUT_TYPE_IMAGE:
		{
			MString newImage = m_directoryPath + MString(attrValue.c_str());

			MFileObject fileObject;
			fileObject.setRawFullName(newImage);

			// Locate the image - either in the material
			// directory, or in the shared maps directory.
			if (!fileObject.exists())
			{
				newImage = m_directoryPath + "../" + MString(attrValue.c_str());
				fileObject.setRawFullName(newImage);

				if (!fileObject.exists())
				{
					// Try find image by exact input path
					newImage = MString(attrValue.c_str());
					fileObject.setRawFullName(newImage);

					if (!fileObject.exists())
					{
						// The image was not found in either location.
						MGlobal::displayError("Unable to find image " + newImage);
						return;
					}
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

void FireRenderXmlImportCmd::attachToPlace2dTextureNode(MObject objectToConnectTo, const std::map<std::string, Param>& paramsToParse)
{
	// 2 cases: need to edit placement node params ans don't need
	auto it = paramsToParse.find("tiling_u");
	if (it != paramsToParse.end())
	{
		// - when need => create new placement node and fill it with parameters
		std::map<const std::string, std::string> attributeMapper;
		attributeMapper["tiling_u"] = "repeatU";
		attributeMapper["tiling_v"] = "repeatV";

		MObject parsedPlacementNode = createPlace2dTextureNode("Placement2dTexture");
		for (auto param : paramsToParse)
		{
			parseAttributeParam(parsedPlacementNode, attributeMapper, param.first, param.second);
		}

		connectPlace2dTextureNodeTo(parsedPlacementNode, objectToConnectTo);

		return;
	}

	// - when don't need => proceed
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
	MString executeCommand = "shadingNode place2dTexture  -asUtility -n " + MString(name.c_str());

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
	MStatus result = modifier.connect(outPlug, uvInputPlug);
	if (result != MS::kSuccess)
	{
		// report failure
		MGlobal::displayError("Error while trying to connect nodes value");
		return;
	}
	modifier.doIt();

}

MObject FireRenderXmlImportCmd::createShadingNode(MString materialName, std::map<const std::string, std::string> &attributeMapper, std::map<std::string, Param> &params, ShadingNodeType shadingNodeType, frw::ShaderType shaderType)
{
	// prepare command
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

	// create node
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

	// create shading group if necessary and connect it with shader node
	if (as == "asShader")
	{
		MString sgCommand = "sets -renderable true -noSurfaceShader true -empty -name " + shaderName + "SG";
		MString sgName = MGlobal::executeCommandStringResult(sgCommand);
		MString connectCommand = "connectAttr -f " + shaderName + ".outColor " + sgName + ".surfaceShader";
		MGlobal::executeCommandStringResult(connectCommand);
	}

	// fill node with data
	MFnDependencyNode nodeFn(shaderNode);
	MPlugArray arr;
	nodeFn.getConnections(arr);
	std::vector<std::string> names_plugs;
	size_t len = arr.length();
	for (int idx = 0; idx < len; ++idx)
	{
		const MPlug& tplug = arr[idx];
		names_plugs.push_back(std::string(tplug.name().asChar()));
	}

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
	std::map<const std::string, std::string> attributeMapper;
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
	std::map<const std::string, std::string> attributeMapper;
	attributeMapper["color0"] = "color0";
	attributeMapper["color1"] = "color1";

	MString materialName = MString(FIRE_RENDER_NODE_PREFIX) + "AddMaterial";

	return createShadingNode(materialName, attributeMapper, params, ShadingNodeType::asUtility);
}

MObject FireRenderXmlImportCmd::loadFireRenderBlendMaterial(std::map<std::string, Param> &params) {
	std::map<const std::string, std::string> attributeMapper;
	attributeMapper["color0"] = "color0";
	attributeMapper["color1"] = "color1";
	attributeMapper["weight"] = "weight";

	MString materialName = MString(FIRE_RENDER_NODE_PREFIX) + "BlendMaterial";

	return createShadingNode(materialName, attributeMapper, params, ShadingNodeType::asShader);
}

MObject FireRenderXmlImportCmd::loadFireRenderStandardMaterial(std::map<std::string, Param> &params)
{
	std::map<const std::string, std::string> attributeMapper;

	attributeMapper["diffuse.color"] = "diffuseColor";
	attributeMapper["diffuse.weight"] = "diffuseWeight";
	attributeMapper["diffuse.normal"] = "diffuseNormal";
	attributeMapper["diffuse.roughness"] = "diffuseRoughness";

	attributeMapper["reflection.color"] = "reflectColor";
	attributeMapper["reflection.weight"] = "reflectWeight";
	attributeMapper["reflection.roughness"] = "reflectRoughness";
	attributeMapper["reflection.anisotropy"] = "reflectAnisotropy";
	attributeMapper["reflection.anistropyRotation"] = "reflectAnisotropyRotation";
	attributeMapper["reflection.mode"] = "reflectMetalMaterial";
	attributeMapper["reflection.ior"] = "reflectIOR"; // OR "reflectMetalness"
	attributeMapper["reflection.normal"] = "reflectNormal";

	attributeMapper["weights.glossy2diffuse"] = "reflectIOR";
	attributeMapper["glossy.color"] = "reflectColor";
	attributeMapper["glossy.roughness_x"] = "reflectRoughnessX";
	attributeMapper["glossy.roughness_y"] = "reflectRoughnessY";
	attributeMapper["glossy.normal"] = "reflectNormal";

	attributeMapper["weights.clearcoat2glossy"] = "coatIOR";
	attributeMapper["clearcoat.color"] = "coatColor";
	attributeMapper["clearcoat.normal"] = "coatNormal";

	attributeMapper["coating.color"] = "coatColor";
	attributeMapper["coating.weight"] = "coatWeight";
	attributeMapper["coating.roughness"] = "coatRoughness";
	attributeMapper["coating.mode"] = "coatMode";
	attributeMapper["coating.ior"] = "coatIor";
	attributeMapper["coating.metalness"] = "coatMetalness";
	attributeMapper["coating.normal"] = "coatNormal";
	attributeMapper["coating.transmissionColor"] = "coatTransmissionColor";
	attributeMapper["coating.thickness"] = "coatThickness";

	attributeMapper["refraction.color"] = "refractColor";
	attributeMapper["refraction.weight"] = "refractWeight";
	attributeMapper["refraction.normal"] = "refNormal";
	attributeMapper["refraction.ior"] = "refractIor";
	attributeMapper["refraction.roughness"] = "refractRoughness";
	attributeMapper["refraction.thinSurface"] = "refractThinSurface";
	attributeMapper["refraction.absorptionColor"] = "refractAbsorbColor";
	attributeMapper["refraction.absorptionDistance"] = "refractAbsorptionDistance";
	attributeMapper["refraction.caustics"] = "refractAllowCaustics";
	attributeMapper["weights.diffuse2refraction"] = "refraction";

	attributeMapper["sheen"] = "sheen";
	attributeMapper["sheen.tint"] = "sheenTint";
	attributeMapper["sheen.weight"] = "sheenWeight";

	attributeMapper["emission.color"] = "emissiveColor";
	attributeMapper["emission.weight"] = "emissiveWeight";
	attributeMapper["emission.intensity"] = "emissiveIntensity";
	attributeMapper["emission.mode"] = "emissiveDoubleSided";

	attributeMapper["displacement"] = "displacementMap";

	attributeMapper["transparency"] = "transparencyLevel";
	attributeMapper["transparency.color"] = "transparencyColor";
	attributeMapper["weights.transparency"] = "transparencyLevel";

	attributeMapper["sss.scatterColor"] = "volumeScatter";
	attributeMapper["sss.scatterDistance"] = "subsurfaceRadius";
	attributeMapper["sss.scatterDirection"] = "scatteringDirection";
	attributeMapper["sss.weight"] = "sssWeight";
	attributeMapper["sss.multiscatter"] = "multipleScattering";
	attributeMapper["backscatter.weight"] = "backscatteringWeight";
	attributeMapper["backscatter.color"] = "backscatteringColor";

	MString materialName = MString(FIRE_RENDER_NODE_PREFIX) + "UberMaterial";

	return createShadingNode(materialName, attributeMapper, params, ShadingNodeType::asShader);
}
MObject FireRenderXmlImportCmd::loadFireRenderArithmeticMaterial(std::map<std::string, Param> &params) {
	std::map<const std::string, std::string> attributeMapper;
	attributeMapper["color0"] = "inputA";
	attributeMapper["color1"] = "inputB";
	attributeMapper["op"] = "operation";

	MString materialName = MString(FIRE_RENDER_NODE_PREFIX) + "Arithmetic";

	return createShadingNode(materialName, attributeMapper, params, ShadingNodeType::asUtility);
}

MObject FireRenderXmlImportCmd::loadFireRenderFresnel(std::map<std::string, Param> &params) {
	std::map<const std::string, std::string> attributeMapper;
	attributeMapper["ior"] = "ior";
	attributeMapper["invec"] = "inVec";
	attributeMapper["normal"] = "normalMap";
	attributeMapper["n"] = "normalMap";

	MString materialName = MString(FIRE_RENDER_NODE_PREFIX) + "Fresnel";

	return createShadingNode(materialName, attributeMapper, params, ShadingNodeType::asUtility);
}

MObject FireRenderXmlImportCmd::loadFireRenderFresnelSchlick(std::map<std::string, Param> &params) {
	std::map<const std::string, std::string> attributeMapper;
	attributeMapper["reflectance"] = "reflectance";
	attributeMapper["normal"] = "normalMap";
	attributeMapper["n"] = "normalMap";
	attributeMapper["invec"] = "inVec";

	MString materialName = MString(FIRE_RENDER_NODE_PREFIX) + "FresnelSchlick";

	return createShadingNode(materialName, attributeMapper, params, ShadingNodeType::asUtility);
}

MObject FireRenderXmlImportCmd::loadFireRenderNormal(std::map<std::string, Param> &params) {
	std::map<const std::string, std::string> attributeMapper;

	attributeMapper["data"] = "color";
	attributeMapper["bumpscale"] = "strength";
	attributeMapper["color"] = "color";

	MString materialName = MString(FIRE_RENDER_NODE_PREFIX) + "Normal";

	return createShadingNode(materialName, attributeMapper, params, ShadingNodeType::asTexture);
}

MObject FireRenderXmlImportCmd::loadFireRenderImageTextureNode(std::map<std::string, Param> &params) {
	std::map<const std::string, std::string> attributeMapper;
	//TODO: UVCOORD: CHECK-Test IT
	attributeMapper["uv"] = "uvCoord";
	attributeMapper["path"] = "filename";

	MString materialName = MString(FIRE_RENDER_NODE_PREFIX) + "Texture";

	return createShadingNode(materialName, attributeMapper, params, ShadingNodeType::asTexture);
}

MObject FireRenderXmlImportCmd::loadFireRenderNoiseNode(std::map<std::string, Param> &params) {
	std::map<const std::string, std::string> attributeMapper;
	//TODO: UVCOORD: CHECK-Test IT
	attributeMapper["uv"] = "uvCoord";
	attributeMapper["color"] = "color";

	MString materialName = MString(FIRE_RENDER_NODE_PREFIX) + "Noise";

	return createShadingNode(materialName, attributeMapper, params, ShadingNodeType::asTexture);
}

MObject FireRenderXmlImportCmd::loadFireRenderDotNode(std::map<std::string, Param> &params) {
	std::map<const std::string, std::string> attributeMapper;
	//TODO: UVCOORD: CHECK-Test IT
	attributeMapper["uv"] = "uvCoord";
	//range is actually not supported in export:
	//attributeMapper["range"] = "range";
	attributeMapper["uv_scale"] = "uvScale";


	MString materialName = MString(FIRE_RENDER_NODE_PREFIX) + "Dot";

	return createShadingNode(materialName, attributeMapper, params, ShadingNodeType::asTexture);
}

MObject FireRenderXmlImportCmd::loadFireRenderGradientNode(std::map<std::string, Param> &params) {
	std::map<const std::string, std::string> attributeMapper;
	//TODO: UVCOORD: CHECK-Test IT
	attributeMapper["uv"] = "uvCoord";
	attributeMapper["color0"] = "color0";
	attributeMapper["color1"] = "color1";

	MString materialName = MString(FIRE_RENDER_NODE_PREFIX) + "Gradient";

	return createShadingNode(materialName, attributeMapper, params, ShadingNodeType::asTexture);
}

MObject FireRenderXmlImportCmd::loadFireRenderCheckerNode(std::map<std::string, Param> &params) {
	std::map<const std::string, std::string> attributeMapper;
	//TODO: UVCOORD: CHECK-Test IT
	attributeMapper["uv"] = "uvCoord";

	MString materialName = MString(FIRE_RENDER_NODE_PREFIX) + "Checker";

	return createShadingNode(materialName, attributeMapper, params, ShadingNodeType::asTexture);
}

MObject FireRenderXmlImportCmd::loadFireRenderLookupNode(std::map<std::string, Param> &params) 
{
	std::map<const std::string, std::string> attributeMapper;
	attributeMapper["value"] = "type";

	// ensure that look up node is not fake
	if (params["value"].value == "0")
		return MObject();

	// proceed creating lookup node
	MString materialName = MString(FIRE_RENDER_NODE_PREFIX) + "Lookup";

	return createShadingNode(materialName, attributeMapper, params, ShadingNodeType::asUtility);
}

MObject FireRenderXmlImportCmd::loadFireRenderBlendValueNode(std::map<std::string, Param> &params) {
	std::map<const std::string, std::string> attributeMapper;
	attributeMapper["color0"] = "inputA";
	attributeMapper["color1"] = "inputB";
	attributeMapper["weight"] = "weight";

	MString materialName = MString(FIRE_RENDER_NODE_PREFIX) + "BlendValue";

	return createShadingNode(materialName, attributeMapper, params, ShadingNodeType::asUtility);
}

MObject FireRenderXmlImportCmd::loadFireRenderPassthroughNode(std::map<std::string, Param> &params) {
	std::map<const std::string, std::string> attributeMapper;
	attributeMapper["color"] = "color";

	MString materialName = MString(FIRE_RENDER_NODE_PREFIX) + "Passthrough";

	return createShadingNode(materialName, attributeMapper, params, ShadingNodeType::asUtility);
}

MObject FireRenderXmlImportCmd::loadFireRenderBumpNode(std::map<std::string, Param> &params) {
	std::map<const std::string, std::string> attributeMapper;

	attributeMapper["data"] = "color";
	attributeMapper["color"] = "color";
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
	else if (materialType == "STANDARD") { matType = RPR_MATERIAL_NODE_UBERV2; }
	else if (materialType == "BLEND_VALUE") { matType = RPR_MATERIAL_NODE_BLEND_VALUE; }
	else if (materialType == "PASSTHROUGH") { matType = RPR_MATERIAL_NODE_PASSTHROUGH; }
	else if (materialType == "ORENNAYAR") { matType = RPR_MATERIAL_NODE_ORENNAYAR; }
	else if (materialType == "FRESNEL_SCHLICK") { matType = RPR_MATERIAL_NODE_FRESNEL_SCHLICK; }
	else if (materialType == "DIFFUSE_REFRACTION") { matType = RPR_MATERIAL_NODE_DIFFUSE_REFRACTION; }
	else if (materialType == "BUMP_MAP") { matType = RPR_MATERIAL_NODE_BUMP_MAP; }
	else if (materialType == "UBER") { matType = RPR_MATERIAL_NODE_UBERV2; }

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
		node.setName(matNode.GetName().c_str());
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
		if (params.find("data") == params.end()) 
		{
			// this is INPUT_TEXTURE. do not want to create a node
			// SHOULDN'T BE HERE!

			//xml file has the image node separated into two levels
			//this use case has it one level, due to changes done to normal map,
			//hence we don't need to grab the data from the second level ("level below")
			shaderNode = loadFireRenderImageTextureNode(params);
			std::map<std::string, Param> params;
			attachToPlace2dTextureNode(shaderNode, params);
		}
		else 
		{
			// this is IMAGE_TEXTURE
			Param tempParam = params["data"];
			MaterialNode textureNode = nodeGroup[tempParam.value];

			// combine nodes to avoid creating "fake" nodes
			std::map<std::string, Param> paramsToParse;
			for (auto it = params.begin(); it != params.end(); ++it)
			{
				if (it->first == "data")
				{
					std::string connectedNodeName = it->second.value;
					MaterialNode connectedNode = nodeGroup[connectedNodeName];

					for (auto it2 = connectedNode.params.begin(); it2 != connectedNode.params.end(); ++it2)
					{
						paramsToParse[it2->first] = it2->second;
					}

					continue;
				}

				paramsToParse[it->first] = it->second;
			}

			shaderNode = loadFireRenderImageTextureNode(paramsToParse);

			textureNode.parsed = true;
			textureNode.parsedObject = shaderNode;

			attachToPlace2dTextureNode(shaderNode, paramsToParse);
		}

		break;
	}
	case RPR_MATERIAL_NODE_NOISE2D_TEXTURE:
	{
		shaderNode = loadFireRenderNoiseNode(params);

		attachToPlace2dTextureNode(shaderNode, params);
		break;
	}
	case RPR_MATERIAL_NODE_DOT_TEXTURE:
	{
		shaderNode = loadFireRenderDotNode(params);

		attachToPlace2dTextureNode(shaderNode, params);

		break;
	}
	case RPR_MATERIAL_NODE_GRADIENT_TEXTURE:
	{
		shaderNode = loadFireRenderGradientNode(params);

		attachToPlace2dTextureNode(shaderNode, params);

		break;
	}
	case RPR_MATERIAL_NODE_CHECKER_TEXTURE:
	{
		shaderNode = loadFireRenderCheckerNode(params);

		attachToPlace2dTextureNode(shaderNode, params);

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
	case RPR_MATERIAL_NODE_UBERV2:
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
