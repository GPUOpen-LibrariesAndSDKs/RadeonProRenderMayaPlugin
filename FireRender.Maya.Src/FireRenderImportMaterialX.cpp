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
#include "FireRenderImportMaterialX.h"

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
#include "FileSystemUtils.h"


FireRenderMaterialXImportCmd::FireRenderMaterialXImportCmd()
{
}

FireRenderMaterialXImportCmd::~FireRenderMaterialXImportCmd()
{
}

void* FireRenderMaterialXImportCmd::creator()
{
	return new FireRenderMaterialXImportCmd;
}

MSyntax FireRenderMaterialXImportCmd::newSyntax()
{
	MSyntax syntax;

	CHECK_MSTATUS(syntax.addFlag(kMaterialFilePathFlag, kMaterialFilePathFlagLong, MSyntax::kString));
	CHECK_MSTATUS(syntax.addFlag(kMaterialNameFlag, kMaterialNameFlagLong, MSyntax::kString));

	return syntax;
}

std::tuple<MStatus, MString> GetFilePath(const MArgDatabase& argData)
{
	// back-off
	if (!argData.isFlagSet(kMaterialFilePathFlag))
	{
		MGlobal::displayError("File path is missing, use -file flag");
		return std::make_tuple(MS::kFailure, MString());
	}

	MString filePath;
	argData.getFlagArgument(kMaterialFilePathFlag, 0, filePath);

	return std::make_tuple(MS::kSuccess, filePath);
}

std::tuple<MStatus, MString> GetName(const MArgDatabase& argData)
{
	// back-off
	if (!argData.isFlagSet(kMaterialNameFlag))
	{
		MGlobal::displayError("Material name is missing, use -name flag");
		return std::make_tuple(MS::kFailure, MString());
	}

	MString materialName;
	argData.getFlagArgument(kMaterialNameFlag, 0, materialName);

	return std::make_tuple(MS::kSuccess, materialName);
}

MStatus FireRenderMaterialXImportCmd::doIt(const MArgList& args)
{
	MStatus result = MS::kSuccess;
	MArgDatabase argData(syntax(), args);

	// Get material name
	MString materialName;
	std::tie(result, materialName) = GetName(argData);
	if (MS::kSuccess != result)
		return result;

	// create node
	MString executeCommand = "shadingNode -name \"" + materialName + "\" -asShader RPRMaterialXMaterial";

	MString shaderName = MGlobal::executeCommandStringResult(executeCommand);
	MSelectionList sList;
	sList.add(shaderName);
	MObject shaderNode;
	sList.getDependNode(0, shaderNode);
	if (shaderNode.isNull())
	{
		MGlobal::displayError("Unable to create shader!");
		return MS::kFailure;
	}

	// create shading groupand connect it with shader node
	MString sgCommand = "sets -renderable true -noSurfaceShader true -empty -name " + shaderName + "SG";
	MString sgName = MGlobal::executeCommandStringResult(sgCommand);
	MString connectCommand = "connectAttr -f " + shaderName + ".outColor " + sgName + ".surfaceShader";
	MGlobal::executeCommandStringResult(connectCommand);

	// Get path to file describing material
	MString filePath;
	std::tie(result, filePath) = GetFilePath(argData);
	if (MS::kSuccess != result)
		return result;

	// Get directory name
	std::string directory = getDirectory(filePath.asChar());

	// write data into created shader node
	MFnDependencyNode nodeFn(shaderNode);
	MStatus status;
	MPlug filePlug = nodeFn.findPlug("filename", false, &status);
	assert(status == MStatus::kSuccess);
	if (MS::kSuccess != status)
	{
		return status;
	}

	status = filePlug.setString(filePath);
	assert(status == MStatus::kSuccess);
	if (MS::kSuccess != status)
	{
		return status;
	}

	// Success!
	return MS::kSuccess;
}

