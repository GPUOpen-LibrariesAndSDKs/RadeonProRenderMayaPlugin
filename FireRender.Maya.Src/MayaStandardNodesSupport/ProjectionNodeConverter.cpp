#include "ProjectionNodeConverter.h"
#include "FireMaya.h"
#include "FireRenderObjects.h"
#include "maya/MFnTransform.h"
#include "maya/MFnMatrixData.h"
#include "Context/FireRenderContext.h"

MayaStandardNodeConverters::ProjectionNodeConverter::ProjectionNodeConverter(const ConverterParams& params) : BaseConverter(params)
{
}

frw::Value MayaStandardNodeConverters::ProjectionNodeConverter::Convert() const
{
	ProjectionType projectionType = static_cast<ProjectionType>(m_params.scope.GetValueWithCheck(m_params.shaderNode, "projType").GetInt());
	frw::Value transparency = m_params.scope.GetValueWithCheck(m_params.shaderNode, "transparency");
	frw::Value uAngle = m_params.scope.GetValueWithCheck(m_params.shaderNode, "uAngle");
	frw::Value vAngle = m_params.scope.GetValueWithCheck(m_params.shaderNode, "vAngle");

	// We should get exactly frw::Image because it only could be set as map input for ImageNode
	// In future we should use frw::Value from "image" node input
	frw::Image imageFromDirectlyConnectedFileNode = GetImageFromConnectedFileNode();
	if (!imageFromDirectlyConnectedFileNode.IsValid())
	{
		return frw::Value();
	}

	// Create UV node corresponding to projection type
	std::unique_ptr<frw::BaseUVNode> proceduralUVResult = CreateUVNode(projectionType);
	if (proceduralUVResult == nullptr)
	{
		return frw::Value();
	}

	// Retrieve mesh or camera transform data
	MayaStandardNodeConverters::ProjectionNodeConverter::TransformInfo transformInfo;
	if (projectionType == ProjectionType::Perspective)
	{
		transformInfo = GetCameraTransform();
	}
	else
	{
		transformInfo = GetPlaceTextureTransfrom();
	}

	// Apply data to core node
	proceduralUVResult->SetOrigin(transformInfo.origin);
	proceduralUVResult->SetInputUVScale(transformInfo.scale);
	proceduralUVResult->SetInputXAxis(transformInfo.rotationX);
	proceduralUVResult->SetInputZAxis(transformInfo.rotationZ);

	frw::ImageNode imageNode(m_params.scope.MaterialSystem());
	imageNode.SetValue("uv", *proceduralUVResult);
	imageNode.SetMap(imageFromDirectlyConnectedFileNode);

	return imageNode;
}

MayaStandardNodeConverters::ProjectionNodeConverter::TransformInfo MayaStandardNodeConverters::ProjectionNodeConverter::GetCameraTransform() const
{
	const IFireRenderContextInfo* contextInfo = m_params.scope.GetContextInfo();
	const FireRenderContext* context = dynamic_cast<const FireRenderContext*>(contextInfo);
	const FireRenderCamera& camera = context->camera();

	MFnDagNode cameraDAGNode(camera.Object());
	MObject transformNode = cameraDAGNode.parent(0);
	MFnTransform cameraTransform(transformNode);

	// Retrieve full matrix to get rotation
	MMatrix transformMatrix = cameraTransform.transformationMatrix();
	double matrix[4][4];
	transformMatrix.transpose().get(matrix);

	// Get translation
	MVector translation = cameraTransform.getTranslation(MSpace::kTransform);

	// Get scale
	double scale[3];
	cameraTransform.getScale(scale);

	return TransformInfo{
		{ translation.x / (100.f * scale[0]), translation.y / (100.f * scale[1]), translation.z / (100.f * scale[2]) },
		{ matrix[0][0], matrix[1][0], matrix[2][0], matrix[3][0] },
		{ matrix[0][2], matrix[1][2], matrix[2][2], matrix[3][2] },
		{ scale[0] * 50.f, scale[1] * 50.f, scale[2] * 50.f }
	};
}

MayaStandardNodeConverters::ProjectionNodeConverter::TransformInfo MayaStandardNodeConverters::ProjectionNodeConverter::GetPlaceTextureTransfrom() const
{
	MPlug placementMatrixPlug = m_params.shaderNode.findPlug("placementMatrix");
	MTransformationMatrix placementMatrix = MFnMatrixData(placementMatrixPlug.asMObject()).transformation();

	// Retrieve full matrix to get rotation
	float matrix[4][4];
	placementMatrix.asMatrix().get(matrix);

	// Get translation
	MVector translation = placementMatrix.getTranslation(MSpace::kTransform);

	// Get scale
	double scale[3];
	placementMatrix.getScale(scale, MSpace::kTransform);

	return TransformInfo{
		{ translation.x / (100.f * scale[0]), translation.y / (100.f * scale[1]), translation.z / (100.f * scale[2]) },
		{ matrix[0][0], matrix[1][0], matrix[2][0], matrix[3][0] },
		{ matrix[0][2], matrix[1][2], matrix[2][2], matrix[3][2] },
		{ scale[0] * 50.f, scale[1] * 50.f, scale[2] * 50.f }
	};
}

std::unique_ptr<frw::BaseUVNode> MayaStandardNodeConverters::ProjectionNodeConverter::CreateUVNode(ProjectionType projectionType) const
{
	switch (projectionType)
	{
		case ProjectionType::Off:
			return nullptr;

		case ProjectionType::Planar:
			return std::make_unique<frw::UVProceduralNode>(m_params.scope.MaterialSystem(), frw::UVTypeValue::Planar);

		case ProjectionType::Spherical:
			return std::make_unique<frw::UVProceduralNode>(m_params.scope.MaterialSystem(), frw::UVTypeValue::Spherical);

		case ProjectionType::Cylindrical:
			return std::make_unique<frw::UVProceduralNode>(m_params.scope.MaterialSystem(), frw::UVTypeValue::Cylindrical);

		case ProjectionType::Ball:
			return nullptr;

		case ProjectionType::Cubic:
			return nullptr;

		case ProjectionType::TriPlanar:
			return std::make_unique<frw::UVTriplanarNode>(m_params.scope.MaterialSystem());

		case ProjectionType::Concentric:
			return nullptr;

		case ProjectionType::Perspective:
			return std::make_unique<frw::UVProceduralNode>(m_params.scope.MaterialSystem(), frw::UVTypeValue::Project);

		default: assert(false);
	}

	return nullptr;
}

frw::Image MayaStandardNodeConverters::ProjectionNodeConverter::GetImageFromConnectedFileNode() const
{
	MPlug bumpValuePlug = m_params.shaderNode.findPlug("image");
	MPlugArray connections;
	bumpValuePlug.connectedTo(connections, true, false);

	if (connections.length() != 1)
	{
		return frw::Image();
	}

	std::vector<MPlug> dest;
	dest.resize(connections.length());
	connections.get(dest.data());

	MObject leftConnectedNode = dest.at(0).node();

	if (!leftConnectedNode.hasFn(MFn::kFileTexture))
	{
		return frw::Image();
	}

	MFnDependencyNode fileTextureNode(leftConnectedNode);

	MPlug texturePlug = fileTextureNode.findPlug("computedFileTextureNamePattern");
	MString texturePath = texturePlug.asString();

	MPlug colorSpacePlug = fileTextureNode.findPlug("colorSpace");
	MString colorSpace;
	if (!colorSpacePlug.isNull())
	{
		colorSpace = colorSpacePlug.asString();
	}

	return m_params.scope.GetImage(texturePath, colorSpace, fileTextureNode.name());
}
