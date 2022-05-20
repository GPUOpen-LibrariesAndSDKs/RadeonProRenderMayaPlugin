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
#include "StartupContextChecker.h"

bool StartupContextChecker::m_IsMachineLearningDenoiserSupportedOnCPU = false;
bool StartupContextChecker::m_IsRprSupported = false;
bool StartupContextChecker::m_WasCheckedBeforeUsage = false;

void StartupContextChecker::CheckContexts()
{
	m_WasCheckedBeforeUsage = true;

	//Check rpr context
	rpr_int res;
	NorthStarContext rprContext;
	try
	{
		auto createFlags = FireMaya::Options::GetContextDeviceFlags();
		rprContext.createContextEtc(createFlags, true, &res);
	}
	catch (const FireRenderException & e)
	{
		FireRenderError(e.code, e.message, true);
		return;
	}

	if (res != RPR_SUCCESS)
	{
		MString msg;
		if (res == RPR_ERROR_INVALID_API_VERSION)
		{
			msg = "Please remove all previous versions of plugin if any and make a fresh install";
		}
		FireRenderError(res, msg, true);
		return;
	}
	m_IsRprSupported = true;

	//Check rif context
#ifdef WIN32
	try
	{
		auto rifContext = std::make_unique<RifContextCPU>(rprContext.context());
		std::unique_ptr<RifFilterWrapper> mRifFilter;
		MString path;
		MStatus status = MGlobal::executeCommand("getModulePath -moduleName RadeonProRender", path);
		MString mlModelsFolder = path + "/data/models";
		auto filter = std::make_unique<RifFilterMlColorOnly>(rifContext.get(), 512, 512, mlModelsFolder.asChar(), true);
	}
	catch (const std::runtime_error & e)
	{
		MGlobal::displayWarning(e.what());
		return;
	}
#endif

	m_IsMachineLearningDenoiserSupportedOnCPU = true;
}

bool StartupContextChecker::IsRprSupported()
{
	assert(m_WasCheckedBeforeUsage);
	return m_IsRprSupported;
}

bool StartupContextChecker::IsMLDenoiserSupportedCPU()
{
	assert(m_WasCheckedBeforeUsage);
	return m_IsMachineLearningDenoiserSupportedOnCPU;
}
