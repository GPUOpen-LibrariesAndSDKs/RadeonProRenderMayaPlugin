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
#include "Context/TahoeContext.h"
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
