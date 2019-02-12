#pragma once
//
// Copyright (C) AMD
//

#include <maya/MSwatchRenderBase.h>
#include <maya/MHWShaderSwatchGenerator.h>

#include "FireRenderIBL.h"
#include "FireRenderContext.h"
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


