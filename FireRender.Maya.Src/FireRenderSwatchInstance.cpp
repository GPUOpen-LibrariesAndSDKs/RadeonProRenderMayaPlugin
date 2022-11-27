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
#include "FireRenderMaterialSwatchRender.h"
#include "FireRenderSwatchInstance.h"

#include "AutoLock.h"
#include "FireRenderThread.h"

FireRenderSwatchInstance FireRenderSwatchInstance::m_instance;

using namespace FireMaya;

FireRenderSwatchInstance::FireRenderSwatchInstance()
{
	sceneIsCleaned = true;
	m_shouldClearContext = false;
	pContext = std::make_unique<NorthStarContext>(); 

	MStatus status;
	callbackId_FireRenderSwatchInstance = MTimerMessage::addTimerCallback(16.0f, FireRenderSwatchInstance::CheckProcessQueue, nullptr, &status);

}

void FireRenderSwatchInstance::initScene()
{
	backgroundRendererBusy = false;
	m_warningDialogOpen = false;

	if (getContext().isFirstIterationAndShadersNOTCached())
	{
		//first iteration and shaders are _NOT_ cached
		rcWarningDialog.show();
		m_warningDialogOpen = true;
	}

	pContext->setCallbackCreationDisabled(true);
	pContext->SetRenderType(RenderType::Thumbnail);
	pContext->initSwatchScene();
	pContext->Freshen();
	sceneIsCleaned = false;
}

FireRenderSwatchInstance::~FireRenderSwatchInstance()
{
	if (!sceneIsCleaned)
	{
		cleanScene();
	}

	if (callbackId_FireRenderSwatchInstance)
	{
		MMessage::removeCallback(callbackId_FireRenderSwatchInstance);
	}
}

FireRenderSwatchInstance& FireRenderSwatchInstance::instance()
{
	if (m_instance.sceneIsCleaned)
	{
		m_instance.initScene();

	}
	return m_instance;
}

bool FireRenderSwatchInstance::IsCleaned()
{
	return m_instance.sceneIsCleaned;
}

void FireRenderSwatchInstance::cleanScene()
{
	if (!sceneIsCleaned)
	{
		pContext->cleanScene();
		sceneIsCleaned = true;
	}
}

void FireRenderSwatchInstance::ProcessInRenderThread()
{
	backgroundRendererBusy = true;

	FireRenderThread::KeepRunning([this]() -> bool
	{
		try
		{
			FireRenderMaterialSwatchRender* item = nullptr;

			while ((item = dequeSwatch()) != nullptr)
			{
				item->processFromBackgroundThread();
			}
		}
		catch (...)
		{
		}

		backgroundRendererBusy = false;

		if (m_warningDialogOpen &&  rcWarningDialog.shown)
		{
			FireRenderThread::RunProcOnMainThread([this]
			{
				rcWarningDialog.close();
			});
		}

		m_shouldClearContext = true;

		return false;
	});
}

void FireRenderSwatchInstance::enqueSwatch(FireRenderMaterialSwatchRender* swatch)
{
	m_shouldClearContext = false; // should not clear context if renderer is running

	{
		RPR::AutoLock<MSpinLock> lock(mutex);
		queueToProcess.push_back(swatch);
	}

	if (!backgroundRendererBusy)
	{
		ProcessInRenderThread();
	}
}

FireRenderMaterialSwatchRender* FireRenderSwatchInstance::dequeSwatch()
{
	RPR::AutoLock<MSpinLock> lock(mutex);

	if (queueToProcess.size() == 0)
	{
		return nullptr;
	}

	FireRenderMaterialSwatchRender* item = queueToProcess.front();
	queueToProcess.pop_front();

	item->setAsyncRunning(true);
	return item;
}

void FireRenderSwatchInstance::removeFromQueue(FireRenderMaterialSwatchRender* swatch)
{
	RPR::AutoLock<MSpinLock> lock(mutex);
	queueToProcess.remove(swatch);
}

void FireRenderSwatchInstance::CheckProcessQueue(float elapsedTime, float lastTime, void* clientData)
{
	RPR::AutoLock<MSpinLock> lock(instance().mutex);

	if (!instance().m_shouldClearContext || (instance().queueToProcess.size() != 0))
	{
		return;
	}

	instance().m_shouldClearContext = false;

	// reset context
	instance().cleanScene();
	instance().pContext.reset();

	instance().pContext = std::make_unique<NorthStarContext>();
	instance().initScene();
}


