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
#include "IESLightLocatorMesh.h"

#include <memory>

/**
* The RPR IES Light locator contains the
* system attributes and is responsible for
* drawing the viewport representation.
*/
class FireRenderIESLightLocator : public MPxLocatorNode
{
public:
	FireRenderIESLightLocator() {};
	virtual ~FireRenderIESLightLocator() {};

public:
	virtual MStatus compute(const MPlug& plug, MDataBlock& data);

	virtual void draw(
		M3dView & view,
		const MDagPath & path,
		M3dView::DisplayStyle style,
		M3dView::DisplayStatus status);

	virtual bool isBounded() const;

	virtual MBoundingBox boundingBox() const;

	static void* creator();

	static MStatus initialize();

	MString GetFilename() const;
	float GetAreaWidth() const;
	bool GetDisplay() const;
	float GetXRotation() const;
	float GetYRotation() const;
	float GetZRotation() const;
	float GetAxisRotation(unsigned axis) const;

public:
	static MTypeId	id;
	static MString	drawDbClassification;
	static MString	drawRegistrantId;
	static MObject	aFilePath;
	static MObject	aAreaWidth;
	static MObject	aRotations[3];
	static MObject	aIntensity;
	static MObject	aDisplay;
	static MObject	aMeshRepresentationUpdated;

private:
	void UpdateMesh(bool forced) const;

	/** The mesh used to represent the mesh in the Maya viewports. */
	std::unique_ptr<IESLightLegacyLocatorMesh> m_mesh;
};

/** The override is required for drawing to Maya's Viewport 2.0. */
class FireRenderIESLightLocatorOverride : public MHWRender::MPxGeometryOverride
{

public:
	static MHWRender::MPxGeometryOverride* Creator(const MObject& obj)
	{
		return new FireRenderIESLightLocatorOverride(obj);
	}

	virtual ~FireRenderIESLightLocatorOverride();

	virtual MHWRender::DrawAPI supportedDrawAPIs() const;

	virtual bool hasUIDrawables() const { return false; }

	virtual void updateDG();
	virtual bool isIndexingDirty(const MHWRender::MRenderItem &item) { return m_changed; }
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

	MString GetFilename() const;
	void SetFilename(const MString& filePath);

	float GetAreaWidth() const;
	bool GetDisplay() const;
	float GetXRotation() const;
	float GetYRotation() const;
	float GetZRotation() const;
	float GetAxisRotation(unsigned axis) const;

private:

	FireRenderIESLightLocatorOverride(const MObject& obj);

	MObject m_obj;
	IESLightLocatorMesh m_mesh;

	bool m_changed;
};
