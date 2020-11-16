#include "NorthStarRenderingHelper.h"

#include "Context/FireRenderContext.h"

#include <chrono>

using namespace std::chrono_literals;

NorthStarRenderingHelper::NorthStarRenderingHelper() : 
	m_UpdateThreadRunning(false)
	, m_DataReady(false)
	, m_pContext(nullptr)
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
	(static_cast<NorthStarRenderingHelper*>(pData))->OnContextRenderUpdateCallback(progress);
}


void NorthStarRenderingHelper::SetData(FireRenderContext* pContext, std::function<void(void)> readBufferAndUpdateCallback)
{
	m_pContext = pContext;
	m_readBufferAndUpdateCallback = readBufferAndUpdateCallback;

	m_pContext->SetRenderUpdateCallback(ContextRenderUpdateCallback, this);
}

void NorthStarRenderingHelper::OnContextRenderUpdateCallback(float progress)
{
	if (m_pContext->isDirty())
	{
		// ABORT RENDER !!!
		m_pContext->AbortRender();
		return;
	}

	std::unique_lock<std::mutex> lck(m_DataReadyMutex);
	m_DataReady = true;
	m_DataReadyConditionalVariable.notify_one();
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

		m_readBufferAndUpdateCallback();
		
		m_DataReady = false;
	}
}
