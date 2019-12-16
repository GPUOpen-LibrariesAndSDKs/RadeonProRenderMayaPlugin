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
	const FireRenderMesh* pMesh = m_params.scope.GetCurrentlyParsedMesh();
	if (pMesh != nullptr)
	{
		uvIdx = pMesh->GetAssignedUVMapIdx(textureFile);
	}

	if (uvIdx == -1)
		uvIdx = 0;

	// create node
	auto value = m_params.scope.MaterialSystem().ValueLookupUV(uvIdx);

	// rotate frame
	auto rotateFrame = m_params.scope.GetValue(m_params.shaderNode.findPlug("rotateFrame"));
	if (rotateFrame != 0.)
		value = m_params.scope.MaterialSystem().ValueRotateXY(value - 0.5, rotateFrame) + 0.5;

	// translate frame
	auto translateFrameU = m_params.scope.GetValue(m_params.shaderNode.findPlug("translateFrameU"));
	auto translateFrameV = m_params.scope.GetValue(m_params.shaderNode.findPlug("translateFrameV"));
	if (translateFrameU != 0. || translateFrameV != 0.)
		value = value + m_params.scope.MaterialSystem().ValueCombine(-translateFrameU, -translateFrameV);

	// handle scale
	auto repeatU = m_params.scope.GetValue(m_params.shaderNode.findPlug("repeatU"));
	auto repeatV = m_params.scope.GetValue(m_params.shaderNode.findPlug("repeatV"));
	if (repeatU != 1. || repeatV != 1.)
		value = value * m_params.scope.MaterialSystem().ValueCombine(repeatU, repeatV);

	// handle offset
	auto offsetU = m_params.scope.GetValue(m_params.shaderNode.findPlug("offsetU"));
	auto offsetV = m_params.scope.GetValue(m_params.shaderNode.findPlug("offsetV"));
	if (offsetU != 0. || offsetV != 0.)
		value = value + m_params.scope.MaterialSystem().ValueCombine(offsetU, offsetV);

	// rotate
	auto rotateUV = m_params.scope.GetValue(m_params.shaderNode.findPlug("rotateUV"));
	if (rotateUV != 0.)
		value = m_params.scope.MaterialSystem().ValueRotateXY(value - 0.5, rotateUV) + 0.5;

	// TODO rotation, + "frame" settings

	return value;
}
