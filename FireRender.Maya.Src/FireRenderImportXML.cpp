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
#include "Context/FireRenderContext.h"
#include "FireRenderUtils.h"
#include "common.h"
#include "FireRenderMaterial.h"
#include <iterator>

#include <maya/MItDependencyNodes.h>
#include <maya/MArgList.h>

#include "FileSystemUtils.h"
#include "FireRenderError.h"

#ifdef _WIN32
#include <tchar.h>
#else
#include <wchar.h>
#endif


std::tuple<MStatus, MString> FireRenderXmlImportCmd::GetFilePath(const MArgDatabase& argData)
{
	// back-off
	if (!argData.isFlagSet(kFilePathFlag))
	{
		MGlobal::displayError("File path is missing, use -file flag");
		return std::make_tuple(MS::kFailure, MString());
	}

	MString filePath;
	argData.getFlagArgument(kFilePathFlag, 0, filePath);

	return std::make_tuple(MS::kSuccess, filePath);
}


