#pragma once

#include <maya/MPxCommand.h>
#include <maya/MSyntax.h>
#include <maya/MArgDatabase.h>

#define kAthenaEnableFlag "-ae"
#define kAthenaEnableFlagLong "-athenaenabled"

class AthenaEnableCmd : public MPxCommand
{
public:

	AthenaEnableCmd();

	virtual ~AthenaEnableCmd();

	MStatus doIt(const MArgList& args);

	static void* creator();

	static MSyntax  newSyntax();
};

