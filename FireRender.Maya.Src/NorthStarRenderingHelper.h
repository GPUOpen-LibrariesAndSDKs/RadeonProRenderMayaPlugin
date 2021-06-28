#pragma once

#include <atomic>
#include <thread>
#include <memory>
#include <condition_variable>
#include <functional>

class FireRenderContext;

class NorthStarRenderingHelper
{
	using RenderingHelperCallback = std::function<void(float)>;

public:
	NorthStarRenderingHelper();
	~NorthStarRenderingHelper();

	void SetData(FireRenderContext* pContext, RenderingHelperCallback readBufferAndUpdateCallback);
	void Start();
	void StopAndJoin();
	void SetStopFlag();

private:
	std::atomic<bool> m_DataReady;
	std::atomic<bool> m_UpdateThreadRunning;
	std::unique_ptr<std::thread> m_UpdateThreadPtr;

	void UpdateThreadFunc();
	friend void ContextRenderUpdateCallback(float progress, void* pData);

	void OnContextRenderUpdateCallback(float progress);

	FireRenderContext* m_pContext;

	RenderingHelperCallback m_readBufferAndUpdateCallback;

	std::mutex m_DataReadyMutex;
	std::condition_variable m_DataReadyConditionalVariable;
	float m_currProgress;
};

