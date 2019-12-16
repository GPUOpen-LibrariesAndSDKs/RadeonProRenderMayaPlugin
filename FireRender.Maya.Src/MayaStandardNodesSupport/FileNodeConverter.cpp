#include "FileNodeConverter.h"
#include "FireMaya.h"

MayaStandardNodeConverters::FileNodeConverter::FileNodeConverter(const ConverterParams& params) : BaseConverter(params)
{
}

frw::Value MayaStandardNodeConverters::FileNodeConverter::Convert() const
{
	MPlug texturePlug = m_params.shaderNode.findPlug("computedFileTextureNamePattern");
	MString texturePath = texturePlug.asString();

	MPlug colorSpacePlug = m_params.shaderNode.findPlug("colorSpace");
	MString colorSpace;
	if (!colorSpacePlug.isNull())
	{
		colorSpace = colorSpacePlug.asString();
	}

	frw::Image image = m_params.scope.GetImage(texturePath, colorSpace, m_params.shaderNode.name());
	if (!image.IsValid())
	{
		m_params.scope.SetIsLastPassTextureMissing(true);
		return nullptr;
	}

	frw::ImageNode imageNode(m_params.scope.MaterialSystem());
	image.SetGamma(ColorSpace2Gamma(colorSpace));
	imageNode.SetMap(image);

	frw::Value uvVal = m_params.scope.GetConnectedValue(m_params.shaderNode.findPlug("uvCoord"));
	if (uvVal.IsNull())
	{
		uvVal = m_params.scope.GetConnectedValue(m_params.shaderNode.findPlug("uCoord")); // uvCoord, uCoord and vCoord are different fields. Hovewer RPR_MATERIAL_NODE_IMAGE_TEXTURE have only "uv" input, thus cCoord is ignored
	}
	imageNode.SetValue("uv", uvVal);

	if (m_params.outPlugName == FireMaya::MAYA_FILE_NODE_OUTPUT_COLOR)
	{
		return imageNode;
	}
	else if (m_params.outPlugName == FireMaya::MAYA_FILE_NODE_OUTPUT_ALPHA)
	{
		MPlug alphaIsLuminancePlug = m_params.shaderNode.findPlug("alphaIsLuminance");
		bool alphaIsLuminance = alphaIsLuminancePlug.asBool();

		bool shouldUseAlphaIsLuminance = alphaIsLuminance || !image.HasAlphaChannel();
		if (shouldUseAlphaIsLuminance)
		{
			// Calculate alpha is luminance value
			return m_params.scope.MaterialSystem().ValueConvertToLuminance(imageNode);
		}
		else
		{
			// Select the real alpha value
			return frw::Value(imageNode).SelectW();
		}
	}

	bool isColorComponentRequested =
		m_params.outPlugName == FireMaya::MAYA_FILE_NODE_OUTPUT_COLOR_COMPONENT_RED ||
		m_params.outPlugName == FireMaya::MAYA_FILE_NODE_OUTPUT_COLOR_COMPONENT_GREEN ||
		m_params.outPlugName == FireMaya::MAYA_FILE_NODE_OUTPUT_COLOR_COMPONENT_BLUE;

	if (isColorComponentRequested)
	{
		// Component is selected later
		return imageNode;
	}

	m_params.scope.SetIsLastPassTextureMissing(true);
	return nullptr;
}

float MayaStandardNodeConverters::FileNodeConverter::ColorSpace2Gamma(const MString& colorSpace) const
{
	if (colorSpace == "sRGB")
		return 2.2f;

	else if (colorSpace == "camera Rec 709")
		return 2.2f;

	else if (colorSpace == "Raw")
		return 1.0f;

	else if (colorSpace == "gamma 1.8 Rec 709")
		return 1.8f;

	else if (colorSpace == "gamma 2.2 Rec 709")
		return 2.2f;

	else if (colorSpace == "gamma 2.4 Rec 709 (video)")
		return 2.4f;

	// default
	return 1.0f;
}
