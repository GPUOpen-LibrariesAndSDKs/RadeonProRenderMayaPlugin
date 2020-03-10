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
#include "FireRenderPhysicalOverride.h"
#include "PhysicalLightAttributes.h"
#include "PhysicalLightGeometryUtility.h"
#include "FireRenderUtils.h"

#include <maya/MHWGeometry.h>
#include <maya/MShaderManager.h>
#include <maya/MHWGeometryUtilities.h>
#include <vector>

// ================================
// Viewport 2.0 override
// ================================

using namespace MHWRender;

MColor FireRenderPhysicalOverride::m_SelectedColor(1.0f, 1.0f, 0.0f);
MColor FireRenderPhysicalOverride::m_Color(0.3f, 0.0f, 0.0f);

FireRenderPhysicalOverride::FireRenderPhysicalOverride(const MObject& obj)
	: MPxGeometryOverride(obj),
	m_depNodeObj(obj),
	m_changed(true)
{
	m_currentTrackedValues.areaLightShape = PLAreaLightShape::PLARectangle;
	m_currentTrackedValues.lightType = PLType::PLTArea;
	m_currentTrackedValues.spotLightInnerConeAngle = 0.0f;
	m_currentTrackedValues.spotLightOuterConeFalloff = 0.0f;
}

FireRenderPhysicalOverride::~FireRenderPhysicalOverride()
{
}

DrawAPI FireRenderPhysicalOverride::supportedDrawAPIs() const
{
	return (kOpenGL | kDirectX11 | kOpenGLCoreProfile);
}

const MString shapeWires = "shapeWires";

void FireRenderPhysicalOverride::updateDG()
{
	PLType lightType = PhysicalLightAttributes::GetLightType(m_depNodeObj.object());

	if (m_currentTrackedValues.lightType != lightType)
	{
		m_currentTrackedValues.lightType = lightType;
		m_changed = true;
	}

	PLAreaLightShape areaLightShape = PhysicalLightAttributes::GetAreaLightShape(m_depNodeObj.object());

	if (m_currentTrackedValues.areaLightShape != areaLightShape)
	{
		m_currentTrackedValues.areaLightShape = areaLightShape;
		m_changed = true;
	}

	float spotLightInnerConeAngle;
	float spotLightOuterConeFalloff;

	PhysicalLightAttributes::GetSpotLightSettings(m_depNodeObj.object(), spotLightInnerConeAngle, spotLightOuterConeFalloff);

	if (m_currentTrackedValues.spotLightInnerConeAngle != spotLightInnerConeAngle ||
		m_currentTrackedValues.spotLightOuterConeFalloff != spotLightOuterConeFalloff)
	{
		m_currentTrackedValues.spotLightInnerConeAngle = spotLightInnerConeAngle;
		m_currentTrackedValues.spotLightOuterConeFalloff = spotLightOuterConeFalloff;
		m_changed = true;
	}

	MString attachedMeshName = PhysicalLightAttributes::GetAreaLightMeshSelectedName(m_depNodeObj.object());

	if (m_currentTrackedValues.attachedMeshName != attachedMeshName)
	{
		m_currentTrackedValues.attachedMeshName = attachedMeshName;
		m_changed = true;
	}
}

bool FireRenderPhysicalOverride::IsPointLight() const
{
	return PhysicalLightAttributes::GetLightType(m_depNodeObj.object()) == PLTPoint;
}

bool FireRenderPhysicalOverride::IsAreaMeshLight() const
{
	return PhysicalLightAttributes::GetLightType(m_depNodeObj.object()) == PLTArea && 
			PhysicalLightAttributes::GetAreaLightShape(m_depNodeObj.object()) == PLAMesh;
}

bool FireRenderPhysicalOverride::HasNoGeomOverride() const
{
	return IsPointLight();
}

bool FireRenderPhysicalOverride::hasUIDrawables() const 
{ 
	return true;
}

void FireRenderPhysicalOverride::addUIDrawables(const MDagPath &path, MUIDrawManager &drawManager, const MFrameContext &frameContext)
{
	if (!IsPointLight())
	{
		return;
	}

	MColor color = GetColor(path);
	MPoint position(0, 0, 0);

	drawManager.beginDrawable();
	drawManager.setColor(color);
	drawManager.icon(position, "POINT_LIGHT", 1.0f);
	drawManager.endDrawable();
}

void FireRenderPhysicalOverride::updateRenderItems(const MDagPath& path, MRenderItemList& list)
{
	MRenderItem* shapeToRender = nullptr;

	int index = list.indexOf(shapeWires);

	if (index >= 0 && HasNoGeomOverride())
	{
		MRenderItem* item = list.itemAt(index);
		item->enable(false);
		return;
	}

	if (index < 0)
	{
		shapeToRender = MRenderItem::Create(
			shapeWires,
			MRenderItem::DecorationItem,
			MGeometry::kLines);
		shapeToRender->setDrawMode(MGeometry::kAll);

		list.append(shapeToRender);
	}
	else
	{
		shapeToRender = list.itemAt(index);
	}

	if (shapeToRender)
	{
		MTransformationMatrix trm;
		float areaWidth = PhysicalLightAttributes::GetAreaWidth(m_depNodeObj.object());
		float areaLength = PhysicalLightAttributes::GetAreaLength(m_depNodeObj.object());

		double scale[3]{ areaWidth, areaWidth, areaLength};
		trm.setScale(scale, MSpace::Space::kObject);

        MMatrix mmatrix = trm.asMatrix();
		shapeToRender->setMatrix(&mmatrix);

		auto renderer = MRenderer::theRenderer();
		auto shaderManager = renderer->getShaderManager();
		auto shader = shaderManager->getStockShader(MShaderManager::k3dSolidShader);
		if (shader)
		{
			MColor color = GetColor(path);

			const float theColor[] = { color.r, color.g, color.b, 1.0f };
			shader->setParameter("solidColor", theColor);
			shapeToRender->setShader(shader);
			shaderManager->releaseShader(shader);
		}

		shapeToRender->enable(true);
	}
}

MColor FireRenderPhysicalOverride::GetColor(const MDagPath& path)
{
	MHWRender::DisplayStatus displayStatus = MHWRender::MGeometryUtilities::displayStatus(path);
	MColor color = m_Color;

	switch (displayStatus)
	{
	case MHWRender::kActive:
	case MHWRender::kLead:
		color = m_SelectedColor;
	}

	return color;
}
void FireRenderPhysicalOverride::populateGeometry(
	const MGeometryRequirements& requirements,
	const MRenderItemList& renderItems,
	MGeometry& data)
{
	if (renderItems.length() == 0)
	{
		return;
	}

	const MRenderItem* renderItem = renderItems.itemAt(0);

	MDagPath dagPath;
	dagPath = renderItem->sourceDagPath();

	IndexVector indexVector;
	GizmoVertexVector vertVector;

	CreateGizmoGeometry(vertVector, indexVector, dagPath);

	// Get the vertex and index counts.
	unsigned int vertexCount = static_cast<unsigned int>(vertVector.size());
	unsigned int indexCount = static_cast<unsigned int>(indexVector.size());

	// Get vertex buffer requirements.
	auto& vertexBufferDescriptorList = requirements.vertexRequirements();
	const int vertexBufferCount = vertexBufferDescriptorList.length();

	float* vertices = nullptr;
	MVertexBuffer* vertexBuffer = nullptr;

	// Create the vertex buffer.
	for (int i = 0; i < vertexBufferCount; ++i)
	{
		MVertexBufferDescriptor vertexBufferDescriptor;
		if (!vertexBufferDescriptorList.getDescriptor(i, vertexBufferDescriptor))
		{
			continue;
		}

		switch (vertexBufferDescriptor.semantic())
		{
		case MGeometry::kPosition:
			if (vertexBuffer)
			{
				break;
			}

			vertexBuffer = data.createVertexBuffer(vertexBufferDescriptor);

			if (vertexBuffer)
			{
				vertices = (float*)vertexBuffer->acquire(sizeof(MFloatVector) * vertexCount);
			}

			break;
		}
	}

	// Populate the vertex buffer.
	if (vertexBuffer && vertices)
	{
		memcpy(vertices, vertVector.data(), sizeof(MFloatVector) * vertexCount);
		vertexBuffer->commit(vertices);
	}

	// Create and populate the index buffer.
	MIndexBuffer*  indexBuffer = data.createIndexBuffer(MGeometry::kUnsignedInt32);
	unsigned int* indices = (unsigned int*)indexBuffer->acquire(indexCount);

	if (indices)
	{
		memcpy(indices, indexVector.data(), sizeof(unsigned int) * indexCount);
		indexBuffer->commit(indices);
	}

	renderItem->associateWithIndexBuffer(indexBuffer);

	m_changed = false;
}

void FireRenderPhysicalOverride::FillWithWireframeForMesh(GizmoVertexVector& vertexVector, IndexVector& indexVector, const MDagPath& dagPath)
{
	MDagPath shapePath;
	if (!findMeshShapeForMeshPhysicalLight(dagPath, shapePath))
	{
		return;
	}

	MFnMesh mesh(shapePath.node());

	// Fill vertices
	int vertexCount = mesh.numVertices();
	if (vertexCount == 0)
	{
		return;
	}

	vertexVector.reserve(vertexCount);

	MPoint point;
	for (int i = 0; i < vertexCount; ++i)
	{
		mesh.getPoint(i, point);
		vertexVector.push_back(MFloatVector(point));
	}

	// Fill indices

	MIntArray polygonIndexList;

	indexVector.reserve(mesh.numPolygons() * 3);

	for (int faceIdx = 0; faceIdx < mesh.numPolygons(); faceIdx++)
	{
		// ignore degenerate faces
		mesh.getPolygonVertices(faceIdx, polygonIndexList);

		int numVerts = mesh.polygonVertexCount(faceIdx);
		if (numVerts < 2)
		{
			continue;
		}

		for (int i = 0; i < numVerts; i++)
		{
			indexVector.push_back(polygonIndexList[i]);
			indexVector.push_back(polygonIndexList[(i + 1) % numVerts]);
		}
	}
}

void FireRenderPhysicalOverride::CreateGizmoGeometry(GizmoVertexVector& vertexVector, IndexVector& indexVector, const MDagPath& dagPath)
{
	PhysicalLightData data;

	PhysicalLightAttributes::FillPhysicalLightData(data, m_depNodeObj.object(), nullptr);

	vertexVector.clear();
	indexVector.clear();

	switch (data.lightType)
	{
	case PLTArea:
		if (data.areaLightShape != PLAMesh)
		{
			PhysicalLightGeometryUtility::FillGizmoGeometryForAreaLight(data.areaLightShape, vertexVector, indexVector);
		}
		else
		{
			FillWithWireframeForMesh(vertexVector, indexVector, dagPath);
		}
		break;
	case PLTSpot:
		PhysicalLightGeometryUtility::FillGizmoGeometryForSpotLight(vertexVector, indexVector, data.spotInnerAngle, data.spotOuterFallOff);
		break;
	case PLTDirectional:
		PhysicalLightGeometryUtility::FillGizmoGeometryForDirectionalLight(vertexVector, indexVector);
		break;
	}
}

bool FireRenderPhysicalOverride::refineSelectionPath(
	const MSelectionInfo &selectInfo,
	const MRenderItem &hitItem,
	MDagPath &path,
	MObject &components,
	MSelectionMask &objectMask)
{
	return true;
}

void FireRenderPhysicalOverride::updateSelectionGranularity(
	const MDagPath& path,
	MSelectionContext& selectionContext)
{

}
