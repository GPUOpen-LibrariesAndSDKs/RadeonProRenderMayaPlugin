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
#include "RenderStampUtils.h"

#include "common.h"
#include "Context/FireRenderContext.h" // used for scene stats
#include "FireRenderThread.h"

#include "FireRenderUtils.h"

#include <Maya/MAnimControl.h>

#if defined(OSMac_)
#include <sys/sysctl.h>
#endif

namespace RenderStampUtils
{
	//-------------------------------------------------------------------------------------------------
	// System information functions
	//------------------------------------------------------------------------------------ 	-------------

	std::string GetCPUNameString()
	{
		if (g_cpuName.empty())
		{
			char buffer[256]{ 0 };

			strcpy(buffer, "Unknown CPU");

#ifdef WIN32
			HKEY hKey = NULL;
			UINT uiRetVal = RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0"), 0, KEY_READ, &hKey);
			if (uiRetVal == ERROR_SUCCESS)
			{
				DWORD valueSize = 256;
				uiRetVal = RegQueryValueExA(hKey, TEXT("ProcessorNameString"), NULL, NULL, (LPBYTE)buffer, &valueSize);
				RegCloseKey(hKey);
			}
#elif defined(OSMac_)
			size_t bufLen = 256;
			sysctlbyname("machdep.cpu.brand_string", &buffer, &bufLen, NULL, 0);
#elif defined(__linux__)
			std::string line;
			std::ifstream finfo("/proc/cpuinfo");
			while (std::getline(finfo, line))
			{
				std::stringstream str(line);
				std::string itype;
				std::string info;
				if (std::getline(str, itype, ':') && std::getline(str, info) && itype.substr(0, 10) == "model name")
				{
					strcpy(buffer, info.c_str());
					break;
				}
			}
#endif

			g_cpuName = buffer;
		}

		return g_cpuName;
	}

	std::string GetFriendlyUsedGPUName()
	{
		if (g_friendlyUsedGPUName.empty())
		{
			g_friendlyUsedGPUName = FireMaya::FireRenderThread::RunOnMainThread<std::string>([]
				{
					MIntArray devicesUsing;
					MGlobal::executeCommand("optionVar -q RPR_DevicesSelected", devicesUsing);
					auto allDevices = HardwareResources::GetAllDevices();
					size_t numDevices = std::min<size_t>(devicesUsing.length(), allDevices.size());

					int numGPUs = 0;
					std::string gpuName;
					int numIdenticalGpus = 1;
					int numOtherGpus = 0;

					for (int i = 0; i < numDevices; i++)
					{
						const HardwareResources::Device &gpuInfo = allDevices[i];
						if (devicesUsing[i])
						{
							if (numGPUs == 0)
							{
								gpuName = gpuInfo.name; // remember 1st GPU name
							}
							else if (gpuInfo.name == gpuName)
							{
								numIdenticalGpus++; // more than 1 GPUs, but with identical name
							}
							else
							{
								numOtherGpus++; // different GPU used
							}
							numGPUs++;
						}
					}

					// compose string
					std::string str;
					if (!numGPUs)
					{
						str += "not used";
					}
					else
					{
						str += gpuName;
						if (numIdenticalGpus > 1)
						{
							char buffer[32];
							sprintf(buffer, " x %d", numIdenticalGpus);
							str += buffer;
						}
						if (numOtherGpus)
						{
							char buffer[32];
							sprintf(buffer, " + %d other", numOtherGpus);
							str += buffer;
						}
					}

					return str;
				});
		}

		return g_friendlyUsedGPUName;
	}

	std::string GetComputerNameString()
	{
		if (g_computerName.empty())
		{
			char buffer[256]{ 0 };

#ifdef WIN32
			DWORD size = 256;
			GetComputerName(buffer, &size);
#elif defined(OSMac_)
			gethostname(buffer, 256);
#elif defined(__linux__)
			struct utsname name;
			const char* sysName = name.nodename;
			if (uname(&name))
			{
				sysName = "Linux Computer";
			}
			strncpy(buffer, sysName, 256);

#endif

			g_computerName = buffer;
		}

		return g_computerName;
	}

	int GetRenderDevice()
	{
		if (g_renderDevice == 0)
		{
			// Rely on logic from GetContextDeviceFlags()
			int createFlags = FireMaya::Options::GetContextDeviceFlags();
			if (createFlags == RPR_CREATION_FLAGS_ENABLE_CPU)
				g_renderDevice = RPR_RENDERDEVICE_CPUONLY;
			else if (createFlags & RPR_CREATION_FLAGS_ENABLE_CPU)
				g_renderDevice = RPR_RENDERDEVICE_CPUGPU;
			else
				g_renderDevice = RPR_RENDERDEVICE_GPUONLY;
		}

		return g_renderDevice;
	}

	//-------------------------------------------------------------------------------------------------
	// Render stamp string formatting function
	//-------------------------------------------------------------------------------------------------

	std::string FormatRenderStamp(FireRenderContext& context, const char* format)
	{
		if (!format || !format[0])
			return "";		// empty string

		const char *currentFormatString = format;

		// parse string
		std::string str;
		str.reserve(256);

		int renderDevice = GetRenderDevice();

		while (char c = *currentFormatString++)
		{
			if (c != '%')
			{
				str += c;
				continue;
			}
			// here we have escape sequence
			c = *currentFormatString++;
			if (!c)
			{
				str += L'%'; // this was a last character in string
				break;
			}

			static const int uninitNumericValue = 0xDDAABBAA;
			int numericValue = uninitNumericValue;
			switch (c)
			{
			case '%': // %% - add single % character
				str += c;
				break;
			case 'p': // performance
			{
				c = *currentFormatString++;
				switch (c)
				{
				case 't': // %pt - total elapsed time
				{
					char buffer[32];
					unsigned int secs = (TimeDiffChrono<std::chrono::seconds>(GetCurrentChronoTime(), context.m_renderStartTime));
					int hrs = secs / (60 * 60);
					secs = secs % (60 * 60);
					int mins = secs / 60;
					secs = secs % 60;
					sprintf(buffer, "%d:%02d:%02u", hrs, mins, secs);
					str += buffer;
				}
				break;
				case 'p': // %pp - passes
					numericValue = context.m_currentIteration;
					break;
				}
			}
			break;
			case 's': // scene information
			{
				c = *currentFormatString++;
				switch (c)
				{
				case 'l': // %sl - number of light primitives
					numericValue = context.GetScene().LightObjectCount();
					break;
				case 'o': // %so - number of objects
					numericValue = context.GetScene().ShapeObjectCount();
					break;
				}
			}
			break;
			case 'c': // CPU name
				str += GetCPUNameString();
				break;
			case 'g': // GPU name
				str += GetFriendlyUsedGPUName();
				break;
			case 'r': // rendering mode
			{
				if (renderDevice == RPR_RENDERDEVICE_CPUONLY)
					str += "CPU";
				else if (renderDevice == RPR_RENDERDEVICE_GPUONLY)
					str += "GPU";
				else
					str += "CPU/GPU";
			}
			break;
			case 'h': // used hardware
			{
				if (renderDevice == RPR_RENDERDEVICE_CPUONLY)
					str += GetCPUNameString();
				else if (renderDevice == RPR_RENDERDEVICE_GPUONLY)
					str += GetFriendlyUsedGPUName();
				else
					str += std::string(GetCPUNameString()) + " / " + GetFriendlyUsedGPUName();
			}
			break;
			case 'i': // computer name
			{
				str += GetComputerNameString();
			}
			break;
			case 'd': // current date
			{
				char buffer[256];
				time_t itime;
				time(&itime);
				auto timeinfo = localtime(&itime);
				strftime(buffer, sizeof(buffer) / sizeof(buffer[0]), "%c", timeinfo);
				str += buffer;
			}
			break;
			case 'b': // build number
			{
				str += PLUGIN_VERSION;
			}
			break;
			case 'f':
			{
				int frame = (int) MAnimControl::currentTime().value();
				str += std::to_string(frame);
			}
			break;
			default:
				// wrong escape sequence, add character
				if (c)
				{
					str += L'%';
					str += c;
				}
			}

			if (!c) break; // could happen when string ends with multi-character escape sequence, like immediately after "%p" etc

			if (numericValue != uninitNumericValue)
			{
				// the value was represented as simple number, add it here
				char buffer[32];
				sprintf(buffer, "%d", numericValue);
				str += buffer;
			}
		}

		return str;
	}

	void ClearCache()
	{
		g_renderDevice = 0;

		g_friendlyUsedGPUName.clear();
		g_cpuName.clear();
		g_computerName.clear();
	}
}
