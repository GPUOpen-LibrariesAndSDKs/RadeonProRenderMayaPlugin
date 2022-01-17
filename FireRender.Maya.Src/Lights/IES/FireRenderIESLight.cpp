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
#include "FireRenderIESLight.h"

#include <maya/MFnDependencyNode.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnMessageAttribute.h>
#include <maya/MImage.h>
#include <maya/MFileObject.h>
#include <maya/MDagModifier.h>
#include <maya/MFnTransform.h>
#include <maya/MEulerRotation.h>
#include "FireMaya.h"
#include "FireRenderUtils.h"
#if defined(OSMac_)
#include "Translators.h"
#else
#include "Translators/Translators.h"
#endif

MObject FireRenderIESLightLocator::aFilePath;
MObject FireRenderIESLightLocator::aAreaWidth;
MObject	FireRenderIESLightLocator::aIntensity;
MObject	FireRenderIESLightLocator::aDisplay;
MObject FireRenderIESLightLocator::aMeshRepresentationUpdated;
MTypeId FireRenderIESLightLocator::id(FireMaya::TypeId::FireRenderIESLightLocator);
MString FireRenderIESLightLocator::drawDbClassification("drawdb/geometry/light/FireRenderIESLightLocator:drawdb/light/directionalLight:light");
MString FireRenderIESLightLocator::drawDbGeomClassification("drawdb/geometry/light/FireRenderIESLightLocator");
MString FireRenderIESLightLocator::drawRegistrantId("FireRenderIESLightNode");

const MString FireRenderIESLightLocator::GetNodeTypeName(void) const
{
	return "RPRIES";
}

MStatus FireRenderIESLightLocator::compute(const MPlug& plug, MDataBlock& data)
{
	if (plug == aMeshRepresentationUpdated)
	{
		UpdateMesh(false);
		data.setClean(plug);
	}

	return MS::kUnknownParameter;
}

template <class T>
void makeAttribute(T& attr)
{
	CHECK_MSTATUS(attr.setKeyable(true));
	CHECK_MSTATUS(attr.setStorable(true));
	CHECK_MSTATUS(attr.setReadable(true));
	CHECK_MSTATUS(attr.setWritable(true));
}

MStatus FireRenderIESLightLocator::initialize()
{
	MFnNumericAttribute nAttr;
	MFnTypedAttribute tAttr;
	MFnMessageAttribute mAttr;

	aMeshRepresentationUpdated = nAttr.create("meshRepresentationUpdated", "mru", MFnNumericData::kBoolean, 0);
	nAttr.setKeyable(false);
	nAttr.setStorable(false);
	nAttr.setReadable(true);
	nAttr.setWritable(true);
	nAttr.setInternal(true);
	nAttr.setHidden(true);
	addAttribute(aMeshRepresentationUpdated);

	aFilePath = tAttr.create("iesFile", "ies", MFnData::kString);
	tAttr.setStorable(true);
	tAttr.setReadable(true);
	tAttr.setWritable(true);
	tAttr.setUsedAsFilename(true);
	addAttribute(aFilePath);

	aAreaWidth = nAttr.create("areaWidth", "aw", MFnNumericData::kFloat, 1.f);
	makeAttribute(nAttr);
	nAttr.setMin(0);
	nAttr.setMax(100);
	nAttr.setSoftMax(2.0);
	addAttribute(aAreaWidth);

	aIntensity = nAttr.create("intensity", "i", MFnNumericData::kFloat, 1.f);
	makeAttribute(nAttr);
	nAttr.setMin(0);
	nAttr.setMax(1000);
	nAttr.setSoftMax(2.0);
	addAttribute(aIntensity);

	aDisplay = nAttr.create("display", "d", MFnNumericData::kBoolean, 1);
	nAttr.setKeyable(true);
	nAttr.setStorable(true);
	nAttr.setReadable(true);
	nAttr.setWritable(true);
	addAttribute(aDisplay);

	MObject a = nAttr.createColor("color", "fcol");
	makeAttribute(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(1.0f, 1.0f, 1.0f));
	addAttribute(a);

	auto setMeshReprAffects = [&](auto... args)
	{
		(void)std::initializer_list<int>
		{
			(0, attributeAffects(args, aMeshRepresentationUpdated))...
		};
	};

	setMeshReprAffects(aFilePath, aDisplay, aAreaWidth);

	return MS::kSuccess;
}

MString FireRenderIESLightLocator::GetFilename() const
{
	return findPlugTryGetValue(thisMObject(), aFilePath, ""_ms);
}

float FireRenderIESLightLocator::GetAreaWidth() const
{
	return findPlugTryGetValue(thisMObject(), aAreaWidth, 1.f);
}

bool FireRenderIESLightLocator::GetDisplay() const
{
	return findPlugTryGetValue(thisMObject(), aDisplay, true);
}


void FireRenderIESLightLocator::UpdateMesh(bool forced) const
{
	if (m_mesh)
	{
		m_mesh->SetEnabled(GetDisplay());
		m_mesh->SetScale(GetAreaWidth());
		m_mesh->SetFilename(GetFilename(), forced);
	}
}

void FireRenderIESLightLocator::draw(
	M3dView& view,
	const MDagPath& dag,
	M3dView::DisplayStyle style,
	M3dView::DisplayStatus status)
{
	bool forcedUpdate = false;

	// Create the locator mesh if required.
	if (!m_mesh)
	{
		m_mesh = std::make_unique<IESLightLegacyLocatorMesh>();
		forcedUpdate = true;
	}

	// Refresh and draw the mesh.
	UpdateMesh(forcedUpdate);
	m_mesh->Draw(view);
}

bool FireRenderIESLightLocator::isBounded() const
{
	return true;
}

MBoundingBox FireRenderIESLightLocator::boundingBox() const
{
	MObject thisNode = thisMObject();
	MPoint corner1(-1.0, -1.0, -1.0);
	MPoint corner2(1.0, 1.0, 1.0);
	return MBoundingBox(corner1, corner2);
}
void* FireRenderIESLightLocator::creator()
{
	return new FireRenderIESLightLocator();
}

void FireRenderIESLightLocator::postConstructor()
{
	FireRenderLightCommon::postConstructor();

	MStatus status;
	MObject mobj = thisMObject();

	// rename node
	MFnDependencyNode nodeFn(thisMObject());
	nodeFn.setName("RPRIESShape#");

	MFnDagNode dagNode(thisMObject());
	MObject parent = dagNode.parent(0, &status);
	CHECK_MSTATUS(status);

	MFnDependencyNode parentFn(parent);
	parentFn.setName("RPRIES#");
}

// ================================
// Viewport 2.0 override
// ================================

FireRenderIESLightLocatorOverride::FireRenderIESLightLocatorOverride(const MObject& obj) :
	MHWRender::MPxGeometryOverride(obj),
	m_obj(obj),
	m_changed(true)
{
	m_mesh.SetFilename(GetFilename(), true);
}

FireRenderIESLightLocatorOverride::~FireRenderIESLightLocatorOverride() = default;

MHWRender::DrawAPI FireRenderIESLightLocatorOverride::supportedDrawAPIs() const
{
#ifndef MAYA2015
	return (MHWRender::kOpenGL | MHWRender::kDirectX11 | MHWRender::kOpenGLCoreProfile);
#else
	return (MHWRender::kOpenGL | MHWRender::kDirectX11);
#endif
}

void FireRenderIESLightLocatorOverride::updateDG()
{
	bool fileNameChanged = false;
	if (m_mesh.SetFilename(GetFilename(), false, &fileNameChanged))
	{
		m_changed = true;
	}
	else if (fileNameChanged)
	{
		// reset filename if it has been loaded incorrectly
		SetFilename(MString(""));
	}
}

void FireRenderIESLightLocatorOverride::updateRenderItems(const MDagPath& path, MHWRender::MRenderItemList& list)
{
	MHWRender::MRenderItem* mesh = nullptr;
	const char* renderItemName = "locatorMesh";
	int index = list.indexOf(renderItemName);

	if (index < 0)
	{
		mesh = MHWRender::MRenderItem::Create(renderItemName,
			MHWRender::MRenderItem::DecorationItem,
			MHWRender::MGeometry::kLines);
		mesh->setDrawMode(MHWRender::MGeometry::kAll);
		list.append(mesh);
	}
	else
		mesh = list.itemAt(index);

	if (mesh)
	{
		float areaWidth = GetAreaWidth();

		MEulerRotation rotation(M_PI / 2, 0, 0);
		MMatrix matrix = rotation.asMatrix();

		MTransformationMatrix trm;
		double scale[3]{ areaWidth, areaWidth, areaWidth };
		trm.setScale(scale, MSpace::Space::kObject);

		matrix *= trm.asMatrix();
		bool ok = mesh->setMatrix(&matrix);
		assert(ok);

		auto renderer = MHWRender::MRenderer::theRenderer();
		auto shaderManager = renderer->getShaderManager();
		auto shader = shaderManager->getStockShader(MHWRender::MShaderManager::k3dSolidShader);

		if (shader)
		{
			MColor c = MHWRender::MGeometryUtilities::wireframeColor(path);
			float color[] = { c.r, c.g, c.b, 1.0f };
			shader->setParameter("solidColor", color);
			mesh->setShader(shader);
			shaderManager->releaseShader(shader);
		}

		mesh->enable(GetDisplay());
	}
}

void FireRenderIESLightLocatorOverride::populateGeometry(
	const MHWRender::MGeometryRequirements& requirements,
	const MHWRender::MRenderItemList& renderItems,
	MHWRender::MGeometry& data)
{
	m_mesh.populateOverrideGeometry(requirements, renderItems, data);
	m_changed = false;
}

#ifndef MAYA2015

bool FireRenderIESLightLocatorOverride::refineSelectionPath(const MHWRender::MSelectionInfo &  	selectInfo,
	const MHWRender::MRenderItem &  	hitItem,
	MDagPath &  	path,
	MObject &  	components,
	MSelectionMask &  	objectMask
)
{
	return true;
}

void FireRenderIESLightLocatorOverride::updateSelectionGranularity(
	const MDagPath& path,
	MHWRender::MSelectionContext& selectionContext)
{
}

#endif

MString FireRenderIESLightLocatorOverride::GetFilename() const
{
	return findPlugTryGetValue(m_obj, FireRenderIESLightLocator::aFilePath, ""_ms);
}

void FireRenderIESLightLocatorOverride::SetFilename(const MString& filePath)
{
	MPlug plug = MFnDependencyNode(m_obj).findPlug(FireRenderIESLightLocator::aFilePath);

	if (!plug.isNull())
	{
		MDGModifier dgModifier;
		dgModifier.newPlugValueString(plug, filePath);
		dgModifier.doIt();
	}
}

float FireRenderIESLightLocatorOverride::GetAreaWidth() const
{
	return findPlugTryGetValue(m_obj, FireRenderIESLightLocator::aAreaWidth, 1.f);
}

bool FireRenderIESLightLocatorOverride::GetDisplay() const
{
	return findPlugTryGetValue(m_obj, FireRenderIESLightLocator::aDisplay, true);
}
