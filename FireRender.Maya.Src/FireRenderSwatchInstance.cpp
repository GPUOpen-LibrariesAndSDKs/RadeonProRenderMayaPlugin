#include "FireRenderMaterialSwatchRender.h"
#include "FireRenderSwatchInstance.h"

#include "AutoLock.h"
#include "FireRenderThread.h"

FireRenderSwatchInstance FireRenderSwatchInstance::m_instance;

using namespace FireMaya;

FireRenderSwatchInstance::FireRenderSwatchInstance()
{
	sceneIsCleaned = true;
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

	context.setCallbackCreationDisabled(true);
	context.m_RenderType = FireRenderContext::RenderType::Thumbnail;
	context.initSwatchScene();
	context.Freshen();
	sceneIsCleaned = false;
}

FireRenderSwatchInstance::~FireRenderSwatchInstance()
{
	if (!sceneIsCleaned)
	{
		cleanScene();
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
		context.cleanScene();
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

		return false;
	});
}

void FireRenderSwatchInstance::enqueSwatch(FireRenderMaterialSwatchRender* swatch)
{
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
