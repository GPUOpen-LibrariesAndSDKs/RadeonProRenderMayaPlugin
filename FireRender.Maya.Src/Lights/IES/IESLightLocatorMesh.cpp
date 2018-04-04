#include "FireRenderIESLight.h"
#include "IESLightLocatorMesh.h"

#include <cassert>
#include <fstream>
#include <sstream>

#include <maya/MFloatMatrix.h>
#include <maya/MEulerRotation.h>

#include "base_mesh.h"
#include "FireRenderError.h"
#include "FireRenderUtils.h"
#include "IESLight/IESprocessor.h"
#include "IESLight/IESLightRepresentationCalc.h"
#include "Translators.h"

namespace
{
	// TODO: these parameters are hardcoded for now. Fix it!
	const float IES_SCALE_MUL = 0.05f;

	template<typename T, size_t N>
	constexpr size_t StackArraySize(const T(&arr)[N])
	{
		return N;
	}

	const char* DescribeIESError(IESLightRepresentationErrorCode code)
	{
		switch (code)
		{
			case IESLightRepresentationErrorCode::INVALID_DATA:
				return "Invalid ies data";

			case IESLightRepresentationErrorCode::NO_EDGES:
				return "Could not build nay edges to show ies light";
		}

		// Wrong use of this function
		assert(false);
		return nullptr;
	}

	const char* DescribeIESError(IESProcessor::ErrorCode code)
	{
		switch (code)
		{
			case IESProcessor::ErrorCode::NO_FILE:
				return "Given file is empty";

			case IESProcessor::ErrorCode::NOT_IES_FILE:
				return "Wrong file (not *ies)";

			case IESProcessor::ErrorCode::FAILED_TO_READ_FILE:
				return "Failed to open the file";

			case IESProcessor::ErrorCode::INVALID_DATA_IN_IES_FILE:
				return "Invalid data in ies file";

			case IESProcessor::ErrorCode::PARSE_FAILED:
				return "ies file parsing failed";

			case IESProcessor::ErrorCode::UNEXPECTED_END_OF_FILE:
				return "Unexpected end of ies file";
		}

		// Wrong use of this function
		assert(false);
		return nullptr;
	}

	bool GenerateIESRepresentation(
		const wchar_t* filename,
		size_t pointsPerPolyline,
		float scale,
		std::vector<MFloatVector>& vertices,
		std::vector<unsigned int>& indices)
	{
		vertices.clear();
		indices.clear();

		FireRenderError error;
		IESProcessor processor;
		IESLightRepresentationParams params;
		std::vector<std::vector<RadeonProRender::float3>> polylines;
		params.maxPointsPerPLine = pointsPerPolyline;
		params.webScale = scale;

		auto parseError = processor.Parse(params.data, filename);

		if (parseError != IESProcessor::ErrorCode::SUCCESS)
		{
			std::stringstream errorMessage;
			const char* errorDescription = DescribeIESError(parseError);
			errorMessage << "RPR Error: Failed to parse ies file";

			if (errorDescription != nullptr)
			{
				errorMessage << " (reason: " << errorDescription << ") ";
			}

			error.set("Parse error", errorMessage.str().c_str(), true, true);
		}

		if (!error.check())
		{
			auto calcError = CalculateIESLightRepresentation(polylines, params);

			if (calcError != IESLightRepresentationErrorCode::SUCCESS)
			{
				std::stringstream errorMessage;
				const char* errorDescription = DescribeIESError(calcError);
				errorMessage << "RPR Warning: ies file parsed successfully but failed to build it's representation";

				if (errorDescription != nullptr)
				{
					errorMessage << " (reason: " << errorDescription << ") ";
				}

				error.set("Show ies form failed", errorMessage.str().c_str());
			}
		}

		if (error.check())
		{
			return false;
		}

		// Convert polyline to lines
		for (const auto& polyline : polylines)
		{
			size_t verticesCount = polyline.size();
			for (size_t nVertex = 0; nVertex < verticesCount; ++nVertex)
			{
				const bool duplicateIndex = (nVertex > 0 && nVertex + 1 < verticesCount);
				const auto& vertex = polyline[nVertex];
				const unsigned vertexIndex = static_cast<unsigned>(vertices.size());

				indices.insert(indices.end(), duplicateIndex ? 2 : 1, vertexIndex);
				vertices.emplace_back(vertex.x, vertex.y, vertex.z);
			}
		}

		return true;
	}

	void GenerateSphereRepresentation(
		std::vector<MFloatVector>& vertices,
		std::vector<unsigned int>& indices)
	{
		vertices.clear();
		indices.clear();

		// Get the position of the sun.
		MFloatVector sunPosition(0, 0, 0);

		// Add sun vertices using low detail sphere data.
		constexpr size_t sunSphereFloatsCount = StackArraySize(lowSpherePoints);
		static_assert(sunSphereFloatsCount % 3 == 0, "Invalid input array");
		constexpr size_t sunSphereVertexCount = sunSphereFloatsCount / 3;

		// Reserve memory for vertices
		vertices.reserve(sunSphereVertexCount);

		// Copy vertices
		for (size_t vertexIdx = 0; vertexIdx < sunSphereVertexCount; vertexIdx++)
		{
			const float* p = &lowSpherePoints[vertexIdx * 3];

			vertices.emplace_back(
				p[0] * 0.5f + sunPosition.x,
				p[1] * 0.5f + sunPosition.y,
				p[2] * 0.5f + sunPosition.z);
		}

		// Add sun indices using low detail sphere connectivity.
		constexpr size_t sunSphereIndexCount = StackArraySize(lowSphereWireConnect);
		indices.resize(sunSphereIndexCount);
		std::copy(lowSphereWireConnect, lowSphereWireConnect + sunSphereIndexCount, indices.begin());
	}
}

bool IESLightLocatorMeshBase::SetFilename(const MString filename, bool forcedUpdate)
{
	// The same. Do nothing
	if (!forcedUpdate && filename == m_filename)
	{
		return false;
	}

	m_filename = filename;

	const wchar_t* castedName = m_filename.asWChar();
	std::wstring local = castedName;

	if (local.empty())
	{
		GenerateSphereRepresentation(m_vertices, m_indices);
	}
	else
	{
		GenerateIESRepresentation(castedName, 32, IES_SCALE_MUL, m_vertices, m_indices);
	}

	return true;
}


IESLightLegacyLocatorMesh::IESLightLegacyLocatorMesh() :
	m_scale(1.f),
	m_enabled(true),
	m_angles{0, 0, 0}
{
}

void IESLightLegacyLocatorMesh::SetScale(float value)
{
	m_scale = value;
}

void IESLightLegacyLocatorMesh::SetEnabled(bool value)
{
	m_enabled = value;
}

void IESLightLegacyLocatorMesh::SetAngle(float value, int axis)
{
	if (axis > 2)
	{
		assert(false);
		return;
	}

	m_angles[axis] = value;
}

void IESLightLegacyLocatorMesh::Draw(M3dView& view)
{
	if (!m_enabled)
	{
		return;
	}

	view.beginGL();

	glPushMatrix();
	
	// Convert angles from degrees to radians
	float radAngles[3];
	for (int i = 0; i < 3; ++i)
	{
		radAngles[i] = FireMaya::deg2rad(m_angles[i]);
	}
	
	// Make euler rotation and mult the result matrix
	MEulerRotation rot(radAngles[0], radAngles[1], radAngles[2]);
	MMatrix matrix = rot.asMatrix();
	float rawMatrix[4][4];
	matrix.get(rawMatrix);
	glMultMatrixf(rawMatrix[0]);

	glScalef(m_scale, m_scale, m_scale);

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

	glPopMatrix();

	view.endGL();
}

void IESLightLocatorMesh::populateOverrideGeometry(
	const MHWRender::MGeometryRequirements &requirements,
	const MHWRender::MRenderItemList &renderItems,
	MHWRender::MGeometry &data)
{
	// Get the vertex and index counts.
	unsigned int vertexCount = static_cast<unsigned int>(m_vertices.size());
	unsigned int indexCount = static_cast<unsigned int>(m_indices.size());

	// Get vertex buffer requirements.
	auto& vertexBufferDescriptorList = requirements.vertexRequirements();
	const int vertexBufferCount = vertexBufferDescriptorList.length();

	float* vertices = nullptr;
	MHWRender::MVertexBuffer* vertexBuffer = nullptr;

	// Create the vertex buffer.
	for (int i = 0; i < vertexBufferCount; ++i)
	{
		MHWRender::MVertexBufferDescriptor vertexBufferDescriptor;
		if (!vertexBufferDescriptorList.getDescriptor(i, vertexBufferDescriptor))
		{
			continue;
		}

		switch (vertexBufferDescriptor.semantic())
		{
			case MHWRender::MGeometry::kPosition:
			{
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
