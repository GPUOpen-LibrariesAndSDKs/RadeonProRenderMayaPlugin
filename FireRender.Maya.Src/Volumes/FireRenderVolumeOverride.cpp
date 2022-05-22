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
#include "FireRenderVolumeOverride.h"
#include "FireRenderUtils.h"
#include "VolumeAttributes.h"
#include <maya/MHWGeometry.h>
#include <maya/MShaderManager.h>
#include <maya/MHWGeometryUtilities.h>
#include <vector>

// ================================
// Viewport 2.0 override
// ================================

using namespace MHWRender;

MColor FireRenderVolumeOverride::m_SelectedColor(1.0f, 1.0f, 0.0f);
MColor FireRenderVolumeOverride::m_Color(0.3f, 0.0f, 0.0f);

FireRenderVolumeOverride::FireRenderVolumeOverride(const MObject& obj)
	: MPxGeometryOverride(obj),
	m_depNodeObj(obj),
	m_changed(true)
{
	m_GridSizeValues.nx = 4;
	m_GridSizeValues.ny = 4;
	m_GridSizeValues.nz = 4;

	m_VoxelSizeValues.nx = 0.25;
	m_VoxelSizeValues.ny = 0.25;
	m_VoxelSizeValues.nz = 0.25;
}

FireRenderVolumeOverride::~FireRenderVolumeOverride()
{
}

DrawAPI FireRenderVolumeOverride::supportedDrawAPIs() const
{
	return (kOpenGL | kDirectX11 | kOpenGLCoreProfile);
}

void FireRenderVolumeOverride::updateDG()
{
	// update m_currentTrackedValues

	MDataHandle volumeGirdSizes = RPRVolumeAttributes::GetVolumeGridDimentions(m_depNodeObj.object());

	if (volumeGirdSizes.asShort3()[0] != m_GridSizeValues.nx)
	{
		m_GridSizeValues.nx = volumeGirdSizes.asShort3()[0];
		m_changed = true;
	}

	if (volumeGirdSizes.asShort3()[1] != m_GridSizeValues.ny)
	{
		m_GridSizeValues.ny = volumeGirdSizes.asShort3()[1];
		m_changed = true;
	}

	if (volumeGirdSizes.asShort3()[2] != m_GridSizeValues.nz)
	{
		m_GridSizeValues.nz = volumeGirdSizes.asShort3()[2];
		m_changed = true;
	}

	MDataHandle volumeVoxelSizes = RPRVolumeAttributes::GetVolumeVoxelSize(m_depNodeObj.object());

	if (volumeVoxelSizes.asDouble3()[0] != m_VoxelSizeValues.nx)
	{
		m_VoxelSizeValues.nx = volumeVoxelSizes.asDouble3()[0];
		m_changed = true;
	}

	if (volumeVoxelSizes.asDouble3()[1] != m_VoxelSizeValues.ny)
	{
		m_VoxelSizeValues.ny = volumeVoxelSizes.asDouble3()[1];
		m_changed = true;
	}

	if (volumeVoxelSizes.asDouble3()[2] != m_VoxelSizeValues.nz)
	{
		m_VoxelSizeValues.nz = volumeVoxelSizes.asDouble3()[2];
		m_changed = true;
	}
}

bool FireRenderVolumeOverride::hasUIDrawables() const
{
	return true;
}

void FireRenderVolumeOverride::addUIDrawables(const MDagPath &path, MUIDrawManager &drawManager, const MFrameContext &frameContext)
{
	MColor color = GetColor(path);
	MPoint position(0, 0, 0);

	drawManager.beginDrawable();
	drawManager.setColor(color);
	drawManager.icon(position, "POINT_LIGHT", 1.0f);
	drawManager.endDrawable();
}

const MString shapeWires = "shapeWires";

void FireRenderVolumeOverride::updateRenderItems(const MDagPath& path, MRenderItemList& list)
{
	MRenderItem* shapeToRender = nullptr;

	int index = list.indexOf(shapeWires);

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

MColor FireRenderVolumeOverride::GetColor(const MDagPath& path)
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

bool FireRenderVolumeOverride::refineSelectionPath(
	const MSelectionInfo &selectInfo,
	const MRenderItem &hitItem,
	MDagPath &path,
	MObject &components,
	MSelectionMask &objectMask)
{
	return true;
}

void FireRenderVolumeOverride::updateSelectionGranularity(
	const MDagPath& path,
	MSelectionContext& selectionContext)
{

}

void FireRenderVolumeOverride::populateGeometry(
	const MGeometryRequirements& requirements,
	const MRenderItemList& renderItems,
	MGeometry& data)
{
	if (renderItems.length() == 0)
	{
		return;
	}

	const float sizeX = m_GridSizeValues.nx * m_VoxelSizeValues.nx;
	const float sizeY = m_GridSizeValues.ny * m_VoxelSizeValues.ny;
	const float sizeZ = m_GridSizeValues.nz * m_VoxelSizeValues.nz;

	// create volume boundaries
	std::vector<float> veritces = {
		-sizeX / 2, -sizeY / 2, -sizeZ / 2,
		 sizeX / 2, -sizeY / 2, -sizeZ / 2,
		 sizeX / 2,  sizeY / 2, -sizeZ / 2,
		-sizeX / 2,  sizeY / 2, -sizeZ / 2,

		-sizeX / 2, -sizeY / 2,  sizeZ / 2,
		 sizeX / 2, -sizeY / 2,  sizeZ / 2,
		 sizeX / 2,  sizeY / 2,  sizeZ / 2,
		-sizeX / 2,  sizeY / 2,  sizeZ / 2,
	};
	std::vector<int> vertexIndices = {
		0, 1, 1, 2, 2, 3, 3, 0,
		4, 5, 5, 6, 6, 7, 7, 4,
		0, 4, 1, 5, 2, 6, 3, 7
	};

	// create grid
	int last_boundary_vtx_idx = (int) ((veritces.size() / 3) - 1);
	float step = sizeX / (m_GridSizeValues.nx);
	for (short idx = 1; idx < (m_GridSizeValues.nx); ++idx)
	{
		veritces.push_back(-sizeX / 2 + step*idx);
		veritces.push_back(-sizeY / 2);
		veritces.push_back(-sizeZ / 2);

		veritces.push_back(-sizeX / 2 + step*idx);
		veritces.push_back(-sizeY / 2);
		veritces.push_back( sizeZ / 2);

		vertexIndices.push_back(last_boundary_vtx_idx + (idx-1)*2 + 1);
		vertexIndices.push_back(last_boundary_vtx_idx + (idx-1)*2 + 2);
	}

	last_boundary_vtx_idx = (int) ((veritces.size()/3) - 1);
	step = sizeZ / (m_GridSizeValues.nz);
	for (short idx = 1; idx < (m_GridSizeValues.nz); ++idx)
	{
		veritces.push_back(-sizeX / 2);
		veritces.push_back(-sizeY / 2);
		veritces.push_back(-sizeZ / 2 + step*idx);

		veritces.push_back( sizeX / 2);
		veritces.push_back(-sizeY / 2);
		veritces.push_back(-sizeZ / 2 + step*idx);

		vertexIndices.push_back(last_boundary_vtx_idx + (idx-1)*2 + 1);
		vertexIndices.push_back(last_boundary_vtx_idx + (idx-1)*2 + 2);
	}

	// Get the vertex and index counts.
	unsigned int vertexCount = static_cast<unsigned int>(veritces.size() / 3);
	unsigned int indexCount = static_cast<unsigned int>(vertexIndices.size());

	// Get vertex buffer requirements.
	auto& vertexBufferDescriptorList = requirements.vertexRequirements();
	const int vertexBufferCount = vertexBufferDescriptorList.length();

	float* vertices = nullptr;
	MVertexBuffer* vertexBuffer = nullptr;

	// Create the vertex buffer
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
				vertices = (float*)vertexBuffer->acquire(sizeof(float) * vertexCount * 3);
			}

			break;
		}
	}

	// Populate the vertex buffer
	if (vertexBuffer && vertices)
	{
		memcpy(vertices, veritces.data(), sizeof(float) * vertexCount * 3);
		vertexBuffer->commit(vertices);
	}

	// Create and populate the index buffer
	MIndexBuffer*  indexBuffer = data.createIndexBuffer(MGeometry::kUnsignedInt32);
	unsigned int* indices = (unsigned int*)indexBuffer->acquire(indexCount);

	if (indices)
	{
		memcpy(indices, vertexIndices.data(), sizeof(unsigned int) * indexCount);
		indexBuffer->commit(indices);
	}

	const MRenderItem* renderItem = renderItems.itemAt(0);
	renderItem->associateWithIndexBuffer(indexBuffer);

	m_changed = false;
}

