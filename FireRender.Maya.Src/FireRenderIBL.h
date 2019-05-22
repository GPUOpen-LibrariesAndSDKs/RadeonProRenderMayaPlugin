#pragma once
//
// Copyright (C) AMD
//
// File: FireRenderGlobals.h
//
// FireRender IBL node
//
// Created by Alan Stanzione.
//

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
#include <maya/MNodeMessage.h>
#include <maya/MMessage.h>

#include <memory>

class FireRenderIBLOverride;

class FireRenderIBL : public MPxLocatorNode
{
public:

	FireRenderIBL();

	virtual ~FireRenderIBL();

	virtual MStatus compute(
		const MPlug& plug,
		MDataBlock& data);

	virtual void draw(
		M3dView & view,
		const MDagPath & path,
		M3dView::DisplayStyle style,
		M3dView::DisplayStatus status);

	virtual bool isBounded() const;
	virtual MBoundingBox boundingBox() const;
	static  void * creator();
	static  MStatus initialize();

	void postConstructor() override;

public:
	static MObject	aFilePath;
	static MObject	aIntensity;
	static MObject	aDisplay;
	static MObject	aPortal;
	static MObject	aFlipIBL;
	static MObject  IBLSelectingPortalMesh;
	static MObject aPortalHolder;

public:
	static  MTypeId id;
	static  MString drawDbClassification;
	static  MString drawRegistrantId;
	MCallbackId m_attributeChangedCallback;
	MCallbackId m_selectionChangedCallback;

private:
	static void onAttributeChanged(MNodeMessage::AttributeMessage msg, MPlug &plug, MPlug &otherPlug, void *clientData);
	static void onSelectionChanged(void *clientData);

	void SubscribeSelectionChangedEvent(bool subscribe = true);
	void AddSelectedMeshToPortalList(void);

private:
	MString mTexturePath;

	MHWRender::MTexture* mTexture;

	std::unique_ptr<Shader> mShader;

	friend class FireRenderIBLOverride;
};


//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// Viewport 2.0 override implementation
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
class FireRenderIBLOverride : public MHWRender::MPxGeometryOverride
{
public:
	static MHWRender::MPxGeometryOverride* Creator(const MObject& obj)
	{
		return new FireRenderIBLOverride(obj);
	}

	virtual ~FireRenderIBLOverride();

	virtual MHWRender::DrawAPI supportedDrawAPIs() const;

	virtual bool hasUIDrawables() const { return false; }

	virtual void updateDG();
	virtual bool isIndexingDirty(const MHWRender::MRenderItem &item) { return false; }
	virtual bool isStreamDirty(const MHWRender::MVertexBufferDescriptor &desc) { return mChanged; }
	virtual void updateRenderItems(
		const MDagPath &path,
		MHWRender::MRenderItemList& list);
	virtual void populateGeometry(
		const MHWRender::MGeometryRequirements &requirements,
		const MHWRender::MRenderItemList &renderItems,
		MHWRender::MGeometry &data);
	virtual void cleanUp() {};

#ifndef MAYA2015
	virtual bool refineSelectionPath(
		const MHWRender::MSelectionInfo &selectInfo,
		const MHWRender::MRenderItem &hitItem,
		MDagPath &path,
		MObject &components,
		MSelectionMask &objectMask);

	virtual void updateSelectionGranularity(
		const MDagPath& path,
		MHWRender::MSelectionContext& selectionContext);
#endif
private:

	FireRenderIBLOverride(const MObject& obj);

private:
	MObject mLocatorNode;
	MString mFilePath;
	float mIntensity;
	bool mDisplay;
	bool mChanged;
	bool mFileExists;
	bool mFilePathChanged;
};
