#include "PluginContext.h"

#include <intrin.h>

PluginContext& PluginContext::instance()
{
	static PluginContext pluginContext;

	return pluginContext;
}

PluginContext::PluginContext()
{
	mHasSSE41 = CheckSSE41();
}

bool PluginContext::HasSSE41() const
{
	return mHasSSE41;
}

bool PluginContext::CheckSSE41()
{
	enum
	{
		SSE_FLAG   = (1 << 25),
		SSE2_FLAG  = (1 << 26),
		SSE3_FLAG  = (1 << 0),
		SSE41_FLAG = (1 << 19),
		SSE42_FLAG = (1 << 20),
	};

	bool hasSSE41 = false;

	int cpuInfo[4];

	__cpuid(cpuInfo, 0);

	size_t elementsNumber = cpuInfo[0];

	if (elementsNumber > 0)
	{
		__cpuidex(cpuInfo, 1, 0);

		int f_1_ECX = cpuInfo[2];
		int f_1_EDX = cpuInfo[3];

		bool hasSSE = (f_1_EDX & SSE_FLAG) == SSE_FLAG;
		bool hasSSE2 = (f_1_EDX & SSE2_FLAG) == SSE2_FLAG;
		bool hasSSE3 = (f_1_ECX & SSE3_FLAG) == SSE3_FLAG;
		hasSSE41 = (f_1_ECX & SSE41_FLAG) == SSE41_FLAG;
		bool hasSSE42 = (f_1_ECX & SSE42_FLAG) == SSE42_FLAG;
	}

	// another way to check
	try
	{
		__m128i data = _mm_set1_epi32(0xaa55);
		__m128i y = _mm_packus_epi32(data, data);
		hasSSE41 = true;
	}
	catch (...)
	{
		// no SSE 4.1
	}

	return hasSSE41;
}
