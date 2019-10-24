#include "athenaCmd.h"
#include "Athena/athenaWrap.h"

AthenaEnableCmd::AthenaEnableCmd()
{}

AthenaEnableCmd::~AthenaEnableCmd()
{}

void * AthenaEnableCmd::creator()
{
	return new AthenaEnableCmd;
}

MSyntax AthenaEnableCmd::newSyntax()
{
	MStatus status;
	MSyntax syntax;

	CHECK_MSTATUS(syntax.addFlag(kAthenaEnableFlag, kAthenaEnableFlagLong, MSyntax::kBoolean));

	return syntax;
}

MStatus AthenaEnableCmd::doIt(const MArgList & args)
{
	MStatus status;

	MArgDatabase argData(syntax(), args);

	if (!argData.isFlagSet(kAthenaEnableFlag))
		return MS::kFailure;

	bool enableAthena = false;
	argData.getFlagArgument(kAthenaEnableFlag, 0, enableAthena);

	AthenaWrapper::GetAthenaWrapper()->SetEnabled(enableAthena);

	return MS::kSuccess;
}

