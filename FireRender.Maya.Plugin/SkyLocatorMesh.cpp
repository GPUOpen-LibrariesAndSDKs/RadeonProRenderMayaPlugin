#include "SkyLocatorMesh.h"
#include "base_mesh.h"
#include <maya/MFloatMatrix.h>
#include "FireRenderMath.h"


// Life Cycle
// -----------------------------------------------------------------------------
SkyLocatorMesh::SkyLocatorMesh(const MObject& object) :
	m_skyBuilder(object)
{
}

// -----------------------------------------------------------------------------
SkyLocatorMesh::~SkyLocatorMesh()
{
}


// Public Methods
// -----------------------------------------------------------------------------
bool SkyLocatorMesh::refresh()
{
	// Recompute sky parameters. Note that it returns 'false' if azimuth was changed
	// because sky IBL don't need to be updated. So, ignore return value here.
	m_skyBuilder.refresh();

	// Verify sun direction. This is only the parameter used in this class.
	MFloatVector sunDirection = m_skyBuilder.getWorldSpaceSunDirection();
	if ((sunDirection - m_sunDirection).length() < 0.001f)
	{
		// Not changed, do nothing.
		return false;
	}

	// Direction has been changed, recreate wire-frame geometry.
	generateMesh();
	m_sunDirection = sunDirection;

	// Indicate that geometry has been changed.
	return true;
}

// -----------------------------------------------------------------------------
void SkyLocatorMesh::glDraw(M3dView& view)
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
void SkyLocatorMesh::populateOverrideGeometry(
	const MHWRender::MGeometryRequirements &requirements,
	const MHWRender::MRenderItemList &renderItems,
	MHWRender::MGeometry &data)
{
	// Get the vertex and index counts.
	unsigned int vertexCount = static_cast<unsigned int>(m_vertices.size());
	unsigned int indexCount = static_cast<unsigned int>(m_indices.size());

	// Get vertex buffer requirements.
	const MHWRender::MVertexBufferDescriptorList& vertexBufferDescriptorList =
		requirements.vertexRequirements();
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
void SkyLocatorMesh::generateMesh()
{
	// Initialize mesh vertex and index storage.
	m_vertices.reserve(getVertexCount());
	m_indices.reserve(getIndexCount());

	m_vertices.clear();
	m_indices.clear();

	// Generate the compass and sun meshes.
	generateCompassMesh();
	generateSunMesh();
}

// -----------------------------------------------------------------------------
void SkyLocatorMesh::generateCompassMesh()
{
	// Define radial compass rose points.
	float ro = 150.f;
	float ri = 50.f;
	float w = ro - ri;
	float rt = w * 0.3f;
	float scale = 0.05f;
	const unsigned int pointCount = 16;

	MFloatVector points[pointCount] = {
		{ 0, ro }, { 27, rt }, { 45, ri }, { 63, rt },
		{ 90, ro },	{ 117, rt }, { 135, ri }, { 153, rt },
		{ 180, ro }, { 207, rt }, { 225, ri }, { 243, rt },
		{ 270, ro }, { 297, rt }, { 315, ri }, { 333, rt }};

	// Add world space vertices.
	unsigned int i = 0;
	for (i = 0; i < pointCount; i++)
	{
		// Calculate the vertex position from the radial values.
		float a = points[i].x * PI_F / 180.0f;
		float r = points[i].y * scale;
		m_vertices.push_back(MFloatVector(r * cos(a), c_height, r * sin(a)));
		m_indices.push_back(i);

		// Index all but the first vertex twice as each one is the
		// end of the previous line and the start of the next line.
		if (i > 0)
			m_indices.push_back(i);

		// Complete the loop by indexing the first point again.
		if (i == pointCount - 1)
			m_indices.push_back(0);
	}

	// Add vertices for the 'N' to indicate North.
	m_vertices.push_back(MFloatVector(-5 * scale, c_height, -160 * scale));
	m_vertices.push_back(MFloatVector(-5 * scale, c_height, -170 * scale));
	m_vertices.push_back(MFloatVector(5 * scale, c_height, -160 * scale));
	m_vertices.push_back(MFloatVector(5 * scale, c_height, -170 * scale));

	m_indices.push_back(i);
	m_indices.push_back(i + 1);
	m_indices.push_back(i + 1);
	m_indices.push_back(i + 2);
	m_indices.push_back(i + 2);
	m_indices.push_back(i + 3);
}

// -----------------------------------------------------------------------------
void SkyLocatorMesh::generateSunMesh()
{
	// Get the vertex start index.
	unsigned int startIndex = static_cast<unsigned int>(m_vertices.size());

	// Get the position of the sun.
	MFloatVector sunDirection = m_skyBuilder.getWorldSpaceSunDirection();
	MFloatVector sunPosition = sunDirection * 7.5f;

	// Add sun vertices using low detail sphere data.
	unsigned int i = 0;
	for (i = 0; i < c_sunSphereVertexCount; i++)
	{
		const float* p = &lowSpherePoints[i * 3];

		m_vertices.push_back(
			MFloatVector(
				p[0] * 0.5f + sunPosition.x,
				p[1] * 0.5f + sunPosition.y + c_height,
				p[2] * 0.5f + sunPosition.z));
	}

	// Add sun indices using low detail sphere connectivity.
	for (unsigned int j = 0; j < c_sunSphereIndexCount; j++)
	{
		unsigned int index = lowSphereWireConnect[j] + startIndex;
		m_indices.push_back(index);
	}

	// Add a vector from the center of the compass to the sun.
	MFloatVector start(0, c_height, 0);
	MFloatVector end(sunDirection * 7);
	end.y += c_height;

	m_vertices.push_back(start);
	m_vertices.push_back(end);
	m_indices.push_back(startIndex + i);
	m_indices.push_back(startIndex + i + 1);
}

// -----------------------------------------------------------------------------
unsigned int SkyLocatorMesh::getVertexCount()
{
	return
		c_compassVertexCount +
		c_sunSphereVertexCount +
		c_sunVectorVertexCount;
}

// -----------------------------------------------------------------------------
unsigned int SkyLocatorMesh::getIndexCount()
{
	return
		c_compassIndexCount +
		c_sunSphereIndexCount +
		c_sunVectorIndexCount;
}
