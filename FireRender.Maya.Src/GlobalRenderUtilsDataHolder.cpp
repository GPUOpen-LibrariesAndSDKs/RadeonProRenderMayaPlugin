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

#include "GlobalRenderUtilsDataHolder.h"
#include <chrono>

GlobalRenderUtilsDataHolder* GlobalRenderUtilsDataHolder::GetGlobalRenderUtilsDataHolder(void)
{
	static GlobalRenderUtilsDataHolder instance;
	return &instance;
}

void GlobalRenderUtilsDataHolder::SetIterationsToSave(std::vector<std::string>& indices)
{
	framesToSave.clear();

	for (auto& token : indices)
	{
		int idx = std::stoi(token);
		framesToSave.insert(idx);
	}
}

bool GlobalRenderUtilsDataHolder::ShouldSaveFrame(int frameIdx) const
{
	auto it = std::find(framesToSave.begin(), framesToSave.end(), frameIdx);
	if (it == framesToSave.end())
		return false;

	return true;
}

void GlobalRenderUtilsDataHolder::UpdateStartTime()
{
	m_startTime = clock();
}

