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
#include "Place2dTextureConverter.h"
#include "FireMaya.h"

#include "maya/MItDependencyGraph.h"
#include "FireRenderObjects.h"

MayaStandardNodeConverters::Place2dTextureConverter::Place2dTextureConverter(const ConverterParams& params) : BaseConverter(params)
{
}

frw::Value MayaStandardNodeConverters::Place2dTextureConverter::Convert() const
{
	// get connected texture
	MString textureFile("");

	MStatus status;
	MObject shaderNodeObject = m_params.shaderNode.object();
	MItDependencyGraph it(shaderNodeObject,
		MFn::kFileTexture,
		MItDependencyGraph::kDownstream,
		MItDependencyGraph::kBreadthFirst,
		MItDependencyGraph::kNodeLevel,
		&status);
	for (; !it.isDone(); it.next())
	{
		MFnDependencyNode fileNode(it.currentItem());
		status = fileNode.findPlug("fileTextureName").getValue(textureFile);

		if (!status) // texture file node not found, continue go down the graph
			continue;

		// texture node found => exit
		break;
	}

	// get uv data
	int uvIdx = 0;
	const FireRenderMeshCommon* pMesh = m_params.scope.GetCurrentlyParsedMesh();
	if (pMesh != nullptr)
	{
		uvIdx = pMesh->GetAssignedUVMapIdx(textureFile);
	}

	if (uvIdx == -1)
		uvIdx = 0;

	// create node
	frw::Value value = m_params.scope.MaterialSystem().ValueLookupUV(uvIdx);

	// rotate frame
	frw::Value rotateFrame = m_params.scope.GetValue(m_params.shaderNode.findPlug("rotateFrame"));
	if (rotateFrame != 0.0f)
	{
		value = m_params.scope.MaterialSystem().ValueRotateXY(value - 0.5f, rotateFrame) + 0.5f;
	}

	// translate frame
	frw::Value translateFrameU = m_params.scope.GetValue(m_params.shaderNode.findPlug("translateFrameU"));
	frw::Value translateFrameV = m_params.scope.GetValue(m_params.shaderNode.findPlug("translateFrameV"));
	if (translateFrameU != 0.0f || translateFrameV != 0.0f)
	{
		value = value + m_params.scope.MaterialSystem().ValueCombine(-translateFrameU, -translateFrameV);
	}

	// handle scale
	frw::Value repeatU = m_params.scope.GetValue(m_params.shaderNode.findPlug("repeatU"));
	frw::Value repeatV = m_params.scope.GetValue(m_params.shaderNode.findPlug("repeatV"));
	if (repeatU != 1.0f || repeatV != 1.0f)
	{
		value = value * m_params.scope.MaterialSystem().ValueCombine(repeatU, repeatV);
	}

	// handle offset
	frw::Value offsetU = m_params.scope.GetValue(m_params.shaderNode.findPlug("offsetU"));
	frw::Value offsetV = m_params.scope.GetValue(m_params.shaderNode.findPlug("offsetV"));
	if (offsetU != 0.0f || offsetV != 0.0f)
	{
		value = value + m_params.scope.MaterialSystem().ValueCombine(offsetU, offsetV);
	}

	// rotate
	frw::Value rotateUV = m_params.scope.GetValue(m_params.shaderNode.findPlug("rotateUV"));
	if (rotateUV != 0.0f)
	{
		value = m_params.scope.MaterialSystem().ValueRotateXY(value - 0.5f, rotateUV) + 0.5f;
	}

	frw::Value coverageU = m_params.scope.GetValue(m_params.shaderNode.findPlug("coverageU"));
	frw::Value coverageV = m_params.scope.GetValue(m_params.shaderNode.findPlug("coverageV"));

	if (coverageU != 1.0f || coverageV != 1.0f)
	{
		value = value / m_params.scope.MaterialSystem().ValueCombine(coverageU, coverageV);
	}

	return value;
}
