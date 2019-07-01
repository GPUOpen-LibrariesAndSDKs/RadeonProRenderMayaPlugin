#pragma once

#include <maya/MPxCommand.h> 
#include <maya/MSyntax.h> 
#include <maya/MArgDatabase.h> 

#define kEnableSaveIntermediateFlag "-esi" 
#define kEnableSaveIntermediateFlagLong "-enabledsaveintermediate" 
#define kImagesFolderPathFlag "-fp"  
#define kImagesFolderPathFlagLong "-folderpath"
#define kSaveIntermediateArgsFlag "-sia"  
#define kSaveIntermediateArgsFlagLong "-saveintermediateargs"

class EnableSaveIntermediateCmd : public MPxCommand
{
public:
	EnableSaveIntermediateCmd();

	virtual ~EnableSaveIntermediateCmd();
	MStatus doIt(const MArgList& args);
	static void* creator();
	static MSyntax newSyntax();
};

