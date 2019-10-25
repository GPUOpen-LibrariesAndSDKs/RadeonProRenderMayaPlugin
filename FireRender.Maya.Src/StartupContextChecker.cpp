#include "StartupContextChecker.h"

bool StartupContextChecker::m_IsMachineLearningDenoiserSupportedOnCPU = false;
bool StartupContextChecker::m_IsRprSupported = false;
bool StartupContextChecker::m_WasCheckedBeforeUsage = false;

void StartupContextChecker::CheckContexts()
{
	m_WasCheckedBeforeUsage = true;

	//Check rpr context
	rpr_int res;
	FireRenderContext rprContext;
	try
	{
		auto createFlags = FireMaya::Options::GetContextDeviceFlags();
		rprContext.createContextEtc(createFlags, true, false, &res);
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
