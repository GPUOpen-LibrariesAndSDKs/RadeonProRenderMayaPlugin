#include "EnableSaveIntermediateCmd.h"
#include "FireRenderUtils.h"
#include <maya/MGlobal.h>
#include <maya/MString.h>
#include "GlobalRenderUtilsDataHolder.h"
#include <string>
#include <vector>
#include <iostream>
#include <sstream>

EnableSaveIntermediateCmd::EnableSaveIntermediateCmd()
{}

EnableSaveIntermediateCmd::~EnableSaveIntermediateCmd()
{}

void * EnableSaveIntermediateCmd::creator()
{
	return new EnableSaveIntermediateCmd;
}

MSyntax EnableSaveIntermediateCmd::newSyntax()
{
	MStatus status;
	MSyntax syntax;

	CHECK_MSTATUS(syntax.addFlag(kEnableSaveIntermediateFlag, kEnableSaveIntermediateFlagLong, MSyntax::kBoolean));
	CHECK_MSTATUS(syntax.addFlag(kImagesFolderPathFlag, kImagesFolderPathFlagLong, MSyntax::kString));
	CHECK_MSTATUS(syntax.addFlag(kSaveIntermediateArgsFlag, kSaveIntermediateArgsFlagLong, MSyntax::kString));

	return syntax;
}

MStatus EnableSaveIntermediateCmd::doIt(const MArgList & args)
{
	MStatus status;
	MArgDatabase argData(syntax(), args);

	if (!argData.isFlagSet(kEnableSaveIntermediateFlag))
		return MS::kFailure;

	bool enable = false;
	argData.getFlagArgument(kEnableSaveIntermediateFlag, 0, enable);
	if (enable == false)
	{
		GlobalRenderUtilsDataHolder::GetGlobalRenderUtilsDataHolder()->SetEnabledSaveIntermediateImages(false);
		return MS::kSuccess;
	}
	
	MString folderPath;
	if (argData.isFlagSet(kImagesFolderPathFlag))
	{
		argData.getFlagArgument(kImagesFolderPathFlag, 0, folderPath);
	}
	else
	{
		MGlobal::displayError("Folder path is missing, use -folderpath flag");
		return MS::kFailure;
	}

	MString argsStream;
	if (argData.isFlagSet(kSaveIntermediateArgsFlag))
	{
		argData.getFlagArgument(kSaveIntermediateArgsFlag, 0, argsStream);
	}
	else
	{
		MGlobal::displayError("Args are missing, use -saveintermediateargs flag");
		return MS::kFailure;
	}

	std::string input (argsStream.asChar());
	std::istringstream ss(input);
	std::string token;
	std::vector<std::string> tokens;

	while (std::getline(ss, token, ' ')) 
	{
		tokens.push_back(token);
	}

	GlobalRenderUtilsDataHolder::GetGlobalRenderUtilsDataHolder()->SetEnabledSaveIntermediateImages(true);
	GlobalRenderUtilsDataHolder::GetGlobalRenderUtilsDataHolder()->SetIntermediateImagesFolder(folderPath.asChar());
	GlobalRenderUtilsDataHolder::GetGlobalRenderUtilsDataHolder()->SetIterationsToSave(tokens);

	return MS::kSuccess;
}
