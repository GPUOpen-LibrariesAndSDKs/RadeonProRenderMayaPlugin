#include "NorthStarRenderingHelper.h"

#include "Context/FireRenderContext.h"

#include <chrono>

using namespace std::chrono_literals;

NorthStarRenderingHelper::NorthStarRenderingHelper() : 
	m_UpdateThreadRunning(false)
	, m_DataReady(false)
	, m_pContext(nullptr)
	, m_currProgress(0.0f)
{
}


NorthStarRenderingHelper::~NorthStarRenderingHelper()
{
    m_UpdateThreadPtr.reset();
}

void NorthStarRenderingHelper::Start()
{
	if (m_pContext == nullptr)
	{
		return;
	}

	if (m_UpdateThreadRunning)
	{
		StopAndJoin();
	}

	m_UpdateThreadRunning = true;
	m_UpdateThreadPtr = std::make_unique<std::thread>(&NorthStarRenderingHelper::UpdateThreadFunc, this);
}

void NorthStarRenderingHelper::SetStopFlag()
{
	m_UpdateThreadRunning = false;
}

void NorthStarRenderingHelper::StopAndJoin()
{
	SetStopFlag();
    if (m_UpdateThreadPtr != nullptr)
    {
		{
			std::unique_lock<std::mutex> lck(m_DataReadyMutex);
			m_DataReadyConditionalVariable.notify_one();
		}

        m_UpdateThreadPtr->join();
        m_UpdateThreadPtr.reset();
    }
}

void ContextRenderUpdateCallback(float progress, void* pData)
{
	assert(pData);
	if (pData != nullptr)
		(static_cast<NorthStarRenderingHelper*>(pData))->OnContextRenderUpdateCallback(progress);
}

void ContextSceneSyncFinCallback(float time, void* pData)
{
	assert(pData);
	if (pData != nullptr)
		(static_cast<NorthStarRenderingHelper*>(pData))->OnContextSceneSyncFinCallback(time);
}

void ContextFirstIterationCallback(float time, void* pData)
{
	assert(pData);
	if (pData != nullptr)
		(static_cast<NorthStarRenderingHelper*>(pData))->OnContextFirstIterationCallback(time);
}

void ContextRenderTimeCallback(float time, void* pData)
{
	assert(pData);
	if (pData != nullptr)
		(static_cast<NorthStarRenderingHelper*>(pData))->OnContextRenderTimeCallback(time);
}

void NorthStarRenderingHelper::SetData(FireRenderContext* pContext, RenderingHelperCallback readBufferAndUpdateCallback)
{
	m_pContext = pContext;
	m_readBufferAndUpdateCallback = readBufferAndUpdateCallback;

	m_pContext->SetRenderUpdateCallback(ContextRenderUpdateCallback, this);

	m_pContext->SetSceneSyncFinCallback(ContextSceneSyncFinCallback, this);
	m_pContext->SetFirstIterationCallback(ContextFirstIterationCallback, this);
	m_pContext->SetRenderTimeCallback(ContextRenderTimeCallback, this);
}

void NorthStarRenderingHelper::OnContextRenderUpdateCallback(float progress)
{
	if (m_pContext->isDirty())
	{
		// ABORT RENDER !!!
		m_pContext->AbortRender();
		return;
	}

	m_currProgress = progress;

	std::unique_lock<std::mutex> lck(m_DataReadyMutex);
	m_DataReady = true;
	m_DataReadyConditionalVariable.notify_one();
}

void NorthStarRenderingHelper::OnContextSceneSyncFinCallback(float time)
{
	m_pContext->m_syncTime = time;
}

void NorthStarRenderingHelper::OnContextFirstIterationCallback(float time)
{
	m_pContext->m_firstFrameRenderTime = time;
}

void NorthStarRenderingHelper::OnContextRenderTimeCallback(float time)
{
	m_pContext->m_lastRenderedFrameRenderTime = time;
}

void NorthStarRenderingHelper::UpdateThreadFunc()
{
	while (m_UpdateThreadRunning)
	{
		{
			std::unique_lock<std::mutex> lck(m_DataReadyMutex);
			m_DataReadyConditionalVariable.wait(lck, [this] { return !m_UpdateThreadRunning || m_DataReady; });
		}

		if (!m_UpdateThreadRunning)
		{
			break;
		}

		m_readBufferAndUpdateCallback(m_currProgress);
		
		m_DataReady = false;
	}
}
