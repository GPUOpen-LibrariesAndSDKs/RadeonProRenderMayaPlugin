#pragma once
#include <FireRenderContext.h>
#include <ImageFilter/ImageFilter.h>

/** 
	Used to check compatibility with RPR functions.
	CheckContexts() should be called before accesing other methods.
	Assuming that if any GPU has support for RPR context -> it has support for ML denoiser.
*/
class StartupContextChecker
{
	static bool m_IsRprSupported;
	static bool m_IsMachineLearningDenoiserSupportedOnCPU;
	static bool m_WasCheckedBeforeUsage;
public:
	static void CheckContexts();
	static bool IsRprSupported();
	static bool IsMLDenoiserSupportedCPU();
};
