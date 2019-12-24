#include "ProjectionNodeConverter.h"
#include "FireMaya.h"
#include "FireRenderObjects.h"
#include "maya/MFnTransform.h"
#include "Context/FireRenderContext.h"

MayaStandardNodeConverters::ProjectionNodeConverter::ProjectionNodeConverter(const ConverterParams& params) : BaseConverter(params)
{
}

frw::Value MayaStandardNodeConverters::ProjectionNodeConverter::Convert() const
{
	ProjectionType projectionType = static_cast<ProjectionType>(m_params.scope.GetValueWithCheck(m_params.shaderNode, "projType").GetInt());
	frw::Value image = m_params.scope.GetValueWithCheck(m_params.shaderNode, "image");
	frw::Value transparency = m_params.scope.GetValueWithCheck(m_params.shaderNode, "transparency");
	frw::Value uAngle = m_params.scope.GetValueWithCheck(m_params.shaderNode, "uAngle");
	frw::Value vAngle = m_params.scope.GetValueWithCheck(m_params.shaderNode, "vAngle");

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
		requiredRotation = GetMeshRotation(projectionType);
		requiredOrigin = GetMeshOrigin(projectionType);
		requiredScale = GetMeshScale(projectionType);
	}

	// Apply data to core node
	proceduralUVResult->SetOrigin(requiredOrigin);
	proceduralUVResult->SetInputUVScale(requiredScale);
	proceduralUVResult->SetInputZAxis(requiredRotation.first);
	proceduralUVResult->SetInputXAxis(requiredRotation.second);

	MString tempPath = L"C:/Users/Aleksandr/Pictures/CheckerTexture.png";
	frw::Image tempImage = m_params.scope.GetImage(tempPath, "sRGB", m_params.shaderNode.name());

	frw::ImageNode imageNode(m_params.scope.MaterialSystem());
	imageNode.SetValue("uv", *proceduralUVResult);
	imageNode.SetMap(tempImage);

	return imageNode;
}

frw::Value MayaStandardNodeConverters::ProjectionNodeConverter::GetMeshOrigin(ProjectionType projectionType) const
{
	if (projectionType == ProjectionType::TriPlanar)
	{
		const FireRenderMesh* mesh = m_params.scope.GetCurrentlyParsedMesh();
		MObject parent = MFnDagNode(mesh->Object()).parent(0);
		MFnTransform transform(parent);
		MVector translation = transform.translation(MSpace::kTransform);
		return { -translation.x / 100, -translation.y / 100, -translation.z / 100 };
	}
	else
	{
		return { 0.f, 0.f, 0.f };
	}
}

frw::Value MayaStandardNodeConverters::ProjectionNodeConverter::GetMeshScale(ProjectionType projectionType) const
{
	if (projectionType == ProjectionType::TriPlanar)
	{
		return { 50.f, 50.f, 50.f };
	}
	else
	{
		return { 50.f, 50.f, 50.f };
	}
}

std::pair<frw::Value, frw::Value> MayaStandardNodeConverters::ProjectionNodeConverter::GetMeshRotation(ProjectionType projectionType) const
{
	if (projectionType == ProjectionType::TriPlanar)
	{
		return {
			{ 0.f, 0.f, 1.f, 0.f },
			{ 1.f, 0.f, 0.f, 0.f }
		};
	}
	else
	{
		return {
			{ 0.f, 0.f, 1.f, 0.f },
			{ 1.f, 0.f, 0.f, 0.f }
		};
	}
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
	return { translationData.x, translationData.y, translationData.z };
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
	return { scaleData[0], scaleData[1], scaleData[2] };
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
