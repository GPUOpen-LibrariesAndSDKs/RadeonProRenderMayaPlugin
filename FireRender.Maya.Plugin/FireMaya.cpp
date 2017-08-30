#include "FireMaya.h"
#include "FireRenderUtils.h"
#include "FireRenderIpr.h"
#include "common.h"
#include <maya/MImage.h>
#include <maya/MPlugArray.h>
#include <maya/MTextureManager.h>
#include <maya/MFileIO.h>
#include <maya/MSceneMessage.h>
#include "../ThirdParty/RadeonProRender SDK/Win/inc/Math/half.h"
#include "FireRenderThread.h"
#include "VRay.h"

#ifndef MAYA2015
#include "maya/MColorManagementUtilities.h"
#endif

#include "synColor/synColor.h"
#include "synColor/synColorInit.h"
#include "synColor/synColorFactory.h"
using namespace SYNCOLOR;

#include "OpenColorIO/OpenColorIO.h"
using namespace OpenColorIO;
using namespace OpenColorIO::v1;

#include "maya/MItDependencyNodes.h"

#pragma comment(linker, "/manifestDependency:\"name='ocio' processorArchitecture='AMD64' version='1.0.0.0' type='win32' \"")
#pragma comment(linker, "/manifestDependency:\"name='oiio' processorArchitecture='AMD64' version='1.0.0.0' type='win32' \"")
#pragma comment(linker, "/manifestDependency:\"name='synColorMgr' processorArchitecture='AMD64' version='1.0.0.0' type='win32' \"")

void FireMaya::Dump(const MFnDependencyNode& shader_node)
{
	DebugPrint("Attributes for %s node \"%s\" (TypeID: 0x%X)", shader_node.typeName().asUTF8(), shader_node.name().asUTF8(), shader_node.typeId().id());
	for (unsigned int i = 0; i < shader_node.attributeCount(); i++)
	{
		auto ob = shader_node.attribute(i);
		if (!ob.isNull() && ob.hasFn(MFn::kAttribute))
		{
			MFnAttribute attr(ob);
			if (attr.parent().isNull())	// top-level?
			{
				auto prefix = attr.isWritable() ? ">\t" : "\t";
				auto plug = shader_node.findPlug(ob);
				auto node = GetConnectedNode(plug);
				if (!node.isNull() && node.hasFn(MFn::kDependencyNode))
				{
					MFnDependencyNode dag(node);
					DebugPrint("%s%s -> Connected to \"%s\"", prefix, attr.name().asUTF8(), dag.name().asUTF8());
				}
				else
				{
					DebugPrint("%s%s", prefix, attr.name().asUTF8());
				}
			}
			else
				DebugPrint("\t-\t%s", attr.name().asUTF8());
		}
	}
}

MPlug FireMaya::GetConnectedPlug(const MPlug& plug)
{
	if (!plug.isNull())
	{
		MPlugArray connections;
		plug.connectedTo(connections, true, false);
		if (connections.length() > 0)
		{
			if (!connections[0].node().hasFn(MFn::kAnimCurve))
				return connections[0];
		}
	}
	return MPlug();
}

MObject FireMaya::GetConnectedNode(const MPlug& plug)
{
	auto to = GetConnectedPlug(plug);
	if (!to.isNull())
		return to.node();

	return MObject();
}

MString FireMaya::GetBasePath()
{
#ifdef Win32_
	static wchar_t buf[MAX_PATH + 1] = {};
#else
	static wchar_t buf[260 + 1] = {}; //260
#endif
	if (!buf[0])
	{
		MString cmd = "pluginInfo -query -path " + MString(FIRE_RENDER_FILE_NAME);
		auto mstr = MGlobal::executeCommandStringResult(cmd);
		if (mstr.length() > 0)
			wcscpy(buf, mstr.asWChar());
		else
			wcscpy(buf, L".");

		for (int i = 0; i < 3; i++)	// step it back 3 slashes
		{
			if (auto slash = wcsrchr(buf, '/'))
				*slash = 0;
		}
	}
	return buf;
}

MString FireMaya::GetIconPath()
{
	static MString path = GetBasePath() + "/icons";
	return path;
}

MString FireMaya::GetShaderPath()
{
	static MString path = GetBasePath() + "/shaders";
	return path;
}

void convertColorSpace(MString colorSpace, rpr_image_format format, rpr_image_desc img_desc, std::vector<unsigned char> & buffer)
{
#ifndef MAYA2015
	if (MColorManagementUtilities::isColorManagementAvailable() && MColorManagementUtilities::isColorManagementEnabled())
	{
		bool bOcioEnabled = false;
		MString ocioConfigFilePath;

		// Before we start work with OCIO set the "OCIO" environment variable to the .ocio config file path the user set in Maya 2017, without that OCIO won't work at all
		MItDependencyNodes it(MFn::kColorMgtGlobals);
		
		if(!it.isDone())
		{
			MObject node = it.item();

			MFnDependencyNode vraySettings(node);
		
			MPlug p0 = vraySettings.findPlug("configFileEnabled");
			MPlug p1 = vraySettings.findPlug("configFilePath");
		
			if (!p0.isNull())
				bOcioEnabled = p0.asBool();
		
			if (!p1.isNull())
				ocioConfigFilePath = p1.asString();
		}

		if (bOcioEnabled)
		{
			if (format.type == RPR_COMPONENT_TYPE_UINT8)
			{
				SetEnv("OCIO", ocioConfigFilePath.asUTF8());

				// Convert to normalized floats
				std::vector<float> pixels;
				for (unsigned char& pixel : buffer)
				{
					pixels.push_back(pixel / 255.f);
				}

				try
				{
					ConstConfigRcPtr config = GetCurrentConfig();
					ConstProcessorRcPtr processor = config->getProcessor(colorSpace.asUTF8(), "role_scene_linear");

					PackedImageDesc img(pixels.data(), img_desc.image_width, img_desc.image_height, format.num_components);
					processor->apply(img);

					// Convert back to u8
					for (int i = 0; i < pixels.size(); ++i)
					{
						buffer[i] = pixels[i] * 255.f;
					}
				}
				catch (Exception& e)
				{
					MString what = e.what();
					MGlobal::displayError("OpenColorIO Error: " + what);
				}
			}
		}
		else
		{
#ifdef MAYA2017
			using namespace std;

			MString inputId;
			MStatus status = MColorManagementUtilities::getColorTransformCacheIdForInputSpace(colorSpace, inputId);
			if (status.error())
				throw logic_error(status.errorString().asUTF8());

			MColorManagementUtilities::MColorTransformData data;

			auto inputIdStart = strstr((const char*)data.getData(), inputId.asUTF8());

			assert(inputIdStart != nullptr);
			if (inputIdStart == nullptr)
				return;

			auto xmlStart = strstr(inputIdStart, "<?xml");
			assert(xmlStart != nullptr);
			if (xmlStart == nullptr)
				return;

			string xmlData(xmlStart);
			{
				auto start = xmlData.c_str();
				auto end = strstr(start, "</ProcessListEntry>");
				auto len = end - start;
				xmlData = xmlData.substr(0, len);
			}

			SynStatus sc_status;
			TransformPtr loadedTransformPtr;

			sc_status = loadColorTransformFromBuffer(xmlData.c_str(), loadedTransformPtr);
			if (!sc_status)
				throw std::logic_error(sc_status.getErrorMessage());

			PixelFormat pf;
			switch (format.type)
			{
			case RPR_COMPONENT_TYPE_FLOAT16:
				pf = format.num_components == 3 ? PixelFormat::PF_RGB_16f : PixelFormat::PF_RGBA_16f;
				break;
			case RPR_COMPONENT_TYPE_FLOAT32:
				pf = format.num_components == 3 ? PixelFormat::PF_RGB_32f : PixelFormat::PF_RGBA_32f;
				break;
			case RPR_COMPONENT_TYPE_UINT8:
			default:
				pf = format.num_components == 3 ? PixelFormat::PF_RGB_8i : PixelFormat::PF_RGBA_8i;
				break;
			}
			vector<unsigned char> converted(buffer.size());
			auto src = buffer.data();
			auto dst = converted.data();
			ROI roi(img_desc.image_width, img_desc.image_height);
			TransformPtr finalizedTransformPtr;
			sc_status = finalize(loadedTransformPtr, pf, pf, OptimizerFlags::OPTIMIZER_LOSSLESS, ResolveFlags::RESOLVE_GRAPHICS_MONITOR, finalizedTransformPtr);
			if (!sc_status)
				throw std::logic_error(sc_status.getErrorMessage());

			sc_status = loadedTransformPtr->applyCPU(src, roi, dst);
			if (!sc_status)
				throw std::logic_error(sc_status.getErrorMessage());

			buffer = converted;
#endif
		}
	}
#endif
}

frw::Image FireMaya::Scope::GetImage(MString texturePath, MString colorSpace)
{
	std::string key = texturePath.asUTF8();
	key += colorSpace.asUTF8();

	if (key.length() == 0) {
		return NULL;
	}

	auto it = m->imageCache.find(key);
	if (it != m->imageCache.end())
		return it->second;

	return FireRenderThread::RunOnMainThread<frw::Image>([this, texturePath, key, colorSpace]() -> frw::Image
	{
		MAIN_THREAD_ONLY; // MTextureManager will not work in other threads
		DebugPrint("Loading Image: %s in colorSpace: %s", texturePath.asUTF8(), colorSpace.asUTF8());

		MString extension;
		const char* filePath = texturePath.asUTF8();
		int length = texturePath.length();
		extension += &filePath[length - 3];
		
		frw::Image image;
		//image = frw::Image(m->context, texturePath.asUTF8());
		if(extension == "hdr" || extension == "exr")
			image = frw::Image(m->context, texturePath.asUTF8());
		else
		//if(!image)
		{
			if (auto renderer = MHWRender::MRenderer::theRenderer())
			{
				if (auto textureManager = renderer->getTextureManager())
				{
					try
					{
						if (auto texture = textureManager->acquireTexture(texturePath))
						{
							MHWRender::MTextureDescription desc = {};
							texture->textureDescription(desc);
							int slicePitch = 0;
							int rowPitch = 0;
							if (const auto rawData_to_free = texture->rawData(rowPitch, slicePitch))
							{
								auto srcData = rawData_to_free;
								auto width = desc.fWidth;
								auto height = desc.fHeight;

								auto channels = 1;
								auto componentSize = 1;

								switch (desc.fFormat)
								{
									//case MHWRender::kD24S8: break;
									//case MHWRender::kD24X8: break;
									//case MHWRender::kD32_FLOAT: break;
									//case MHWRender::kR24G8: break;
									//case MHWRender::kR24X8: break;
									//case MHWRender::kDXT1_UNORM: break;
									//case MHWRender::kDXT1_UNORM_SRGB: break;
									//case MHWRender::kDXT2_UNORM: break;
									//case MHWRender::kDXT2_UNORM_SRGB: break;
									//case MHWRender::kDXT2_UNORM_PREALPHA: break;
									//case MHWRender::kDXT3_UNORM: break;
									//case MHWRender::kDXT3_UNORM_SRGB: break;
									//case MHWRender::kDXT3_UNORM_PREALPHA: break;
									//case MHWRender::kDXT4_UNORM: break;
									//case MHWRender::kDXT4_SNORM: break;
									//case MHWRender::kDXT5_UNORM: break;
									//case MHWRender::kDXT5_SNORM: break;
									//case MHWRender::kR9G9B9E5_FLOAT: break;
									//case MHWRender::kR1_UNORM: break;
								case MHWRender::kA8:
								case MHWRender::kR8_UNORM:
									//case MHWRender::kR8_SNORM: break;
								case MHWRender::kR8_UINT:
									//case MHWRender::kR8_SINT: break;
								case MHWRender::kL8:
									channels = 1;
									componentSize = 1;
									break;

								case MHWRender::kR16_FLOAT:
									channels = 1;
									componentSize = 2;
									break;
									//case MHWRender::kR16_UNORM: break;
									//case MHWRender::kR16_SNORM: break;
									//case MHWRender::kR16_UINT: break;
									//case MHWRender::kR16_SINT: break;
									//case MHWRender::kL16: break;
									//case MHWRender::kR8G8_UNORM: break;
									//case MHWRender::kR8G8_SNORM: break;
									//case MHWRender::kR8G8_UINT: break;
									//case MHWRender::kR8G8_SINT: break;
									//case MHWRender::kB5G5R5A1: break;
									//case MHWRender::kB5G6R5: break;
								case MHWRender::kR32_FLOAT:
									channels = 1;
									componentSize = 4;
									break;
									//case MHWRender::kR32_UINT: break;
									//case MHWRender::kR32_SINT: break;
									//case MHWRender::kR16G16_FLOAT: break;
									//case MHWRender::kR16G16_UNORM: break;
									//case MHWRender::kR16G16_SNORM: break;
									//case MHWRender::kR16G16_UINT: break;
									//case MHWRender::kR16G16_SINT: break;
								case MHWRender::kR8G8B8A8_UNORM:
									//case MHWRender::kR8G8B8A8_SNORM: break;
								case MHWRender::kR8G8B8A8_UINT:
									channels = 4;
									componentSize = 1;
									break;
									//case MHWRender::kR8G8B8A8_SINT: break;
									//case MHWRender::kR10G10B10A2_UNORM: break;
									//case MHWRender::kR10G10B10A2_UINT: break;
									//case MHWRender::kB8G8R8A8: break;
									//case MHWRender::kB8G8R8X8: break;
								case MHWRender::kR8G8B8X8:
									channels = 4;
									componentSize = 1;
									break;
									//case MHWRender::kA8B8G8R8: break;
									//case MHWRender::kR32G32_FLOAT: break;
									//case MHWRender::kR32G32_UINT: break;
									//case MHWRender::kR32G32_SINT: break;
								case MHWRender::kR16G16B16A16_FLOAT:
									channels = 4;
									componentSize = 2;
									break;
									//case MHWRender::kR16G16B16A16_UNORM: break;
									//case MHWRender::kR16G16B16A16_SNORM: break;
									//case MHWRender::kR16G16B16A16_UINT: break;
									//case MHWRender::kR16G16B16A16_SINT: break;
								case MHWRender::kR32G32B32_FLOAT:
									channels = 3;
									componentSize = 4;
									break;
									//case MHWRender::kR32G32B32_UINT: break;
									//case MHWRender::kR32G32B32_SINT: break;
								case MHWRender::kR32G32B32A32_FLOAT:
									channels = 3;
									componentSize = 4;
									break;
									//case MHWRender::kR32G32B32A32_UINT: break;
									//case MHWRender::kR32G32B32A32_SINT: break;
								default:
									ErrorPrint("Unsupported MRasterFormat format: %d", desc.fFormat);
#ifndef MAYA2015
									texture->freeRawData(rawData_to_free);
#else
									free(rawData_to_free);
#endif
									textureManager->releaseTexture(texture);
									throw;
									break;
								}

								int srcRowPitch = desc.fBytesPerRow;

								std::vector<float> tempBuffer;
								if (componentSize == 2)	// we don't support half size floats at the moment!
								{
									tempBuffer.resize(width * height * channels);
									for (auto y = 0u; y < height; y++)
									{
										auto src = static_cast<const half *>(srcData) + y * (width * channels);
										auto dst = tempBuffer.data() + y * (width * channels);
										for (auto x = 0u; x < width * channels; x++)
										{
											dst[x] = src[x];
										}
									}

									componentSize = 4;
									srcData = tempBuffer.data();
									srcRowPitch = width * channels * componentSize;
								}

								rpr_image_format format = {};
								format.num_components = channels >= 3 ? 3 : 1;
								format.type =
									(componentSize == 4) ? RPR_COMPONENT_TYPE_FLOAT32 :
									(componentSize == 2) ? RPR_COMPONENT_TYPE_FLOAT16 :
									RPR_COMPONENT_TYPE_UINT8;

								int srcPixSize = componentSize * channels;
								int dstPixSize = componentSize * format.num_components;

								rpr_image_desc img_desc = {};
								img_desc.image_width = width;
								img_desc.image_height = height;
								img_desc.image_row_pitch = width * dstPixSize;

								std::vector<unsigned char> buffer(height * img_desc.image_row_pitch);

								for (unsigned int y = 0; y < height; y++)
								{
									auto src = static_cast<const char*>(srcData) + y * srcRowPitch;
									auto dst = buffer.data() + y * img_desc.image_row_pitch;
									for (unsigned int x = 0; x < width; x++)
										memcpy(dst + x * dstPixSize, src + x * srcPixSize, dstPixSize);
								}

								convertColorSpace(colorSpace, format, img_desc, buffer);
								image = frw::Image(m->context, format, img_desc, buffer.data());
#ifndef MAYA2015
								texture->freeRawData(rawData_to_free);
#else
								free(rawData_to_free);
#endif
							}
							textureManager->releaseTexture(texture);
						}
					}
					catch (...)
					{
						ErrorPrint("Error Loading Image: %s", texturePath.asUTF8());
					}
				}
			}
		}

		if (image)
			m->imageCache[key] = image;
		else
			return frw::Image(m->context, 0.5, 0.5, 1);

		return image;
	});
}

template <class T>
const T& Clamp(const T& v, const T& mn, const T& mx)
{
	if (v < mn)
		return mn;
	if (mx < v)
		return mx;
	return v;
}

frw::Image FireMaya::Scope::GetAdjustedImage(MString texturePath,
	int viewWidth, int viewHeight,
	FitType imgFit, double contrast, double brightness,
	FitType filmFit, double overScan,
	double imgSizeX, double imgSizeY,
	double apertureSizeX, double apertureSizeY,
	double offsetX, double offsetY,
	bool ignoreColorSpaceFileRules, MString colorSpace)
{
	MStatus status;
	char key[1024] = {};
	int written = snprintf(key, 1024, "%s-%d-%d-%d-%f-%f-%d-%f-%f-%f-%f-%f-%f-%f-%s-%s",
		texturePath.asUTF8(),
		viewWidth, viewHeight,
		imgFit, contrast, brightness,
		filmFit, overScan,
		imgSizeX, imgSizeY,
		apertureSizeX, apertureSizeY,
		offsetX, offsetY,
		ignoreColorSpaceFileRules ? "true" : "false", colorSpace.asUTF8());
	assert(written >= 0 && written < 1024);

	auto it = m->imageCache.find(key);
	if (it != m->imageCache.end())
	{
		DebugPrint("Using cached image from imageCache...");
		return it->second;
	}

	return FireRenderThread::RunOnMainThread<frw::Image>([&]()
	{
		MAIN_THREAD_ONLY; // MTextureManager will not work in other threads

		frw::Image image;
		if (auto renderer = MHWRender::MRenderer::theRenderer())
		{
			if (auto textureManager = renderer->getTextureManager())
			{
				if (auto texture = textureManager->acquireTexture(texturePath))
				{
					MHWRender::MTextureDescription desc = {};
					texture->textureDescription(desc);

					int slicePitch = 0;
					int rowPitch = 0;
					if (auto rawData = texture->rawData(rowPitch, slicePitch))
					{
						auto pixels = static_cast<const unsigned char*>(rawData);
						auto srcPixSize = desc.fBytesPerRow / desc.fWidth;

						int componentSize = srcPixSize > 4 ? 4 : 1;

						rpr_image_format format = {};
						format.num_components = srcPixSize / componentSize > 3 ? 3 : 1;
						format.type = (componentSize == 4) ? RPR_COMPONENT_TYPE_FLOAT32 : RPR_COMPONENT_TYPE_UINT8;

						int dstPixSize = componentSize * format.num_components;

						rpr_image_desc img_desc = {};
						img_desc.image_width = viewWidth * 2;
						img_desc.image_height = viewHeight * 2;
						img_desc.image_row_pitch = img_desc.image_width * dstPixSize;

						std::vector<unsigned char> buffer(img_desc.image_height * img_desc.image_row_pitch);

						double srcAspect = double(desc.fWidth) / double(desc.fHeight);
						double dstAspect = double(img_desc.image_width) / double(img_desc.image_height);


						double fitX = 0, fitY = 0;
						//lets get the image size for film size (viewport - gate size):
						if (filmFit == FitHorizontal) {
							//base is: width(x):
							fitX = viewWidth / overScan;
							fitY = fitX * apertureSizeY / apertureSizeX;
						}
						else if (filmFit == FitVertical) {
							//base is: height(y):
							fitY = viewHeight / overScan;
							fitX = fitY * apertureSizeX / apertureSizeY;
						}

						//decide fill and fit:
						if (imgFit == FitFill) {
							imgFit = (desc.fWidth >= desc.fHeight) ? FitHorizontal : FitVertical;
						}
						else if (imgFit == FitBest) {
							imgFit = (desc.fHeight > desc.fWidth) ? FitHorizontal : FitVertical;
						}


						double initImgSizeX = 0, initImgSizeY = 0;
						//calculate initial image size, based on fitting and original image ratio:
						if (imgFit == FitHorizontal) {
							//base is width/X:
							initImgSizeX = fitX;
							initImgSizeY = initImgSizeX * double(desc.fHeight) / double(desc.fWidth);
						}
						else if (imgFit == FitVertical) {
							//base is height/Y:
							initImgSizeY = fitY;
							initImgSizeX = initImgSizeY * double(desc.fWidth) / double(desc.fHeight);
						}
						else if (imgFit == FitStretch) {
							initImgSizeX = fitX;
							initImgSizeY = fitY;
						}


						double scaledImgSizeX = 0, scaledImgSizeY = 0;
						//calculate scaled image size based on the aperture vs image size ratio:
						if (imgFit == FitHorizontal) {
							//base is: width(x):
							scaledImgSizeX = initImgSizeX / (apertureSizeX / imgSizeX);
							scaledImgSizeY = initImgSizeY / (apertureSizeX / imgSizeX);
						}
						else if (imgFit == FitVertical) {
							//base is: width(x):
							scaledImgSizeX = initImgSizeX / (apertureSizeY / imgSizeY);
							scaledImgSizeY = initImgSizeY / (apertureSizeY / imgSizeY);
						}
						else if (imgFit == FitStretch) {
							scaledImgSizeX = initImgSizeX / (apertureSizeX / imgSizeX);
							scaledImgSizeY = initImgSizeY / (apertureSizeY / imgSizeY);
						}

						double defaultOffsetX = (scaledImgSizeX - viewWidth) / scaledImgSizeX / 2.0;
						double defaultOffsetY = (scaledImgSizeY - viewHeight) / scaledImgSizeY / 2.0;

						double defaultScaleX = (double)viewWidth / scaledImgSizeX;
						double defaultScaleY = (double)viewHeight / scaledImgSizeY;

						double scale[] = { defaultScaleX, defaultScaleY };
						double offset[] = { defaultOffsetX, defaultOffsetY };

						if (imgFit == FireMaya::FitHorizontal) {
							offset[0] -= offsetX * (1.0 / imgSizeX);
							offset[1] += offsetY * (1.0 / imgSizeX) * (double(desc.fWidth) / double(desc.fHeight));
						}
						else if (imgFit == FireMaya::FitVertical) {
							offset[0] -= offsetX * (1.0 / imgSizeY) / (double(desc.fWidth) / double(desc.fHeight));
							offset[1] += offsetY * (1.0 / imgSizeY);
						}
						else if (imgFit == FireMaya::FitStretch) {
							offset[0] -= offsetX * (1.0 / imgSizeX);
							offset[1] += offsetY * (1.0 / imgSizeY);
						}

						for (unsigned int y = 0; y < img_desc.image_height - 2; y++)
						{
							auto dst = buffer.data() + y * img_desc.image_row_pitch;
							double src_yF = (double(y) / img_desc.image_height * scale[1] + offset[1]) * desc.fHeight; // -0.xxx is truncated to 0 which is not less than 0...
							int src_y = static_cast<int>(src_yF);
							if (src_yF < 0 || src_yF >= desc.fHeight)
								memset(dst, 0, dstPixSize * img_desc.image_width);
							else
							{
								auto src = pixels + src_y * desc.fBytesPerRow;
								for (unsigned int x = 0; x < img_desc.image_width - 2; x++)
								{
									double src_xF = (double(x) / img_desc.image_width * scale[0] + offset[0]) * desc.fWidth;
									int src_x = static_cast<int>(src_xF);
									if (src_xF < 0 || src_xF >= desc.fWidth)
										memset(dst + x * dstPixSize, 0, dstPixSize);
									else
									{
										auto sp = src + src_x * srcPixSize;
										auto dp = dst + x * dstPixSize;

										if (format.num_components == 3)
										{
											switch (format.type)
											{
											case RPR_COMPONENT_TYPE_UINT8:
												dp[0] = Clamp(int(sp[0] * contrast + brightness * 255), 0, 255);
												dp[1] = Clamp(int(sp[1] * contrast + brightness * 255), 0, 255);
												dp[2] = Clamp(int(sp[2] * contrast + brightness * 255), 0, 255);
												break;
											case RPR_COMPONENT_TYPE_FLOAT32:
											{
												auto _dp = reinterpret_cast<float*>(dp);
												auto _sp = reinterpret_cast<const float*>(sp);
												_dp[0] = _sp[0] * float(contrast) + float(brightness);
												_dp[1] = _sp[1] * float(contrast) + float(brightness);
												_dp[2] = _sp[2] * float(contrast) + float(brightness);
											}	break;
											default:
												memcpy(dp, sp, dstPixSize);
												break;
											}
										}
										else
										{
											switch (format.type)
											{
											case RPR_COMPONENT_TYPE_UINT8:
												dp[0] = Clamp(int(sp[0] * contrast + brightness * 255), 0, 255);
												break;
											case RPR_COMPONENT_TYPE_FLOAT32:
											{
												auto _dp = reinterpret_cast<float*>(dp);
												auto _sp = reinterpret_cast<const float*>(sp);
												_dp[0] = _sp[0] * float(contrast) + float(brightness);
											}	break;
											default:
												memcpy(dp, sp, dstPixSize);
												break;
											}
										}
									}
								}
							}
						}

						convertColorSpace(colorSpace, format, img_desc, buffer);

						image = frw::Image(m->context, format, img_desc, buffer.data());
#ifndef MAYA2015
						texture->freeRawData(rawData);
#else
						free(rawData);
#endif
					}
					textureManager->releaseTexture(texture);
					}
				else
				{
					LogPrint("Unable to load image %s", texturePath.asUTF8());
				}
				}
			}

		if (image)
			m->imageCache[key] = image;
		else
			return frw::Image(m->context, 0.5, 0.5, 1);

		return image;
		});
	}


frw::Value FireMaya::Scope::convertMayaAddDoubleLinear(const MFnDependencyNode &node)
{
	auto input1 = GetValueWithCheck(node, "input1");
	auto input2 = GetValueWithCheck(node, "input2");
	return m->materialSystem.ValueAdd(input1, input2);
}


frw::Value FireMaya::Scope::convertMayaBlendColors(const MFnDependencyNode &node)
{
	auto color1 = GetValueWithCheck(node, "color1");
	auto color2 = GetValueWithCheck(node, "color2");
	auto blender = GetValueWithCheck(node, "blender");

	return m->materialSystem.ValueBlend(color1, color2, blender);
}


double calcContrast(double value, double bias, double contrast)
{
	double res = 0;
	double factor = 1.0;
	bool inv = value > bias;

	if (inv)
	{
		factor = bias / 0.25 * 0.5;
		value = 1 - value;
		bias = 1 - bias;
	}

	res = pow(bias * pow((value / bias), contrast * factor), log(0.5) / log(bias));
	return inv ? 1 - res : res;
}

frw::Value FireMaya::Scope::convertMayaContrast(const MFnDependencyNode &node)
{
	auto value = GetValueWithCheck(node, "value");
	auto bias = GetValueWithCheck(node, "bias");
	auto contrast = GetValueWithCheck(node, "contrast");

	// renderPassMode

	auto x = calcContrast(value.GetX(), bias.GetX(), contrast.GetX());
	auto y = calcContrast(value.GetY(), bias.GetY(), contrast.GetY());
	auto z = calcContrast(value.GetZ(), bias.GetZ(), contrast.GetZ());

	return frw::Value(x, y, z);
}

frw::Value FireMaya::Scope::convertMayaMultDoubleLinear(const MFnDependencyNode &node)
{
	auto input1 = GetValueWithCheck(node, "input1");
	auto input2 = GetValueWithCheck(node, "input2");
	return m->materialSystem.ValueMul(input1, input2);
}

frw::Value FireMaya::Scope::convertMayaMultiplyDivide(const MFnDependencyNode &node)
{
	enum
	{
		OpNone = 0,
		OpMul,
		OpDiv,
		OpPow
	};

	auto input1 = GetValueWithCheck(node, "input1");
	auto input2 = GetValueWithCheck(node, "input2");
	auto operation = GetValueWithCheck(node, "operation");
	int op = operation.GetInt();
	switch (op)
	{
	case OpMul: return m->materialSystem.ValueMul(input1, input2);
	case OpDiv: return m->materialSystem.ValueDiv(input1, input2);
	case OpPow: return m->materialSystem.ValuePow(input1, input2);
	}

	return input1;
}

void FireMaya::Scope::GetArrayOfValues(const MFnDependencyNode &node, const char * plugName, ArrayOfValues &arr)
{
	MStatus status;
	MPlug plug = node.findPlug(plugName, &status);
	assert(status == MStatus::kSuccess);

	if (plug.isNull())
		return;

	for (unsigned int i = 0; i < plug.numElements(); i++)
	{
		MPlug elementPlug = plug[i];
		frw::Value val = GetValue(elementPlug);
		arr.push_back(val);
	}
}

frw::Value FireMaya::Scope::calcElements(const ArrayOfValues &arr, bool substract)
{
	if (arr.size() == 0)
		return frw::Value(0.0);

	frw::Value res = arr[0];

	frw::MaterialSystem materialSystem = m->materialSystem;

	if (substract)
	{
		for (size_t i = 1; i < arr.size(); i++)
			res = materialSystem.ValueSub(res, arr[i]);
	}
	else
	{
		for (size_t i = 1; i < arr.size(); i++)
			res = materialSystem.ValueAdd(res, arr[i]);
	}

	return res;
}

frw::Value FireMaya::Scope::convertMayaPlusMinusAverage(const MFnDependencyNode &node, const MString &outPlugName)
{
	enum
	{
		OpNone = 0,
		OpSum,
		OpSubtract,
		OpAverage,
	};

	const char * name = "input1D";
	if (outPlugName.indexW("o2"_ms) == 0)
		name = "input2D";
	if (outPlugName.indexW("o3"_ms) == 0)
		name = "input3D";

	ArrayOfValues arr;
	GetArrayOfValues(node, name, arr);
	if (arr.size() == 0)
		return frw::Value(0.0);

	auto operation = GetValueWithCheck(node, "operation");
	int op = operation.GetInt();
	frw::Value res;

	switch (op)
	{
	case OpNone:		res = arr[0]; break;
	case OpSum:			res = calcElements(arr); break;
	case OpSubtract:	res = calcElements(arr, true); break;
	case OpAverage:		res = m->materialSystem.ValueDiv(calcElements(arr), frw::Value((double)arr.size())); break;
	}

	return res;
}

frw::Value FireMaya::Scope::convertMayaVectorProduct(const MFnDependencyNode &node)
{
	enum
	{
		OpNone = 0,
		OpDotProduct,
		OpCrossProduct,
		OpVectorMatrixProduct, // Outvector = Input x Matrix
		OpPointMatrixProduct, // Output = Input x Matrix
	};

	auto input1 = GetValueWithCheck(node, "input1");
	auto input2 = GetValueWithCheck(node, "input2");
	auto matrix = GetValueWithCheck(node, "matrix");
	auto operation = GetValueWithCheck(node, "operation");
	int op = operation.GetInt();

	auto res = input1;
	switch (op)
	{
	case OpDotProduct: res = m->materialSystem.ValueDot(input1, input2); break;
	case OpCrossProduct: res = m->materialSystem.ValueCross(input1, input2); break;
	}

	auto normalizeOutput = GetValueWithCheck(node, "normalizeOutput");
	if (normalizeOutput.GetBool())
		return m->materialSystem.ValueNormalize(res);

	return res;
}

frw::Value FireMaya::Scope::ParseValue(MObject node, const MString &outPlugName)
{
	if (node.isNull())
		return nullptr;

	frw::MaterialSystem materialSystem = m->materialSystem;

	MFnDependencyNode shaderNode(node);

	auto id = MayaValueId(shaderNode.typeId().id());
	auto name = shaderNode.typeName();

	// handle native fire render value nodes
	if (auto valueNode = dynamic_cast<FireMaya::ValueNode*>(shaderNode.userNode()))
		return valueNode->GetValue(*this);

	// interpret Maya nodes
	switch (id)
	{
	case MayaAddDoubleLinear:	return convertMayaAddDoubleLinear(shaderNode);
	case MayaBlendColors:		return convertMayaBlendColors(shaderNode);
	case MayaContrast:			return convertMayaContrast(shaderNode);
	case MayaMultDoubleLinear:	return convertMayaMultDoubleLinear(shaderNode);
	case MayaMultiplyDivide:	return convertMayaMultiplyDivide(shaderNode);
	case MayaPlusMinusAverage:	return convertMayaPlusMinusAverage(shaderNode, outPlugName);
	case MayaVectorProduct:		return convertMayaVectorProduct(shaderNode);

	case MayaNoise:
		return convertMayaNoise(shaderNode);

	case MayaBump2d:
		return convertMayaBump2d(shaderNode);

	case MayaNodePlace2dTexture:
	{
		auto value = materialSystem.ValueLookupUV(0);

		// rotate frame
		auto rotateFrame = GetValue(shaderNode.findPlug("rotateFrame"));
		if (rotateFrame != 0.)
			value = materialSystem.ValueRotateXY(value - 0.5, rotateFrame) + 0.5;

		// translate frame
		auto translateFrameU = GetValue(shaderNode.findPlug("translateFrameU"));
		auto translateFrameV = GetValue(shaderNode.findPlug("translateFrameV"));
		if (translateFrameU != 0. || translateFrameV != 0.)
			value = value + materialSystem.ValueCombine(-translateFrameU, -translateFrameV);

		// handle scale
		auto repeatU = GetValue(shaderNode.findPlug("repeatU"));
		auto repeatV = GetValue(shaderNode.findPlug("repeatV"));
		if (repeatU != 1. || repeatV != 1.)
			value = value * materialSystem.ValueCombine(repeatU, repeatV);

		// handle offset
		auto offsetU = GetValue(shaderNode.findPlug("offsetU"));
		auto offsetV = GetValue(shaderNode.findPlug("offsetV"));
		if (offsetU != 0. || offsetV != 0.)
			value = value + materialSystem.ValueCombine(offsetU, offsetV);

		// rotate
		auto rotateUV = GetValue(shaderNode.findPlug("rotateUV"));
		if (rotateUV != 0.)
			value = materialSystem.ValueRotateXY(value - 0.5, rotateUV) + 0.5;

		// TODO rotation, + "frame" settings

		return value;
	}break;

	case MayaNodeFile:
	{
		MPlug texturePlug = shaderNode.findPlug("computedFileTextureNamePattern");
		MString texturePath = texturePlug.asString();
		MPlug colorSpacePlug = shaderNode.findPlug("colorSpace");
		MString colorSpace;
		if (!colorSpacePlug.isNull())
			colorSpace = colorSpacePlug.asString();
		if (auto image = GetImage(texturePath, colorSpace))
		{
			frw::ImageNode imageNode(materialSystem);
			imageNode.SetMap(image);
			imageNode.SetValue("uv", GetConnectedValue(shaderNode.findPlug("uvCoord")));
			return imageNode;
		}
	}break;

	case MayaNodeChecker:
	{
		frw::ValueNode map(materialSystem, frw::ValueTypeCheckerMap);
		auto uv = (GetConnectedValue(shaderNode.findPlug("uvCoord")) | materialSystem.ValueLookupUV(0));
		map.SetValue("uv", (uv * .25) + 128.);	// <- offset added because FR mirrors checker at origin

		frw::Value color1 = GetValue(shaderNode.findPlug("color1"));
		frw::Value color2 = GetValue(shaderNode.findPlug("color2"));
		frw::Value contrast = GetValue(shaderNode.findPlug("contrast"));

		return materialSystem.ValueBlend(color2, color1, map); // Reverse blend to match tile alignment
	}

	default:
		if (FireMaya::TypeId::IsValidId(id))
			DebugPrint("Error: Unhandled FireMaya NodeId: %X", id);
		else if (FireMaya::TypeId::IsReservedId(id))
			DebugPrint("Error: Unrecognized FireMaya NodeId: %X", id);
		else
		{
			DebugPrint("Warning: Unhandled or Unknown Maya Node: %X", id);
			// I don't think we need this: Dump(shaderNode);
		}
		return createImageFromShaderNode(node);

	case MayaNodeRamp:
		//falling back onto rasterization:
		return createImageFromShaderNode(node, 128, 128);

		break;
	}

	return nullptr;
}

frw::Value FireMaya::Scope::createImageFromShaderNode(MObject node, int width, int height)
{
	return FireRenderThread::RunOnMainThread<frw::Value>([&]()
	{
		frw::Value ret;

		MAIN_THREAD_ONLY; // MTextureManager will not work in other threads
		if (auto renderer = MHWRender::MRenderer::theRenderer())
		{
			if (auto textureManager = renderer->getTextureManager())
			{
				MFnDependencyNode shaderNode(node);
				MPlug outColorPlug = shaderNode.findPlug("outColor");
				if (auto texture = textureManager->acquireTexture("", outColorPlug, width, height, false))
				{
					std::vector<unsigned char> buffer(width * height * 3, 128);

					int slicePitch = 0;
					int rowPitch = 0;
					if (auto pixelData = static_cast<const unsigned char*>(texture->rawData(rowPitch, slicePitch)))
					{
						for (int v = 0; v < height; v++)
						{
							auto src = pixelData + v * rowPitch;
							auto dst = buffer.data() + (height - v - 1) * width * 3;
							for (int u = 0; u < width; u++)
							{
								*(dst++) = *(src++);
								*(dst++) = *(src++);
								*(dst++) = *(src++);
								src++;
							}
						}

						rpr_image_desc img_desc = {};
						img_desc.image_width = width;
						img_desc.image_height = height;
						img_desc.image_row_pitch = width * 3;

						frw::Image image(m->context, { 3, RPR_COMPONENT_TYPE_UINT8 }, img_desc, buffer.data());

						frw::ImageNode imageNode(m->materialSystem);
						imageNode.SetMap(image);
						//we are not setting UV input because it is already baked into image.

						ret = imageNode;
#ifndef MAYA2015
						texture->freeRawData((void*)pixelData);
#else
						free((void*)pixelData);
#endif
					}
					textureManager->releaseTexture(texture);
					}
				}
			}

		if (!ret)
			DebugPrint("WARNING: Can't create image from ShaderNode");

		return ret;
		});
	}

frw::Value FireMaya::Scope::CosinePowerToRoughness(const frw::Value &power)
{
	frw::MaterialSystem materialSystem = m->materialSystem;

	// power -  The valid range is 2 to infinity. The default value is 20.
	auto val = materialSystem.ValueDiv(power, frw::Value(100.0f));

	auto normVal = materialSystem.ValueClamp(val);

	auto rough = materialSystem.ValueSub(1.0, normVal);
	rough = materialSystem.ValuePow(rough, 3);

	// clamp for identity Maya spot size
	rough = materialSystem.ValueMul(rough, 0.45f);	// spot max size
	return materialSystem.ValueAdd(rough, 0.01f);		// spot min size
}


frw::Shader FireMaya::Scope::convertMayaBlinn(const MFnDependencyNode &node)
{
	auto color = GetValueWithCheck(node, "color");
	auto diffuse = GetValueWithCheck(node, "diffuse");
	auto specularColor = GetValueWithCheck(node, "specularColor");
	auto eccentricity = GetValueWithCheck(node, "eccentricity");
	auto specularRollOff = GetValueWithCheck(node, "specularRollOff");

	frw::MaterialSystem materialSystem = m->materialSystem;

	frw::Shader result(materialSystem, frw::ShaderTypeDiffuse);

	auto res = materialSystem.ValueMul(color, diffuse);
	result.SetValue("color", res);

	frw::Shader shader(materialSystem, frw::ShaderTypeMicrofacet);

	shader.SetValue("color", specularColor);

	auto val = materialSystem.ValuePow(eccentricity, frw::Value(0.5));
	val = materialSystem.ValueSub(val, frw::Value(0.1));
	val = materialSystem.ValueMax(val, frw::Value(0.0));
	val = materialSystem.ValueMul(val, 0.45f);

	shader.SetValue("roughness", val);
	shader.SetValue("ior", frw::Value(1.0f));

	auto blendVal = materialSystem.ValueMul(specularRollOff, specularRollOff);
	blendVal = materialSystem.ValueMul(blendVal, frw::Value(0.15));
	blendVal = materialSystem.ValueAdd(blendVal, frw::Value(0.002));

	return materialSystem.ShaderBlend(result, shader, blendVal);
}


frw::Shader FireMaya::Scope::convertMayaPhong(const MFnDependencyNode &node)
{
	auto color = GetValueWithCheck(node, "color");
	auto diffuse = GetValueWithCheck(node, "diffuse");
	auto cosinePower = GetValueWithCheck(node, "cosinePower");
	auto specularColor = GetValueWithCheck(node, "specularColor");

	frw::MaterialSystem materialSystem = m->materialSystem;

	frw::Shader result(materialSystem, frw::ShaderTypeDiffuse);

	auto res = materialSystem.ValueMul(color, diffuse);
	result.SetValue("color", res);

	frw::Shader shader(materialSystem, frw::ShaderTypeMicrofacet);

	shader.SetValue("color", specularColor);

	auto val = CosinePowerToRoughness(cosinePower);
	shader.SetValue("roughness", val);
	shader.SetValue("ior", frw::Value(1.0f));

	return materialSystem.ShaderBlend(result, shader, frw::Value(0.35, 0.35, 0.35));
}


frw::Shader FireMaya::Scope::convertMayaPhongE(const MFnDependencyNode &node)
{
	auto color = GetValueWithCheck(node, "color");
	auto diffuse = GetValueWithCheck(node, "diffuse");
	auto roughness = GetValueWithCheck(node, "roughness");
	auto whiteness = GetValueWithCheck(node, "whiteness");
	auto highlightSize = GetValueWithCheck(node, "highlightSize");
	auto specularColor = GetValueWithCheck(node, "specularColor");

	frw::MaterialSystem materialSystem = m->materialSystem;

	frw::Shader result(materialSystem, frw::ShaderTypeDiffuse);

	auto res = materialSystem.ValueMul(color, diffuse);
	result.SetValue("color", res);

	frw::Shader shader(materialSystem, frw::ShaderTypeMicrofacet);

	auto secondColor = materialSystem.ValueMul(specularColor, whiteness);
	specularColor = materialSystem.ValueMix(specularColor, secondColor, frw::Value(0.4));

	shader.SetValue("color", specularColor);

	auto val = materialSystem.ValueMul(roughness, highlightSize);
	val = materialSystem.ValueMul(val, 0.5f);

	shader.SetValue("roughness", val);
	shader.SetValue("ior", frw::Value(1.0f));

	auto blendVal = materialSystem.ValueMul(roughness, frw::Value(0.3));
	blendVal = materialSystem.ValueAdd(blendVal, frw::Value(0.01));

	return materialSystem.ShaderBlend(result, shader, blendVal);
}


frw::Value FireMaya::Scope::convertMayaNoise(const MFnDependencyNode &node)
{
	auto uv = GetValueWithCheck(node, "uvCoord");

	auto threshold = GetValueWithCheck(node, "threshold");
	auto amplitude = GetValueWithCheck(node, "amplitude");
	auto ratio = GetValueWithCheck(node, "ratio");
	auto frequencyRatio = GetValueWithCheck(node, "frequencyRatio");
	auto depthMax = GetValueWithCheck(node, "depthMax");
	auto time = GetValueWithCheck(node, "time");
	auto frequency = GetValueWithCheck(node, "frequency");
	// Implode - not implemented
	// Implode Center - not implemented

	frw::Value minScale(0.225f);

	frw::MaterialSystem materialSystem = m->materialSystem;

	// calc uv1
	auto scaleUV1 = materialSystem.ValueMul(frequency, minScale);
	auto uv1 = materialSystem.ValueMul(uv, scaleUV1);

	// calc uv2
	auto freqSq = materialSystem.ValuePow(frequencyRatio, frw::Value(1.4f));
	auto freq = materialSystem.ValueMul(frequency, freqSq);
	auto scaleUV2 = materialSystem.ValueMul(freq, minScale);

	auto dephVal = materialSystem.ValueSub(depthMax, frw::Value(1.0));
	dephVal = materialSystem.ValueDiv(dephVal, frw::Value(2.0));
	scaleUV2 = materialSystem.ValueMul(scaleUV2, dephVal);

	auto uv2 = materialSystem.ValueMul(uv, scaleUV2);

	auto scaledTime = materialSystem.ValueMul(time, frw::Value(0.5));
	auto offset1 = materialSystem.ValueMul(scaleUV1, scaledTime);
	auto offset2 = materialSystem.ValueMul(scaleUV2, scaledTime);

	frw::ValueNode node1(materialSystem, frw::ValueTypeNoiseMap);
	uv1 = materialSystem.ValueAdd(uv1, offset1);
	node1.SetValue("uv", uv1);

	frw::ValueNode node2(materialSystem, frw::ValueTypeNoiseMap);
	uv2 = materialSystem.ValueSub(uv2, offset2);
	node2.SetValue("uv", uv2);

	auto mulNoise = materialSystem.ValueMul(node1, node2);
	auto v = materialSystem.ValueBlend(node1, mulNoise, ratio);


	auto amplScaled = materialSystem.ValueMul(amplitude, frw::Value(0.5));
	auto th = materialSystem.ValueAdd(threshold, frw::Value(0.5));

	auto min = materialSystem.ValueSub(th, amplScaled);
	auto max = materialSystem.ValueAdd(th, amplScaled);

	frw::Value thresholdInv = materialSystem.ValueSub(frw::Value(1.0), threshold);
	frw::Value v1 = materialSystem.ValueMin(v, thresholdInv);

	return materialSystem.ValueBlend(min, max, v1);
}

frw::Value FireMaya::Scope::convertMayaBump2d(const MFnDependencyNode& shaderNode)
{
	if (auto bumpValue = GetConnectedValue(shaderNode.findPlug("bumpValue")))
	{
		// TODO: need to find a way to turn generic input color data
		// into bump/normal format
	}

	return nullptr;
}

frw::Shader FireMaya::Scope::ParseShader(MObject node)
{
	if (node.isNull())
		return nullptr;

	frw::Shader result;

	MFnDependencyNode shaderNode(node);
	frw::MaterialSystem materialSystem = m->materialSystem;

	if (auto n = dynamic_cast<FireMaya::ShaderNode*>(shaderNode.userNode()))
	{
		result = n->GetShader(*this);
	}
	else
	{
		// interpret Maya nodes (complex materials, not utilities and maps)
		MTypeId typeId = shaderNode.typeId();
		auto typeIdUI = typeId.id();
		auto id = MayaSurfaceId(shaderNode.typeId().id());
		auto name = shaderNode.typeName();

		DebugPrint("Shader: %s [0x%x]", name.asUTF8(), id);

		switch (id)
		{
		case MayaNodeLambert:
		{
			frw::DiffuseShader diffuseShader(materialSystem);
			frw::Value color = GetValue(shaderNode.findPlug("color"));
			color = color * GetValue(shaderNode.findPlug("diffuse"), 1.);
			diffuseShader.SetColor(color);
			result = diffuseShader;
		} break;

		case MayaNodeBlinn:
			result = convertMayaBlinn(shaderNode);
			break;

		case MayaNodePhong:
			result = convertMayaPhong(shaderNode);
			break;

		case MayaNodePhongE:
			result = convertMayaPhongE(shaderNode);
			break;

		case MayaNodeAnisotropic: {
			///
			frw::DiffuseShader diffuseShader(materialSystem);

			auto color = GetValue(shaderNode.findPlug("color"));
			auto diffuse = GetValue(shaderNode.findPlug("diffuse"));
			auto diffColor = materialSystem.ValueMul(color, diffuse);
			diffuseShader.SetColor(diffColor);

			///
			frw::Shader wardShader(materialSystem, frw::ShaderTypeWard);

			frw::Value roughness = GetValue(shaderNode.findPlug("roughness"));
			frw::Value fresnel = GetValue(shaderNode.findPlug("fresnelReflectiveIndex"));
			frw::Value specularColor = GetValue(shaderNode.findPlug("specularColor"));
			frw::Value angle = GetValue(shaderNode.findPlug("angle"));

			frw::Value mayaSpreadX = GetValue(shaderNode.findPlug("spreadX"));
			frw::Value mayaSpreadY = GetValue(shaderNode.findPlug("spreadY"));

			//mapping equations from Maya spread and roughness to RPR spread:
			if (mayaSpreadX.GetX() > 98.0) {
				mayaSpreadX = 98.0;
			}
			if (mayaSpreadY.GetX() > 98.0) {
				mayaSpreadY = 98.0;
			}

			frw::Value spreadX = materialSystem.ValueMul(roughness, 1.0 - mayaSpreadX / 100.0);
			frw::Value spreadY = materialSystem.ValueMul(roughness, 1.0 - mayaSpreadY / 100.0);

			wardShader.SetValue("color", specularColor);
			double rads = -1.0 * angle.GetX() * M_PI / 180.0;
			frw::Value radians = frw::Value(rads, rads, rads);
			wardShader.SetValue("rotation", radians);

			wardShader.SetValue("roughness_x", spreadX);
			wardShader.SetValue("roughness_y", spreadY);

			result = materialSystem.ShaderBlend(diffuseShader, wardShader, 0.5);
		} break;
		case MayaLayeredShader:
			// TODO
			break;
		default:
			break;
		}
	}

	if (result)	// we have a valid shader?
	{
		// applies to ALL surface nodes with the transparency attribute
		auto transparency = GetValue(shaderNode.findPlug("transparency"));
		if (transparency.NonZeroXYZ())
		{
			frw::TransparentShader transShader(materialSystem);
			result = materialSystem.ShaderBlend(result, transShader, transparency);
		}

		if (auto normals = GetConnectedValue(shaderNode.findPlug("normalCamera")))
		{
			result.SetValue("normal", normals);
		}
	}
	else if (auto value = GetValue(node)) // else try getting a simple value instead (and use it as a diffuse color)
	{
		frw::DiffuseShader diffuseShader(materialSystem);
		diffuseShader.SetColor(value);
		result = diffuseShader;
	}

	return result;
}

frw::Shader FireMaya::Scope::ParseVolumeShader(MObject node)
{
	if (node.isNull())
		return nullptr;

	MFnDependencyNode shaderNode(node);

	if (auto n = dynamic_cast<FireMaya::ShaderNode*>(shaderNode.userNode()))
	{
		return n->GetVolumeShader(*this);
	}
	else
	{
		// interpret Maya nodes (complex materials, not utilities and maps)
		// TODO
		frw::Shader result;
		// interpret Maya nodes (complex materials, not utilities and maps)
		MTypeId typeId = shaderNode.typeId();
		unsigned int typeIdUI = typeId.id();
		auto id = MayaSurfaceId(shaderNode.typeId().id());
		auto name = shaderNode.typeName();
	}
	return frw::Shader();
}


void FireMaya::Scope::RegisterCallback(MObject node)
{
	m->callbackId.push_back(MNodeMessage::addNodeDirtyCallback(node, NodeDirtyCallback, this));
}

void FireMaya::Scope::NodeDirtyCallback(MObject& ob)
{
	try
	{
		MFnDependencyNode node(ob);
		DebugPrint("Callback: %s dirty", node.typeName().asUTF8());

		auto shaderId = getNodeUUid(ob);
		if (auto shader = GetCachedShader(shaderId)) {
			shader.SetDirty();
		}
		if (auto shader = GetCachedVolumeShader(shaderId)) {
			shader.SetDirty();
		}
	}
	catch (const std::exception & ex)
	{
		DebugPrint("NodeDirtyCallback -> %s", ex.what());
		throw;
	}
	catch (...)
	{
		DebugPrint("NodeDirtyCallback ... unknown exception");
		throw;
	}
}

void FireMaya::Scope::NodeDirtyCallback(MObject& node, void* clientData)
{
	auto self = static_cast<Scope*>(clientData);
	self->NodeDirtyCallback(node);
}


void FireMaya::Scope::NodeDirtyPlugCallback(MObject& ob, MPlug& plug)
{
	MFnDependencyNode node(ob);
	DebugPrint("Callback: %s plug %s dirty", node.typeName().asUTF8(), plug.name().asUTF8());
}

void FireMaya::Scope::NodeDirtyPlugCallback(MObject& node, MPlug& plug, void* clientData)
{
	auto self = static_cast<Scope*>(clientData);
	self->NodeDirtyPlugCallback(node, plug);
}

frw::Shader FireMaya::Scope::GetCachedShader(const NodeId& id) const
{
	auto it = m->shaderMap.find(id);
	if (it != m->shaderMap.end())
		return it->second;
	return frw::Shader();
}

frw::Shader FireMaya::Scope::GetCachedVolumeShader(const NodeId& id) const
{
	auto it = m->volumeShaderMap.find(id);
	if (it != m->volumeShaderMap.end())
		return it->second;
	return frw::Shader();
}

void FireMaya::Scope::SetCachedShader(const NodeId& id, frw::Shader shader)
{
	if (!shader)
		m->shaderMap.erase(id);
	else
		m->shaderMap[id] = shader;
}

void FireMaya::Scope::SetCachedVolumeShader(const NodeId& id, frw::Shader shader)
{
	if (!shader)
		m->volumeShaderMap.erase(id);
	else
		m->volumeShaderMap[id] = shader;
}

frw::Value FireMaya::Scope::GetCachedValue(const NodeId& id) const
{
	auto it = m->valueMap.find(id);
	if (it != m->valueMap.end())
		return it->second;
	return nullptr;
}

void FireMaya::Scope::SetCachedValue(const NodeId& id, frw::Value v)
{
	if (!v)
		m->valueMap.erase(id);
	else
		m->valueMap[id] = v;
}

FireMaya::Scope::Scope()
{
}

FireMaya::Scope::~Scope()
{
	FireRenderThread::RunOnceProcAndWait([this]() { Reset(); });
}


frw::Shader FireMaya::Scope::GetShader(MObject node, bool forceUpdate)
{
	if (node.isNull())
		return frw::Shader();

	auto shaderId = getNodeUUid(node);
	auto shader = GetCachedShader(shaderId);
	if (forceUpdate || !shader || shader.IsDirty())
	{
		if (!shader)	// register callbacks if one doesn't already exist
			RegisterCallback(node);

		DebugPrint("Parsing shader: %s (forceUpdate=%d, shader.IsDirty()=%d)", shaderId.c_str(), forceUpdate, shader.IsDirty());
		// create now
		shader = ParseShader(node);
		if (shader)
			SetCachedShader(shaderId, shader);
	}
	return shader;
}

frw::Shader FireMaya::Scope::GetVolumeShader(MObject node, bool forceUpdate)
{
	if (node.isNull())
		return frw::Shader();

	auto shaderId = getNodeUUid(node);
	auto shader = GetCachedVolumeShader(shaderId);
	if (forceUpdate || !shader || shader.IsDirty())
	{
		if (!shader)	// register callbacks if one doesn't already exist
			RegisterCallback(node);

		// create now
		shader = ParseVolumeShader(node);
		if (shader)
			SetCachedVolumeShader(shaderId, shader);
	}
	return shader;
}

frw::Value FireMaya::Scope::GetValue(MObject ob, const MString &outPlugName)
{
	return ParseValue(ob, outPlugName);
}

frw::Value FireMaya::Scope::GetConnectedValue(const MPlug& plug)
{
	auto p = GetConnectedPlug(plug);
	if (!p.isNull())
	{
		auto node = p.node();
		if (!node.isNull())
		{
			if (auto ret = GetValue(node, p.partialName()))
			{
				if (p.isChild())	// we are selecting a channel!
				{
					auto parent = p.parent();
					int idx = -1;
					for (unsigned int i = 0; i < parent.numChildren(); i++)
					{
						if (parent.child(i) == p)
						{
							idx = (int)i;
							break;
						}
					}
					switch (idx)
					{
					case 0:
						ret = ret.SelectX();
						break;
					case 1:
						ret = ret.SelectY();
						break;
					case 2:
						ret = ret.SelectZ();
						break;
					default:
						DebugPrint("Error: invalid source index %d for connected node");
						break;
					}
				}
				return ret;
			}
		}
	}

	return nullptr;
}

frw::Value FireMaya::Scope::GetValue(const MPlug& plug)
{
	if (plug.isNull())
		return nullptr;

	if (auto value = GetConnectedValue(plug))
		return value;

	// a simple color or vector value
	if (plug.numChildren() > 0)
	{
		frw::Value f[3];

		for (unsigned int i = 0; i < plug.numChildren() && i < 3; i++)
		{
			const auto& child = plug.child(i);
			f[i] = GetConnectedValue(child);
			if (f[i].IsNull())
				f[i] = child.asFloat();
		}

		return MaterialSystem().ValueCombine(f[0], f[1], f[2]);
	}

	// a scalar value
	float v;
	plug.getValue(v);
	return frw::Value(v);
}

frw::Value FireMaya::Scope::GetValueWithCheck(const MFnDependencyNode &node, const char * plugName)
{
	MStatus status;
	MPlug plug = node.findPlug(plugName, &status);

	assert(status == MStatus::kSuccess);
	return GetValue(plug);
}

frw::Shader FireMaya::Scope::GetShader(MPlug plug)
{
	if (!plug.isNull())
	{
		MPlug connectedPlug;
		if (getInputColorConnection(plug, connectedPlug))
		{
			auto connectedPlugNode = connectedPlug.node();
			if (!connectedPlugNode.isNull())
				return GetShader(connectedPlugNode);
		}
	}

	return frw::Shader();
}

frw::Shader FireMaya::Scope::GetVolumeShader(MPlug plug)
{
	if (!plug.isNull())
	{
		MPlug connectedPlug;
		if (getInputColorConnection(plug, connectedPlug))
		{
			auto connectedPlugNode = connectedPlug.node();
			if (!connectedPlugNode.isNull())
				return GetVolumeShader(connectedPlugNode);
		}
	}

	return frw::Shader();
}

FireMaya::Scope::Data::Data()
{
}

FireMaya::Scope::Data::~Data()
{
	RPR_THREAD_ONLY;

	for (auto it : callbackId)
		MNodeMessage::removeCallback(it);

	scene.Reset();
	materialSystem.Reset();
	context.Reset();

	// everything else destroyed automatically
}

void FireMaya::Scope::Reset()
{
	m.reset();
}

void FireMaya::Scope::Init(rpr_context handle, bool destroyMaterialSystemOnDelete)
{
	RPR_THREAD_ONLY;

	m = std::make_shared<Data>();

	m->context = frw::Context(handle);
	m->materialSystem = frw::MaterialSystem(m->context, nullptr, destroyMaterialSystemOnDelete);
	m->scene = m->context.CreateScene();
	m->scene.SetActive();
}

void FireMaya::Node::postConstructor()
{
	// TODO: may be put 'setMPSafe(true);' here? This method is called from all overridden postConstructor functions.
	if (MFileIO::isReadingFile())
	{
		// Register callbacks to be executed after file loading
		openCallback = MSceneMessage::addCallback(MSceneMessage::Message::kAfterOpen, onFileOpen, this);
		importCallback = MSceneMessage::addCallback(MSceneMessage::Message::kAfterImport, onFileOpen, this);
	}
}

/*static*/ void FireMaya::Node::onFileOpen(void* clientData)
{
	FireMaya::Node* node = (FireMaya::Node*)clientData;
	// Remove callbacks
	MSceneMessage::removeCallback(node->openCallback);
	MSceneMessage::removeCallback(node->importCallback);
	// Execute upgrade code
	node->OnFileLoaded();
}

void FireMaya::Node::OnFileLoaded()
{
}

void FireMaya::Node::ConvertFilenameToFileInput(MObject filenameAttr, MObject inputAttr)
{
	MFnDependencyNode shaderNode(thisMObject());
	MPlug texturePlug = shaderNode.findPlug(filenameAttr);
	MPlug colorPlug = shaderNode.findPlug(inputAttr);

	MString nodeName = shaderNode.name();
	MString texturePath = texturePlug.asString();

	if (texturePath.length())
	{
		// should create and attach texture file node
		MString command =
			R"(
string $node = `shadingNode -asTexture -name texture_$parent file`;
setAttr ($node+".fileTextureName") -type "string" "$filename";
connectAttr -f ($node+".outColor") "$inputAttr";
string $placeNode = `shadingNode -asUtility place2dTexture -name place_$parent`;
connectAttr -f ($placeNode+".coverage") ($node+".coverage");
connectAttr -f ($placeNode+".translateFrame") ($node+".translateFrame");
connectAttr -f ($placeNode+".rotateFrame") ($node+".rotateFrame");
connectAttr -f ($placeNode+".stagger") ($node+".stagger");
connectAttr -f ($placeNode+".wrapU") ($node+".wrapU");
connectAttr -f ($placeNode+".wrapV") ($node+".wrapV");
connectAttr -f ($placeNode+".repeatUV") ($node+".repeatUV");
connectAttr -f ($placeNode+".offset") ($node+".offset");
connectAttr -f ($placeNode+".rotateUV") ($node+".rotateUV");
connectAttr -f ($placeNode+".mirrorU") ($node+".mirrorU");
connectAttr -f ($placeNode+".mirrorV") ($node+".mirrorV");
connectAttr -f ($placeNode+".outUV") ($node+".uv");
connectAttr -f ($placeNode+".outUvFilterSize") ($node+".uvFilterSize");
)";
		command.substitute("$filename", texturePath);
		command.substitute("$parent", nodeName);
		command.substitute("$inputAttr", colorPlug.name());

		MGlobal::executeCommand(command);
	}
}
