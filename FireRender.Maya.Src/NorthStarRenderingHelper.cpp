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
}

void NorthStarRenderingHelper::Start()
{
	if (m_pContext == nullptr)
	{
		return;
	}

	if (m_UpdateThreadRunning)
	{
		Stop();
	}

	m_UpdateThreadRunning = true;
	m_UpdateThreadPtr = std::make_unique<std::thread>(&NorthStarRenderingHelper::UpdateThreadFunc, this);
}

void NorthStarRenderingHelper::Stop()
{
	if (!m_UpdateThreadRunning)
	{
		return;
	}

	m_UpdateThreadRunning = false;
	m_UpdateThreadPtr->join();
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
	/*if (m_contextPtr->GetSamplesPerUpdate() == 1)
	{
		return;
	}*/

	if (m_pContext->isDirty())
	{
		// ABORT RENDER !!!
		int res = rprContextAbortRender(m_pContext->context());
		return;
	}

	m_DataReady = true;
}

void NorthStarRenderingHelper::UpdateThreadFunc()
{
	while (m_UpdateThreadRunning)
	{
		while (!m_DataReady && m_UpdateThreadRunning)
		{
			std::this_thread::sleep_for(1ms);
		}

		if (!m_UpdateThreadRunning)
		{
			break;
		}

		m_readBufferAndUpdateCallback();
		
		m_DataReady = false;
	}
}
