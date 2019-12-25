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
	std::pair<frw::Value, frw::Value> requiredRotation;
	frw::Value requiredOrigin;
	frw::Value requiredScale;
	if (projectionType == ProjectionType::Perspective)
	{
		requiredRotation = GetCameraRotation(projectionType);
		requiredOrigin = GetCameraOrigin(projectionType);
		requiredScale = GetCameraScale(projectionType);
	}
	else
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

		// Apply data
		requiredRotation = {{ matrix[2][0], matrix[2][1], matrix[2][2], matrix[2][3] }, { matrix[0][0], matrix[0][1], matrix[0][2], matrix[0][3] }};
		requiredOrigin = { -translation.x / (100.f * scale[0]), -translation.y / (100.f * scale[1]), -translation.z / (100.f * scale[2]) };
		requiredScale = { scale[0] * 50.f, scale[1] * 50.f, scale[2] * 50.f };
	}

	// Apply data to core node
	proceduralUVResult->SetOrigin(requiredOrigin);
	proceduralUVResult->SetInputUVScale(requiredScale);
	proceduralUVResult->SetInputZAxis(requiredRotation.first);
	proceduralUVResult->SetInputXAxis(requiredRotation.second);

	frw::ImageNode imageNode(m_params.scope.MaterialSystem());
	imageNode.SetValue("uv", *proceduralUVResult);
	imageNode.SetMap(imageFromDirectlyConnectedFileNode);

	return imageNode;
}

frw::Value MayaStandardNodeConverters::ProjectionNodeConverter::GetCameraOrigin(ProjectionType projectionType) const
{
	const IFireRenderContextInfo* contextInfo = m_params.scope.GetContextInfo();
	const FireRenderContext* context = dynamic_cast<const FireRenderContext*>(contextInfo);
	const FireRenderCamera& camera = context->camera();

	MFnDagNode cameraDAGNode(camera.Object());
	MObject transformNode = cameraDAGNode.parent(0);
	MFnTransform cameraTransform(transformNode);

	MVector translationData = cameraTransform.getTranslation(MSpace::kTransform);
	return { translationData.x / 100.f, translationData.y / 100.f, translationData.z / 100.f };
}

frw::Value MayaStandardNodeConverters::ProjectionNodeConverter::GetCameraScale(ProjectionType projectionType) const
{
	const IFireRenderContextInfo* contextInfo = m_params.scope.GetContextInfo();
	const FireRenderContext* context = dynamic_cast<const FireRenderContext*>(contextInfo);
	const FireRenderCamera& camera = context->camera();

	MFnDagNode cameraDAGNode(camera.Object());
	MObject transformNode = cameraDAGNode.parent(0);
	MFnTransform cameraTransform(transformNode);

	double scaleData[3];
	cameraTransform.getScale(scaleData);
	return { 50, 50, 50 };
}

std::pair<frw::Value, frw::Value> MayaStandardNodeConverters::ProjectionNodeConverter::GetCameraRotation(ProjectionType projectionType) const
{
	const IFireRenderContextInfo* contextInfo = m_params.scope.GetContextInfo();
	const FireRenderContext* context = dynamic_cast<const FireRenderContext*>(contextInfo);
	const FireRenderCamera& camera = context->camera();

	MFnDagNode cameraDAGNode(camera.Object());
	MObject transformNode = cameraDAGNode.parent(0);
	MFnTransform cameraTransform(transformNode);

	bool isValid = transformNode.hasFn(MFn::kTransform);
	bool isValid2 = camera.Object().hasFn(MFn::kTransform);

	MMatrix transformMatrix = cameraTransform.transformationMatrix();
	double transformData[4][4];
	transformMatrix.get(transformData);

	return {
		{ transformData[2][0], transformData[2][1], transformData[2][2], transformData[2][3] },
		{ transformData[0][0], transformData[0][1], transformData[0][2], transformData[0][3] }
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
