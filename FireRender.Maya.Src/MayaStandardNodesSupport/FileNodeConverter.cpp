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
#include "FileNodeConverter.h"
#include "FireMaya.h"
#include "../Context/TahoeContext.h"

#include <sstream>

MayaStandardNodeConverters::FileNodeConverter::FileNodeConverter(const ConverterParams& params) : BaseConverter(params)
{
}

void GetResolvedPatternStringArray(const MString& nodeName, MStringArray& resolvedNames)
{
	std::ostringstream ostream;
	ostream << "source \"rprCmdRenderUtils.mel\"; geFileNodeUDIMFiles(\"" << nodeName.asChar() << "\")";

	MStatus status = MGlobal::executeCommand(ostream.str().c_str(), resolvedNames);
}

void LoadAndAssignUdimImages(
	const MString& nodeName,  
	frw::Context context, 
	frw::Image masterImage, 
	const MString& udimPathPattern, 
	const MString& colorspace, 
	const FireMaya::Scope& scope)
{
	size_t index = std::string(udimPathPattern.asChar()).find("<UDIM>");
	const size_t tagLength = std::string("UDIM").length();

	if (index == std::string::npos)
	{
		MGlobal::displayWarning(MString("UDIM pattern is not recognized: ") + udimPathPattern);
		return;
	}

	MStringArray fileNames;
	GetResolvedPatternStringArray(nodeName, fileNames);

	for (size_t i = 0; i < fileNames.length(); ++i)
	{
		std::string fileName = fileNames[(unsigned int) i].asChar();

		rpr_uint tileIndex = std::stoi(fileName.substr(index, tagLength));

		frw::Image image = scope.GetImage(MString(fileName.c_str()), colorspace, MString());
		
		if (!image.IsValid())
		{
			frw::Image tile(context, fileName.c_str());
			tile.SetName(fileName.c_str());
			masterImage.SetUDIM(tileIndex, tile);

			continue;
		}

		masterImage.SetUDIM(tileIndex, image);
	}
}


frw::Value MayaStandardNodeConverters::FileNodeConverter::Convert() const
{
	MPlug texturePlug = m_params.shaderNode.findPlug("computedFileTextureNamePattern");
	MString texturePath = texturePlug.asString();

	MPlug uvTilingModePlug = m_params.shaderNode.findPlug("uvTilingMode");

	// This is index in FileNode for property "UV Tiling Mode". I haven't find this constant defined anywhere
	const int fileNodeUdimMode = 3;
	int uvMode = uvTilingModePlug.asInt();

	frw::Image image;
	MString colorSpace;

	MPlug colorSpacePlug = m_params.shaderNode.findPlug("colorSpace");
	if (!colorSpacePlug.isNull())
	{
		colorSpace = colorSpacePlug.asString();
	}

	if (fileNodeUdimMode != uvMode)
	{	
		bool useFrameExt = m_params.shaderNode.findPlug("useFrameExtension").asBool();

		if (useFrameExt)
		{
			MStringArray fileNames;
			GetResolvedPatternStringArray(m_params.shaderNode.name(), fileNames);

			if (fileNames.length() > 0)
			{
				texturePath = fileNames[0];
			}
		}	

		image = m_params.scope.GetImage(texturePath, colorSpace, m_params.shaderNode.name());
		if (!image.IsValid())
		{
			m_params.scope.SetIsLastPassTextureMissing(true);
			return nullptr;
		}
	}
	else
	{
		frw::Context context = m_params.scope.Context();
		rpr_image_format imageFormat = { 0, RPR_COMPONENT_TYPE_UINT8 };
		image = frw::Image(context, imageFormat);

		LoadAndAssignUdimImages(m_params.shaderNode.name(), context, image, texturePath, colorSpace, m_params.scope);
	}

	frw::ImageNode imageNode(m_params.scope.MaterialSystem());

	image.SetGamma(ColorSpace2Gamma(colorSpace));
	image.SetColorSpace(colorSpace.asChar()); // if RPR_CONTEXT_OCIO_CONFIG_PATH is invalid, this colorspace is automatically ignored and the gamma will be used.
	imageNode.SetMap(image);

	frw::Value uvVal = m_params.scope.GetConnectedValue(m_params.shaderNode.findPlug("uvCoord"));
	if (uvVal.IsNull())
	{
		uvVal = m_params.scope.GetConnectedValue(m_params.shaderNode.findPlug("uCoord")); // uvCoord, uCoord and vCoord are different fields. Hovewer RPR_MATERIAL_NODE_IMAGE_TEXTURE have only "uv" input, thus cCoord is ignored
	}
	imageNode.SetValue(RPR_MATERIAL_INPUT_UV, uvVal);

	if (m_params.outPlugName == FireMaya::MAYA_FILE_NODE_OUTPUT_COLOR)
	{
		return imageNode;
	}
	else if (m_params.outPlugName == FireMaya::MAYA_FILE_NODE_OUTPUT_TRANSPARENCY)
	{
		return 1.0 - frw::Value(imageNode).SelectW();
	}
	else if (m_params.outPlugName == FireMaya::MAYA_FILE_NODE_OUTPUT_ALPHA)
	{
		if (!m_params.scope.GetIContextInfo()->IsGLTFExport())
		{
			MPlug alphaIsLuminancePlug = m_params.shaderNode.findPlug("alphaIsLuminance");
			bool alphaIsLuminance = alphaIsLuminancePlug.asBool();

			bool imageHasAlpha = false;
			
			if (NorthStarContext::IsGivenContextNorthStar(dynamic_cast<const FireRenderContext*>(m_params.scope.GetIContextInfo())))
			{
				MPlug fileHasAlphaPlug = m_params.shaderNode.findPlug("fileHasAlpha");

				if (!fileHasAlphaPlug.isNull())
				{
					imageHasAlpha = fileHasAlphaPlug.asBool();
				}
			}
			else
			{
				// RPR1 case
				imageHasAlpha = image.HasAlphaChannel();
			}

			bool shouldUseAlphaIsLuminance = alphaIsLuminance || !imageHasAlpha;
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
		// In GLTF Export case we don't process outAlpha as in rendering mode because in this case Arithmetic nodes will be produced 
		// but Hybrid engine doesn't support them
		else
		{
			return imageNode;
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

float MayaStandardNodeConverters::FileNodeConverter::ColorSpace2Gamma(const MString& colorSpace)
{
	if ((colorSpace == "sRGB") || (colorSpace == "sRGB gamma"))
		return 2.2f;

	else if ((colorSpace == "camera Rec 709") || (colorSpace == "Rec 709 gamma"))
		return 2.2f;

	else if (colorSpace == "Raw")
		return 1.0f;

	else if ((colorSpace == "gamma 1.8 Rec 709") || (colorSpace == "1.8 gamma"))
		return 1.8f;

	else if ((colorSpace == "gamma 2.2 Rec 709") || (colorSpace == "2.2 gamma"))
		return 2.2f;

	else if (colorSpace == "gamma 2.4 Rec 709 (video)")
		return 2.4f;

	// default
	return 1.0f;
}
