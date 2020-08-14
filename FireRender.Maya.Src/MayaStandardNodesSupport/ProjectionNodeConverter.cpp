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
#include "ProjectionNodeConverter.h"
#include "FireMaya.h"
#include "FireRenderObjects.h"
#include "maya/MFnTransform.h"
#include "maya/MFnMatrixData.h"
#include "../Context/FireRenderContext.h"

MayaStandardNodeConverters::ProjectionNodeConverter::ProjectionNodeConverter(const ConverterParams& params) : BaseConverter(params)
{
}

frw::Value MayaStandardNodeConverters::ProjectionNodeConverter::Convert() const
{
	ProjectionType projectionType = static_cast<ProjectionType>(m_params.scope.GetValueWithCheck(m_params.shaderNode, "projType").GetInt());
	
	// Unused for now
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
		SubscribeCurrentMeshToCameraUpdates();
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
	imageNode.SetValue(RPR_MATERIAL_INPUT_UV, *proceduralUVResult);
	imageNode.SetMap(imageFromDirectlyConnectedFileNode);

	return imageNode;
}

MayaStandardNodeConverters::ProjectionNodeConverter::TransformInfo MayaStandardNodeConverters::ProjectionNodeConverter::GetCameraTransform() const
{
	MFnDagNode cameraDAGNode(GetConnectedCamera());
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
		{ -translation.x / (100.f * scale[0]), -translation.y / (100.f * scale[1]), -translation.z / (100.f * scale[2]) },
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

MObject MayaStandardNodeConverters::ProjectionNodeConverter::GetConnectedCamera() const
{
	MStatus status;

	// Retrieve selected camera from maya
	MPlug plug = m_params.shaderNode.findPlug("linkedCamera", &status);
	assert(status == MStatus::kSuccess);

	MPlugArray connections;
	plug.connectedTo(connections, true, false);
	if (connections.length() == 1)
	{
		return connections[0].node();
	}

	// If no camera specified return default camera
	M3dView view = M3dView::active3dView(&status);
	assert(status == MStatus::kSuccess);

	MDagPath cameraPath;
	status = view.getCamera(cameraPath);
	assert(status == MStatus::kSuccess);

	return cameraPath.node();
}

void MayaStandardNodeConverters::ProjectionNodeConverter::SubscribeCurrentMeshToCameraUpdates() const
{
	MFnDagNode cameraDAGNode(GetConnectedCamera());
	MObject transformNode = cameraDAGNode.parent(0);
	MFnTransform cameraTransform(transformNode);

	//    We have some architecture problems here, so we use const_cast to highlight them
	//
	// 1. GetCurrentlyParsedMesh() from Scope initially was used to retrieve mesh info inside shader parsing stage.
	//
	//    It used like that:
	//        m_currentlyParsedMesh = *some mesh*;
	//        ParseShader(...); // here GetCurrentlyParsedMesh() is valid
	//        m_currentlyParsedMesh = nullptr;
	//
	//    With that getter there's no need to pass current mesh to all the function stack from shader parsing.
	//    Shader shouldn't affect any specific mesh, therefore GetCurrentlyParsedMesh() was marked as const.
	//    We need more discussion about that method because we almost use global variable that's not really safe.
	//
	// 2. We should subscribe shader to camera updates in order to projection node in perspective mode work correctly.
	//    Currently there no shader class that could store callbacks from maya objects.
	//    For now all the callbacks could be only stored in mesh objects, or derivatives from FireRenderObject.
	//    So we need to add callback to currently parsed mesh, because there are no other ways to store it.
	//    Therefore const_cast was used to indicate the problem that we modify some mesh from almost global variable in shader parsing code.
	//    It would be good to have a shader class that could have stored callbacks.

	FireRenderMeshCommon* mesh = const_cast<FireRenderMeshCommon*>(m_params.scope.GetCurrentlyParsedMesh());

	// Swatch render returns nullptr for current mesh
	if (mesh != nullptr)
	{
		mesh->AddForceShaderDirtyDependOnOtherObjectCallback(transformNode);
	}
}
