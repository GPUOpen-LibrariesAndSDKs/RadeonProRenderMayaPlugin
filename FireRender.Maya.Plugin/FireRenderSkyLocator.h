#pragma once

#include "ShadersManager.h"
#include <maya/MPxLocatorNode.h>
#include <maya/MDrawRegistry.h>
#include <maya/MPxDrawOverride.h>
#include <maya/MPxGeometryOverride.h>
#include <maya/MUserData.h>
#include <maya/MDrawContext.h>
#include <maya/MGlobal.h>
#include <maya/MSelectionList.h>
#include <maya/MViewport2Renderer.h>
#include <maya/MHWGeometryUtilities.h>
#include <maya/MStateManager.h>
#include <maya/MShaderManager.h>
#include <maya/MRenderTargetManager.h>
#include <maya/MPointArray.h>
#include "SkyLocatorMesh.h"

#include <memory>

/**
 * The sky locator contains the Sun / Sky
 * system attributes and is responsible for
 * drawing the viewport representation.
 */
class FireRenderSkyLocator : public MPxLocatorNode
{
public:

	FireRenderSkyLocator() {}

	virtual ~FireRenderSkyLocator() {}

	virtual MStatus compute(const MPlug& plug, MDataBlock& data);

	virtual void draw(M3dView & view, const MDagPath & path,
		M3dView::DisplayStyle style,
		M3dView::DisplayStatus status);

	virtual bool isBounded() const;

	virtual MBoundingBox boundingBox() const;

	static void* creator();

	static MStatus initialize();

public:

	static  MTypeId     id;
	static  MString     drawDbClassification;
	static  MString     drawRegistrantId;

private:

	/** The mesh used to represent the sky locator in the Maya viewports. */
	std::unique_ptr<SkyLocatorMesh> m_mesh;
};

/** The override is required for drawing to Maya's Viewport 2.0. */
class FireRenderSkyLocatorOverride : public MHWRender::MPxGeometryOverride
{

public:
	static MHWRender::MPxGeometryOverride* Creator(const MObject& obj)
	{
		return new FireRenderSkyLocatorOverride(obj);
	}

	virtual ~FireRenderSkyLocatorOverride();

	virtual MHWRender::DrawAPI supportedDrawAPIs() const;

	virtual bool hasUIDrawables() const { return false; }

	virtual void updateDG();
	virtual bool isIndexingDirty(const MHWRender::MRenderItem &item) { return false; }
	virtual bool isStreamDirty(const MHWRender::MVertexBufferDescriptor &desc) { return m_changed; }
	virtual void updateRenderItems(const MDagPath &path, MHWRender::MRenderItemList& list);
	virtual void populateGeometry(const MHWRender::MGeometryRequirements &requirements, const MHWRender::MRenderItemList &renderItems, MHWRender::MGeometry &data);
	virtual void cleanUp() {};

#ifndef MAYA2015
	virtual bool refineSelectionPath(const MHWRender::MSelectionInfo &  	selectInfo,
		const MHWRender::MRenderItem &  	hitItem,
		MDagPath &  	path,
		MObject &  	components,
		MSelectionMask &  	objectMask
		);

	virtual void updateSelectionGranularity(
		const MDagPath& path,
		MHWRender::MSelectionContext& selectionContext);
#endif
private:

	FireRenderSkyLocatorOverride(const MObject& obj);

	SkyLocatorMesh m_mesh;

	bool m_changed;
};
