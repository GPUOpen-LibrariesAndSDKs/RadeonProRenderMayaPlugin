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
	m_currentTrackedValues.nx = 4;
	m_currentTrackedValues.ny = 4;
	m_currentTrackedValues.nz = 4;
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

	if (volumeGirdSizes.asShort3()[0] != m_currentTrackedValues.nx)
	{
		m_currentTrackedValues.nx = volumeGirdSizes.asShort3()[0];
		m_changed = true;
	}

	if (volumeGirdSizes.asShort3()[1] != m_currentTrackedValues.ny)
	{
		m_currentTrackedValues.ny = volumeGirdSizes.asShort3()[1];
		m_changed = true;
	}

	if (volumeGirdSizes.asShort3()[2] != m_currentTrackedValues.nz)
	{
		m_currentTrackedValues.nz = volumeGirdSizes.asShort3()[2];
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

	// create volume boundaries
	std::vector<float> veritces = {
		-0.5f, -0.5f, -0.5f,
		0.5f, -0.5f, -0.5f,
		0.5f, 0.5f, -0.5f,
		-0.5f, 0.5f, -0.5f,

		-0.5f, -0.5f, 0.5f,
		0.5f, -0.5f, 0.5f,
		0.5f, 0.5f, 0.5f,
		-0.5f, 0.5f, 0.5f,
	};
	std::vector<int> vertexIndices = {
		0, 1, 1, 2, 2, 3, 3, 0,
		4, 5, 5, 6, 6, 7, 7, 4,
		0, 4, 1, 5, 2, 6, 3, 7
	};

	// create grid
	int last_boundary_vtx_idx = (int) ((veritces.size() / 3) - 1);
	float step = 1.0f / (m_currentTrackedValues.nx);
	for (short idx = 1; idx < (m_currentTrackedValues.nx); ++idx)
	{
		veritces.push_back(-0.5f + step*idx);
		veritces.push_back(-0.5f);
		veritces.push_back(-0.5f);

		veritces.push_back(-0.5f + step*idx);
		veritces.push_back(-0.5f);
		veritces.push_back(0.5f);

		vertexIndices.push_back(last_boundary_vtx_idx + (idx-1)*2 + 1);
		vertexIndices.push_back(last_boundary_vtx_idx + (idx-1)*2 + 2);
	}

	last_boundary_vtx_idx = (int) ((veritces.size()/3) - 1);
	step = 1.0f / (m_currentTrackedValues.nz);
	for (short idx = 1; idx < (m_currentTrackedValues.nz); ++idx)
	{
		veritces.push_back(-0.5f);
		veritces.push_back(-0.5f);
		veritces.push_back(-0.5f + step*idx);

		veritces.push_back(0.5f);
		veritces.push_back(-0.5f);
		veritces.push_back(-0.5f + step*idx);

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

