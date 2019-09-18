#pragma once
//
// Copyright (C) AMD
//
// File: FireMaterialViewRenderer.h
//
// Material view renderer
//
// Created by Alan Stanzione.
//

#ifndef MAYA2015
#include "frWrap.h"
#include "FireRenderContext.h"
#include "Translators/Translators.h"
#include <maya/MPxRenderer.h>
#include <maya/MGlobal.h>
#include <maya/MString.h>
#include <maya/MThreadAsync.h>
#include <maya/MMutexLock.h>
#include <map>
#include <string>

class FireRenderRenderData
{
public:
	FireRenderRenderData();

	~FireRenderRenderData();

public:

	MMutexLock m_mutex;

	std::map<std::string, FrLight> m_lights;

	std::map<std::string, std::string> m_lightsNames;

	FireRenderContext m_context;

	frw::Shader m_surfaceShader;

	frw::Shader m_volumeShader;

	frw::Shape m_shape;

	frw::Camera m_camera;

	frw::EnvironmentLight m_endLight;

	frw::Image m_envImage;

	rpr_framebuffer m_framebuffer;

	unsigned int m_width;

	unsigned int m_height;

	float *m_pixels;
};

class MaterialViewThreadWorder;

// FireRender Material view implementation
class FireMaterialViewRenderer : public MPxRenderer
{
public:

	// Constructor
	FireMaterialViewRenderer();

	// Destructor
	virtual ~FireMaterialViewRenderer() = default;

	// Start async render
	virtual MStatus startAsync(const JobParams& params);

	// Stop async render
	virtual MStatus stopAsync();

	// Return true if the async render is running
	virtual bool isRunningAsync();

	// Begin scene update
	virtual MStatus beginSceneUpdate();

	// Translate mesh
	virtual MStatus translateMesh(const MUuid& id, const MObject& node);

	// Translate light
	virtual MStatus translateLightSource(const MUuid& id, const MObject& node);

	// Translate camera
	virtual MStatus translateCamera(const MUuid& id, const MObject& node);

	// Translate environment
	virtual MStatus translateEnvironment(const MUuid& id, EnvironmentType type);

	// Translate transform
	virtual MStatus translateTransform(const MUuid& id, const MUuid& childId, const MMatrix& matrix);

	// Translate shader
	virtual MStatus translateShader(const MUuid& id, const MObject& node);

	virtual MStatus setProperty(const MUuid& id, const MString& name, bool value);
	
	virtual MStatus setProperty(const MUuid& id, const MString& name, int value);
	
	virtual MStatus setProperty(const MUuid& id, const MString& name, float value);

	virtual MStatus setProperty(const MUuid& id, const MString& name, const MString& value);

	// Set shader
	virtual MStatus setShader(const MUuid& id, const MUuid& shaderId);

	// Set resolution
	virtual MStatus setResolution(unsigned int width, unsigned int height);


	// End scene update
	virtual MStatus endSceneUpdate();

	// Destroy scene
	virtual MStatus destroyScene();

	// Check if is safe to destroy the renderer
	virtual bool isSafeToUnload();

	// Render thread function
	bool RunOnFireRenderThread();

	// Creator
	static void* creator();

	// Render function
	void Render();

	// Render data
	// Contain the context and other data information needed by the render function
	FireRenderRenderData m_renderData;

	// Flag used to synchronize the render thread and the main thread
	volatile bool m_isThreadRunning;

	// Number of render iterations
	int m_numIteration;

	enum ThreadCommand {
		BEGIN_UPDATE = 0,
		RENDER_IMAGE = 1,
		STOP_THREAD = 2
	};

	// Render commands used by the render thread
	volatile int m_threadCmd;

private:

	// Camera uuid
	MUuid m_cameraId;

	// Mesh uuid
	MUuid m_meshId;

	// Environment uuid
	MUuid m_envId;

};

#endif
