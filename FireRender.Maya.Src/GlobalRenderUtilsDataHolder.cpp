
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

