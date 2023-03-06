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
#pragma once

#include <maya/MSpinLock.h>

#include "RenderCacheWarningDialog.h"
#include "Context/TahoeContext.h"
#include <maya/MTimerMessage.h>

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

	static void resetInstance();

	FireRenderContext& getContext() { return *pContext.get(); }

private:
	FireRenderSwatchInstance();

	void initScene();

	~FireRenderSwatchInstance();

	void ProcessInRenderThread();

	FireRenderSwatchInstance(const FireRenderSwatchInstance&);

	FireRenderSwatchInstance& operator=(const FireRenderSwatchInstance&);

	static void CheckProcessQueue(float elapsedTime, float lastTime, void* clientData);

private:
	bool sceneIsCleaned;
	MSpinLock mutex;

	std::unique_ptr<NorthStarContext> pContext;
	std::atomic<bool> backgroundRendererBusy;
	std::atomic<bool> m_shouldClearContext;

	std::list<FireRenderMaterialSwatchRender*> queueToProcess;

	RenderCacheWarningDialog rcWarningDialog;
	bool m_warningDialogOpen;
	MCallbackId callbackId_FireRenderSwatchInstance;

	static FireRenderSwatchInstance m_instance;
};

