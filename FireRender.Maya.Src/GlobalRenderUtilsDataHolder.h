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

