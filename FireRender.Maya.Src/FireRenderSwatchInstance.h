#pragma once

#include <maya/MSpinLock.h>

#include "RenderCacheWarningDialog.h"
#include "FireRenderContext.h"

class FireRenderMaterialSwatchRender;

class FireRenderSwatchInstance
{
public:
	static FireRenderSwatchInstance& instance();
	static bool IsCleaned();
	void cleanScene();

	void enqueSwatch(FireRenderMaterialSwatchRender* swatch);
	FireRenderMaterialSwatchRender* dequeSwatch();
	void removeFromQueue(FireRenderMaterialSwatchRender* swatch);

	FireRenderContext& getContext() { return context; }

private:
	FireRenderSwatchInstance();

	void initScene();

	~FireRenderSwatchInstance();

	void ProcessInRenderThread();

	FireRenderSwatchInstance(const FireRenderSwatchInstance&);

	FireRenderSwatchInstance& operator=(const FireRenderSwatchInstance&);

private:
	bool sceneIsCleaned;
	MSpinLock mutex;

	FireRenderContext context;
	std::atomic<bool> backgroundRendererBusy;

	std::list<FireRenderMaterialSwatchRender*> queueToProcess;

	RenderCacheWarningDialog rcWarningDialog;
	bool m_warningDialogOpen;

	static FireRenderSwatchInstance m_instance;
};

