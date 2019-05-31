#pragma once

#include <string>

class FireRenderContext;

namespace RenderStampUtils
{
	static int	g_renderDevice;
	static std::string g_friendlyUsedGPUName;
	static std::string g_cpuName;
	static std::string g_computerName;

	std::string FormatRenderStamp(FireRenderContext& context, const char* format);
	std::string GetCPUNameString();
	std::string GetFriendlyUsedGPUName();
	std::string GetComputerNameString();
	int GetRenderDevice();
};
