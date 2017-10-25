#pragma once
//
// Copyright (C) AMD
//
// File: FireRenderSwatchRender.h
//
// FireRender swatch render
//
// Created by Alan Stanzione.
//

#include <maya/MSwatchRenderBase.h>
#include <maya/MHWShaderSwatchGenerator.h>
#include <maya/MSpinLock.h>

#include "FireRenderIBL.h"
#include "FireRenderContext.h"
#include "frWrap.h"
#include "RenderCacheWarningDialog.h"

class FireRenderSwatchInstance
{
public:

	static FireRenderSwatchInstance& instance()
	{
		static FireRenderSwatchInstance data;
		if (data.sceneIsCleaned) {
			data.initScene();
		}
		return data;
	}

	void cleanScene() {
		if (!sceneIsCleaned) {
			context.cleanScene();
			sceneIsCleaned = true;
		}
	}

private:

	FireRenderSwatchInstance()
	{
		initScene();
	}

	void initScene() {
		context.setCallbackCreationDisabled(true);
		context.initSwatchScene();
		sceneIsCleaned = false;
	}

	~FireRenderSwatchInstance()
	{
		cleanScene();
	}


	FireRenderSwatchInstance(const FireRenderSwatchInstance&);

	FireRenderSwatchInstance& operator=(const FireRenderSwatchInstance&);

public:

	FireRenderContext context;

	MSpinLock mutex;

	bool sceneIsCleaned;
};

// FireRender Maya swatch implementation
// This class implement the swatch render for FireRender materials
class FireRenderMaterialSwatchRender : public MSwatchRenderBase
{
public:

	// Constructor
	FireRenderMaterialSwatchRender(MObject obj, MObject renderObj, int res);

	// Destructor
	virtual ~FireRenderMaterialSwatchRender();

	// Do iteration
	virtual bool doIteration();
	virtual bool renderParallel();
	virtual void cancelParallelRendering();

	// Creator function
	static MSwatchRenderBase* creator(MObject dependNode, MObject renderNode, int imageResolution);

private:
	RenderCacheWarningDialog rcWarningDialog;
	bool m_warningDialogOpen;
	bool m_runningAsyncRender;
	bool m_finnishedAsyncRender;
	bool m_cancelAsyncRender;
};


