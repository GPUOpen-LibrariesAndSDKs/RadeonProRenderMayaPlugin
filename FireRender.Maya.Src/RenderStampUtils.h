#pragma once

#include <string>

class FireRenderContext;

namespace RenderStampUtils
{
	static int	g_renderDevice;
	static std::string g_friendlyUsedGPUName;
	static std::string g_cpuName;
	static std::string g_computerName;

	// Please update help text when adding new tokens here. If could be found in
	// scripts/createFireRenderGlobalsTab.mel, showRenderStampHelp().
	std::string FormatRenderStamp(FireRenderContext& context, const char* format);

	std::string GetCPUNameString();
	std::string GetFriendlyUsedGPUName();
	std::string GetComputerNameString();
	int GetRenderDevice();

	void ClearCache();

	enum
	{
		RPR_RENDERDEVICE_CPUONLY = 1,
		RPR_RENDERDEVICE_GPUONLY = 2,
		RPR_RENDERDEVICE_CPUGPU = 3,
	};
};
