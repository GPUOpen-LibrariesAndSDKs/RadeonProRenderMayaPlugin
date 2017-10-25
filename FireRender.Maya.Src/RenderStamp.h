#pragma once

#include <string>
// Forward declarations
class FireRenderContext;
struct RV_PIXEL;

namespace FireMaya
{
	class RenderStamp
	{
	private:
		int	m_renderDevice;
		std::string m_friendlyUsedGPUName;
		std::string m_cpuName;
		std::string m_computerName;

	public:
		void AddRenderStamp(FireRenderContext& context, RV_PIXEL* pixels, int width, int height, bool flip, const char* format);

	private:
		std::string FormatRenderStamp(FireRenderContext& context, const char* format);
		std::string GetCPUNameString();
		std::string GetFriendlyUsedGPUName();
		std::string GetComputerNameString();
		int GetRenderDevice();
	};

}