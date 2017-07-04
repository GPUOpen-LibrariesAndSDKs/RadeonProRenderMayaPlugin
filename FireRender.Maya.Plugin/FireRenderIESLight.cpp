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

using namespace std;


MObject FireRenderIESLightLocator::aFilePath;
MObject	FireRenderIESLightLocator::aIntensity;
MObject	FireRenderIESLightLocator::aDisplay;
MObject	FireRenderIESLightLocator::aPortal;
MTypeId FireRenderIESLightLocator::id(FireMaya::TypeId::FireRenderIESLightLocator);

MString FireRenderIESLightLocator::drawDbClassification("drawdb/geometry/FireRenderIESLightLocator");
MString FireRenderIESLightLocator::drawRegistrantId("FireRenderIESLightNode");

MStatus FireRenderIESLightLocator::compute(const MPlug& plug, MDataBlock& data)
{
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
	MFnEnumAttribute eAttr;
	MFnMessageAttribute mAttr;

	aFilePath = tAttr.create("iesFile", "ies", MFnData::kString);
	tAttr.setStorable(true);
	tAttr.setReadable(true);
	tAttr.setWritable(true);
	tAttr.setUsedAsFilename(true);
	addAttribute(aFilePath);

	aIntensity = nAttr.create("intensity", "i", MFnNumericData::kFloat, 0.1f);
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

	aPortal = mAttr.create("portal", "p");
	addAttribute(aPortal);
	MObject a = nAttr.createColor("color", "fcol");
	makeAttribute(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(1.0f, 1.0f, 1.0f));
	addAttribute(a);

	return MS::kSuccess;
}

void FireRenderIESLightLocator::draw(
	M3dView& view,
	const MDagPath&,
	M3dView::DisplayStyle style,
	M3dView::DisplayStatus status)
{
	// Create the locator mesh if required.
	if (!m_mesh)
		m_mesh = make_unique<IESLightLocatorMesh>(thisMObject());

	// Refresh and draw the mesh.
	m_mesh->refresh();
	m_mesh->glDraw(view);
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

// ================================
// Viewport 2.0 override
// ================================

FireRenderIESLightLocatorOverride::FireRenderIESLightLocatorOverride(const MObject& obj) :
	MHWRender::MPxGeometryOverride(obj),
	m_mesh(obj),
	m_changed(true)
{
}

FireRenderIESLightLocatorOverride::~FireRenderIESLightLocatorOverride()
{
}

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
	if (m_mesh.refresh())
		m_changed = true;
}

void FireRenderIESLightLocatorOverride::updateRenderItems(const MDagPath& path, MHWRender::MRenderItemList& list)
{
	MHWRender::MRenderItem* mesh = nullptr;

	int index = list.indexOf("locatorMesh");

	if (index < 0)
	{
		mesh = MHWRender::MRenderItem::Create("locatorMesh",
			MHWRender::MRenderItem::DecorationItem,
			MHWRender::MGeometry::kLines);
		mesh->setDrawMode(MHWRender::MGeometry::kAll);
		list.append(mesh);
	}
	else
		mesh = list.itemAt(index);

	if (mesh)
	{
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

		mesh->enable(true);
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
