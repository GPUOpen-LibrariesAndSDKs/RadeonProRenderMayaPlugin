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

