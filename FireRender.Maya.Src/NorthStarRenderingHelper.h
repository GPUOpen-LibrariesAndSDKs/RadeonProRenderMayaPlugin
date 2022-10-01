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
	bool IsStopped() const { return !m_UpdateThreadRunning && !m_DataReady; }

private:
	std::atomic<bool> m_DataReady;
	std::atomic<bool> m_UpdateThreadRunning;
	std::unique_ptr<std::thread> m_UpdateThreadPtr;

	void UpdateThreadFunc();
	friend void ContextRenderUpdateCallback(float progress, void* pData);
	friend void ContextSceneSyncFinCallback(float time, void* pData); // the time it took to update / compile the scene inside the core rendering engine.
	friend void ContextFirstIterationCallback(float time, void* pData); // the time it took to run the first rendering iteration. This time could he higher than next iteration as first iteration is creating some cache.
	friend void ContextRenderTimeCallback(float time, void* pData); // the time it took to run the actual rendering operation.

	void OnContextRenderUpdateCallback(float progress);
	void OnContextSceneSyncFinCallback(float time); // time in milliseconds
	void OnContextFirstIterationCallback(float time); // time in milliseconds
	void OnContextRenderTimeCallback(float time); // time in milliseconds

	FireRenderContext* m_pContext;

	RenderingHelperCallback m_readBufferAndUpdateCallback;

	std::mutex m_DataReadyMutex;
	std::condition_variable m_DataReadyConditionalVariable;
	float m_currProgress;
};

