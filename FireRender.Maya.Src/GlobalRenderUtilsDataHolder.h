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
#include <set>
#include <vector>

class GlobalRenderUtilsDataHolder
{
private:
	GlobalRenderUtilsDataHolder(void) : m_isSaveIntermediateEnabled(false) {}
	~GlobalRenderUtilsDataHolder(void) {}

private:
	bool m_isSaveIntermediateEnabled;
	std::string m_IntermediateImagesFolderPath;
	std::set<int> framesToSave;
	long m_startTime;

public:
	static GlobalRenderUtilsDataHolder* GetGlobalRenderUtilsDataHolder(void);

	GlobalRenderUtilsDataHolder(const GlobalRenderUtilsDataHolder&) = delete;
	GlobalRenderUtilsDataHolder& operator=(const GlobalRenderUtilsDataHolder&) = delete;

	bool IsSavingIntermediateEnabled(void) const { return m_isSaveIntermediateEnabled; }
	bool ShouldSaveFrame(int frameIdx) const;
	std::string FolderPath(void) const { return m_IntermediateImagesFolderPath; }
	long GetStartTime(void) const { return m_startTime; }

	void SetEnabledSaveIntermediateImages(bool enable = true) { m_isSaveIntermediateEnabled = enable; }
	void SetIntermediateImagesFolder(const std::string& folderPath) { m_IntermediateImagesFolderPath = folderPath; }
	void SetIterationsToSave(std::vector<std::string>& indices);
	void UpdateStartTime(void);
};

