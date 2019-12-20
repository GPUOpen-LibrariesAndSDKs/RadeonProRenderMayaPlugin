#include "Bump2dConverter.h"
#include "FireMaya.h"
#include "FireRenderMath.h"
#include <maya/MPlugArray.h>

MayaStandardNodeConverters::Bump2dConverter::Bump2dConverter(const ConverterParams& params) : BaseConverter(params)
{
}

frw::Value MayaStandardNodeConverters::Bump2dConverter::Convert() const
{
	frw::Value bumpDepth = m_params.scope.GetValue(m_params.shaderNode.findPlug("bumpDepth"));
	UseAs useAs = static_cast<UseAs>(m_params.shaderNode.findPlug("bumpInterp").asInt());

	switch (useAs)
	{
		case UseAs::Bump: return GetBumpValue(bumpDepth);
		case UseAs::TangentSpaceNormals: return GetNormalValue(bumpDepth);
		case UseAs::ObjectSpaceNormals: return {};
		default: assert(false);
	}

	return frw::Value();
}

frw::Value MayaStandardNodeConverters::Bump2dConverter::GetBumpValue(const frw::Value& strength) const
{
	frw::Value color = m_params.scope.GetConnectedValue(m_params.shaderNode.findPlug("bumpValue"));

	if (!color)
	{
		return frw::Value();
	}

	frw::BumpMapNode imageNode(m_params.scope.MaterialSystem());
	imageNode.SetMap(color);

	if (strength != 1.0f)
	{
		imageNode.SetValue("bumpscale", strength);
	}

	return imageNode;
}


frw::Value MayaStandardNodeConverters::Bump2dConverter::GetNormalValue(const frw::Value& strength) const
{
	frw::Value color;

	// Normal node should expect float3 as input but this node could have only float as input
	// In maya, if this node connected directly to file node, visually there's connection between outAlpha and bumpValue
	// But under hood it retieves outColor from file node
	MPlug bumpValuePlug = m_params.shaderNode.findPlug("bumpValue");
	MPlugArray connections;
	bumpValuePlug.connectedTo(connections, true, false);

	if (connections.length() != 1)
	{
		return frw::Value();
	}

	std::vector<MPlug> dest;
	dest.resize(connections.length());
	connections.get(dest.data());

	if (dest.at(0).node().hasFn(MFn::kFileTexture))
	{
		// "oc" stands for "outColor"
		color = m_params.scope.GetValue(dest.at(0).node(), "oc");
	}

	if (!color)
	{
		return frw::Value();
	}

	frw::NormalMapNode imageNode(m_params.scope.MaterialSystem());
	imageNode.SetMap(color);

	if (strength != 1.0f)
	{
		imageNode.SetValue("bumpscale", strength);
	}

	return imageNode;
}
