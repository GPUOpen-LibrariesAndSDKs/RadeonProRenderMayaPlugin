// Formatting function from this cpp file is heavily based on RenderStamp functionality from RPR 3ds Max plugin.
// It is worth keeping both implementation in sync. Relevant functions:
// - GetFriendlyUsedGPUName()
// - FormatRenderStamp() ~ ProductionRenderCore::RenderStamp in 3ds Max

#include "RenderStamp.h"

#include <ctime>
#include <time.h>
#include <string>
#include <fstream>

#ifdef WIN32
//
#elif defined(OSMac_)
#include <unistd.h>
#include <sys/sysctl.h>
#elif defined(__linux__)
#include <sys/utsname.h>
#endif

#include <maya/MRenderView.h> // for RV_PIXEL declaration
#include <maya/MIntArray.h>
#include <maya/MGlobal.h>

#include "common.h"
#include "Logger.h"
#include "FireRenderUtils.h"
#include "FireRenderContext.h" // used for scene stats
#include "FireRenderThread.h"
// We are using bitmap fonts to render text. This is the easiest cross-platform way of drawing texts.
#include "RenderStampFont.h"

namespace FireMaya
{
	//-------------------------------------------------------------------------------------------------
	// System information functions
	//------------------------------------------------------------------------------------ 	-------------

	std::string RenderStamp::GetCPUNameString()
	{
		if (m_cpuName.empty())
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

			m_cpuName = buffer;
		}

		return m_cpuName;
	}


	std::string RenderStamp::GetFriendlyUsedGPUName()
	{
		if (m_friendlyUsedGPUName.empty())
		{
			m_friendlyUsedGPUName = FireMaya::FireRenderThread::RunOnMainThread<std::string>([]
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

		return m_friendlyUsedGPUName;
	}

	std::string RenderStamp::GetComputerNameString()
	{
		if (m_computerName.empty())
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

			m_computerName = buffer;
		}

		return m_computerName;
	}

	enum
	{
		RPR_RENDERDEVICE_CPUONLY = 1,
		RPR_RENDERDEVICE_GPUONLY = 2,
		RPR_RENDERDEVICE_CPUGPU = 3,
	};

	int RenderStamp::GetRenderDevice()
	{
		if (m_renderDevice == 0)
		{
			// Rely on logic from GetContextDeviceFlags()
			int createFlags = FireMaya::Options::GetContextDeviceFlags();
			if (createFlags == RPR_CREATION_FLAGS_ENABLE_CPU)
				m_renderDevice = RPR_RENDERDEVICE_CPUONLY;
			else if (createFlags & RPR_CREATION_FLAGS_ENABLE_CPU)
				m_renderDevice = RPR_RENDERDEVICE_CPUGPU;
			else
				m_renderDevice = RPR_RENDERDEVICE_GPUONLY;
		}

		return m_renderDevice;
	}

	//-------------------------------------------------------------------------------------------------
	// Render stamp string formatting function
	//-------------------------------------------------------------------------------------------------

	// Please update help text when adding new tokens here. If could be found in
	// scripts/createFireRenderGlobalsTab.mel, showRenderStampHelp().

	std::string RenderStamp::FormatRenderStamp(FireRenderContext& context, const char* format)
	{
		if (!format || !format[0])
			return "";		// empty string

		const char *str2 = format;

		// parse string
		std::string str;
		str.reserve(256);

		int renderDevice = GetRenderDevice();

		while (char c = *str2++)
		{
			if (c != '%')
			{
				str += c;
				continue;
			}
			// here we have escape sequence
			c = *str2++;
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
				c = *str2++;
				switch (c)
				{
				case 't': // %pt - total elapsed time
				{
					char buffer[32];
					unsigned int secs = (clock() - context.m_startTime) / CLOCKS_PER_SEC;
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
				c = *str2++;
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

	//-------------------------------------------------------------------------------------------------
	// Text rendering functions
	//-------------------------------------------------------------------------------------------------

	static float lerp(float A, float B, float Alpha)
	{
		return A * (1 - Alpha) + B * Alpha;
	}

	// Use structure to avoid sending lots of common data to BlitBitmap
	struct BlitData
	{
		RV_PIXEL*	dstBitmap;
		int			dstWidth;
		int			dstHeight;
		bool		flip;
		const unsigned char* srcBitmap;
		int			srcWidth;
		int			srcHeight;
	};

	void BlitBitmap(const BlitData& data, int dstX, int dstY, int srcX, int srcY, int w, int h, float alpha = 1.0f)
	{
		// clamp data
		if (dstX >= data.dstWidth || dstY >= data.dstHeight)
			return;
		if (dstX + w >= data.dstWidth)
			w = data.dstWidth - dstX;
		if (dstY + h >= data.dstHeight)
			h = data.dstHeight - dstY;

		// compute loop parameters
		int stride, pos;
		if (!data.flip)
		{
			stride = data.dstWidth - w;
			pos = dstY * data.dstWidth + dstX;
		}
		else
		{
			stride = -(data.dstWidth + w);
			pos = (data.dstHeight - dstY - 1) * data.dstWidth + dstX;
		}

		// blend source bitmap to destination bitmap
		const unsigned char* srcPic = &data.srcBitmap[data.srcWidth * srcY + srcX];
		for (int y = 0; y < h; y++)
		{
			for (int x = 0; x < w; x++)
			{
				unsigned char c = *srcPic;
				float v = c / 255.0f;
				RV_PIXEL* p = data.dstBitmap + pos;
				if (alpha == 1.0f)
				{
					p->r = p->g = p->b = v;
				}
				else
				{
					p->r = lerp(p->r, v, alpha);
					p->g = lerp(p->g, v, alpha);
					p->b = lerp(p->b, v, alpha);
				}
				pos++;
				srcPic++;
			}
			pos += stride;
			srcPic += data.srcWidth - w;
		}
	}

	static void GetTextExtent(const char* text, int& width, int& height)
	{
		height = DlgFont::CHAR_HEIGHT;
		width = 0;
		while (char c = *text++)
		{
			if (c < DlgFont::FONT_FIRST_CHAR || c >= 128)
				c = '?';
			const unsigned char* pos = DlgFont::CHAR_LOCATIONS + (c - DlgFont::FONT_FIRST_CHAR) * 3;
			width += pos[2];
		}
	}

	static void GetCharRect(char c, int& x, int& y, int& w, int& h)
	{
		if (c < DlgFont::FONT_FIRST_CHAR || c >= 128)
			c = '?';
		const unsigned char* pos = DlgFont::CHAR_LOCATIONS + (c - DlgFont::FONT_FIRST_CHAR) * 3;
		x = pos[0];
		y = pos[1];
		w = pos[2];
		h = DlgFont::CHAR_HEIGHT;
	}

	void RenderStamp::AddRenderStamp(FireRenderContext& context, RV_PIXEL* pixels, int width, int height, bool flip, const char* format)
	{
		if (!format || !format[0])
			return;

		std::string text = FormatRenderStamp(context, format);

		// prepare common blit data
		BlitData data;
		data.dstBitmap = pixels;
		data.dstWidth = width;
		data.dstHeight = height;
		data.flip = flip;
		data.srcBitmap = DlgFont::TEX_DATA;
		data.srcWidth = DlgFont::TEX_WIDTH;
		data.srcHeight = DlgFont::TEX_HEIGHT;

		// compute location of stamp
		int stampWidth, stampHeight;
		const char* pText = text.c_str();
		GetTextExtent(pText, stampWidth, stampHeight);
		int stampX = width - stampWidth;
		if (stampX < 0) stampX = 0;
		int stampY = height - stampHeight;
		if (stampY < 0) stampY = 0;

		// draw characters
		while (char c = *pText++)
		{
			int x, y, w, h;
			GetCharRect(c, x, y, w, h);
			BlitBitmap(data, stampX, stampY, x, y, w, h, 0.5f);
			stampX += w;
			if (stampX >= width) break; // text is too long
		}
	}

}
