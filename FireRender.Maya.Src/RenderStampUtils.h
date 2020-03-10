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
