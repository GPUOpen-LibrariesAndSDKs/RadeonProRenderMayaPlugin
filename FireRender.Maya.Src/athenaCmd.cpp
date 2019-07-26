#include "athenaCmd.h"
#ifdef WIN32
	#include "Athena/athenaWrap.h"
#endif

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

#ifdef WIN32
	AthenaWrapper::GetAthenaWrapper()->SetEnabled(enableAthena);
#endif

	return MS::kSuccess;
}

