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

#include <maya/MSwatchRenderBase.h>
#include <maya/MHWShaderSwatchGenerator.h>

#include "FireRenderIBL.h"
#include "Context/FireRenderContext.h"
#include "frWrap.h"

#include <mutex>              // std::mutex, std::unique_lock
#include <condition_variable> // std::condition_variable


class FireRenderSwatchInstance;

// FireRender Maya swatch implementation
// This class implement the swatch render for FireRender materials
class FireRenderMaterialSwatchRender : public MSwatchRenderBase
{
public:
	static const unsigned int MaterialSwatchPreviewTextureSize = 64;

	// Constructor
	FireRenderMaterialSwatchRender(MObject obj, MObject renderObj, int res);

	// Destructor
	virtual ~FireRenderMaterialSwatchRender();

	// Do iteration
	virtual bool doIteration();
	virtual bool renderParallel();
	virtual void cancelParallelRendering();

	FireRenderSwatchInstance& getSwatchInstance();

	void processFromBackgroundThread();

	void setAsyncRunning(bool val) { m_runningAsyncRender = val; }

	// Creator function
	static MSwatchRenderBase* creator(MObject dependNode, MObject renderNode, int imageResolution);

private:
	bool doIterationForNonFRNode();
	bool finalizeRendering();

	bool IsFRNode() const;
	bool setupFRNode();

private:
	std::atomic<bool> m_runningAsyncRender;
	std::atomic<bool> m_finishedAsyncRender;
	std::atomic<bool> m_cancelAsyncRender;

	frw::Shader m_shader;
	frw::Shader m_volumeShader;

	int m_resolution;

	// for cancelation synchronization
	std::mutex m_cancellationMutex;
	std::condition_variable m_cancellationCondVar;
};


