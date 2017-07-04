#include "IESLightLocatorMesh.h"

#include "base_mesh.h"
#include <maya/MFloatMatrix.h>


// Life Cycle
// -----------------------------------------------------------------------------
IESLightLocatorMesh::IESLightLocatorMesh(const MObject& object)
{
}

// -----------------------------------------------------------------------------
IESLightLocatorMesh::~IESLightLocatorMesh()
{
}


// Public Methods
// -----------------------------------------------------------------------------
bool IESLightLocatorMesh::refresh()
{
	bool changed = m_vertices.empty();

	if (changed)
		generateMesh();

	return changed;
}

// -----------------------------------------------------------------------------
void IESLightLocatorMesh::glDraw(M3dView& view)
{
	view.beginGL();

	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);

	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, m_vertices.data());

	glDrawElements(GL_LINES,
		static_cast<GLsizei>(m_indices.size()),
		GL_UNSIGNED_INT, m_indices.data());

	glDisableClientState(GL_VERTEX_ARRAY);

	glPopClientAttrib();
	glPopAttrib();

	view.endGL();
}

// -----------------------------------------------------------------------------
void IESLightLocatorMesh::populateOverrideGeometry(
	const MHWRender::MGeometryRequirements &requirements,
	const MHWRender::MRenderItemList &renderItems,
	MHWRender::MGeometry &data)
{
	// Get the vertex and index counts.
	auto vertexCount = static_cast<unsigned int>(m_vertices.size());
	auto indexCount = static_cast<unsigned int>(m_indices.size());

	// Get vertex buffer requirements.
	auto& vertexBufferDescriptorList = requirements.vertexRequirements();
	const int vertexBufferCount = vertexBufferDescriptorList.length();

	float* vertices = nullptr;
	MHWRender::MVertexBuffer* vertexBuffer = nullptr;

	// Create the vertex buffer.
	MHWRender::MVertexBufferDescriptor vertexBufferDescriptor;
	for (int i = 0; i < vertexBufferCount; ++i)
	{
		if (!vertexBufferDescriptorList.getDescriptor(i, vertexBufferDescriptor))
			continue;

		switch (vertexBufferDescriptor.semantic())
		{
		case MHWRender::MGeometry::kPosition:
		{
			if (vertexBuffer)
				break;

			vertexBuffer = data.createVertexBuffer(vertexBufferDescriptor);
			if (vertexBuffer)
				vertices = (float*)vertexBuffer->acquire(sizeof(MFloatVector) * vertexCount);

			break;
		}

		default:
			break;
		}
	}

	// Populate the vertex buffer.
	if (vertexBuffer && vertices)
	{
		memcpy(vertices, m_vertices.data(), sizeof(MFloatVector) * vertexCount);
		vertexBuffer->commit(vertices);
	}

	// Create and populate the index buffer.
	for (int i = 0; i < renderItems.length(); ++i)
	{
		const MHWRender::MRenderItem* item = renderItems.itemAt(i);

		if (!item)
			continue;

		MHWRender::MIndexBuffer*  indexBuffer = data.createIndexBuffer(MHWRender::MGeometry::kUnsignedInt32);
		unsigned int* indices = (unsigned int*)indexBuffer->acquire(indexCount);

		if (indices)
		{
			memcpy(indices, m_indices.data(), sizeof(unsigned int) * indexCount);
			indexBuffer->commit(indices);
		}

		item->associateWithIndexBuffer(indexBuffer);
	}
}


// Private Methods
// -----------------------------------------------------------------------------
void IESLightLocatorMesh::generateMesh()
{
	// Initialize mesh vertex and index storage.
	m_vertices.reserve(getVertexCount());
	m_indices.reserve(getIndexCount());

	m_vertices.clear();
	m_indices.clear();

	// Generate the sun mesh.
	generateSunMesh();
}

// -----------------------------------------------------------------------------
void IESLightLocatorMesh::generateSunMesh()
{
	// Get the vertex start index.
	unsigned int startIndex = static_cast<unsigned int>(m_vertices.size());

	// Get the position of the sun.
	MFloatVector sunPosition(0, 0, 0);

	// Add sun vertices using low detail sphere data.
	unsigned int i = 0;
	for (i = 0; i < c_sunSphereVertexCount; i++)
	{
		const float* p = &lowSpherePoints[i * 3];

		m_vertices.push_back(
			MFloatVector(
				p[0] * 0.5f + sunPosition.x,
				p[1] * 0.5f + sunPosition.y,
				p[2] * 0.5f + sunPosition.z));
	}

	// Add sun indices using low detail sphere connectivity.
	for (i = 0; i < c_sunSphereIndexCount; i++)
	{
		unsigned int index = lowSphereWireConnect[i] + startIndex;
		m_indices.push_back(index);
	}

}

// -----------------------------------------------------------------------------
unsigned int IESLightLocatorMesh::getVertexCount()
{
	return c_sunSphereVertexCount;
}

// -----------------------------------------------------------------------------
unsigned int IESLightLocatorMesh::getIndexCount()
{
	return c_sunSphereIndexCount;
}
