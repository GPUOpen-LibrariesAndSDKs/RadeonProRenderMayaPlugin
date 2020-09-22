#pragma once

#include <atomic>
#include <thread>
#include <memory>

class FireRenderContext;

class NorthStarRenderingHelper
{
public:
	NorthStarRenderingHelper();
	~NorthStarRenderingHelper();

	void SetData(FireRenderContext* pContext, std::function<void(void)> readBufferAndUpdateCallback);
	void Start();
	void Stop();

private:
	std::atomic<bool> m_DataReady;
	std::atomic<bool> m_UpdateThreadRunning;
	std::unique_ptr<std::thread> m_UpdateThreadPtr;

	void UpdateThreadFunc();
	friend void ContextRenderUpdateCallback(float progress, void* pData);

	void OnContextRenderUpdateCallback(float progress);

	FireRenderContext* m_pContext;

	std::function<void(void)> m_readBufferAndUpdateCallback;
};

