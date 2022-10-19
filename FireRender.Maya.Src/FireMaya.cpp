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
#include "FireMaya.h"
#include "common.h"
#include "FireRenderThread.h"
#include "VRay.h"
#include "Context/FireRenderContext.h"
#include "MayaStandardNodesSupport/NodeConverterUtil.h"

#include <maya/MImage.h>
#include <maya/MPlugArray.h>
#include <maya/MTextureManager.h>
#include <maya/MFileIO.h>
#include <maya/MSceneMessage.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MUuid.h>
#include <maya/MItDependencyGraph.h>
#include <maya/MImageFileInfo.h>
#include <FireRenderLayeredTextureUtils.h>
#include <exception>

#ifdef MAYA2017
#include "maya/MColorManagementUtilities.h"
#ifdef USE_SYNCOLOR
#include "synColor/synColor.h"
#include "synColor/synColorInit.h"
#include "synColor/synColorFactory.h"

using namespace SYNCOLOR;
#endif
#endif

#ifdef DEBUG
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
#endif

MPlug FireMaya::GetConnectedPlug(const MPlug& plug)
{
	if (!plug.isNull())
	{
		MPlugArray connections;
		plug.connectedTo(connections, true, false);
		if (connections.length() > 0)
		{
			MObject objNode = connections[0].node();

			if (!objNode.hasFn(MFn::kAnimCurve) && 
				!objNode.hasFn(MFn::kTransform))
			{
				return connections[0];
			}
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
#if defined(MAYA2017) && defined(USE_SYNCOLOR)
	if (colorSpace.length() &&
		MColorManagementUtilities::isColorManagementAvailable() &&
		MColorManagementUtilities::isColorManagementEnabled())
	{
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
		vector<char> converted(buffer.size());
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

		buffer.clear();
		buffer.assign(converted.begin(), converted.end());
	}
#endif
}

std::tuple<int, int, bool> getTexturePixelStride(MHWRender::MRasterFormat fFormat)
{
	int channels = 1;
	int componentSize = 0;
	bool success = true;

	switch (fFormat)
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
		channels = 4;
		componentSize = 4;
		break;
		//case MHWRender::kR32G32B32A32_UINT: break;
		//case MHWRender::kR32G32B32A32_SINT: break;
	default:
		success = false;
	}

	return std::make_tuple(channels, componentSize, success);
}

const int bytesPerPixel = 4; /// red, green, blue
const int fileHeaderSize = 14;
const int infoHeaderSize = 40;

void generateBitmapImage(unsigned char *image, int height, int width, int pitch, const char* imageFileName);
unsigned char* createBitmapFileHeader(int height, int width, int pitch, int paddingSize);
unsigned char* createBitmapInfoHeader(int height, int width);



void generateBitmapImage(unsigned char *image, int height, int width, int pitch, const char* imageFileName) {

	unsigned char padding[3] = { 0, 0, 0 };
	int paddingSize = (4 - (/*width*bytesPerPixel*/ pitch) % 4) % 4;

	unsigned char* fileHeader = createBitmapFileHeader(height, width, pitch, paddingSize);
	unsigned char* infoHeader = createBitmapInfoHeader(height, width);

	FILE* imageFile = fopen(imageFileName, "wb");

	fwrite(fileHeader, 1, fileHeaderSize, imageFile);
	fwrite(infoHeader, 1, infoHeaderSize, imageFile);

	int i;
	for (i = 0; i < height; i++) {
		fwrite(image + (i*pitch /*width*bytesPerPixel*/), bytesPerPixel, width, imageFile);
		fwrite(padding, 1, paddingSize, imageFile);
	}

	fclose(imageFile);
	//free(fileHeader);
	//free(infoHeader);
}

unsigned char* createBitmapFileHeader(int height, int width, int pitch, int paddingSize) {
	int fileSize = fileHeaderSize + infoHeaderSize + (/*bytesPerPixel*width*/pitch + paddingSize) * height;

	static unsigned char fileHeader[] = {
		0,0, /// signature
		0,0,0,0, /// image file size in bytes
		0,0,0,0, /// reserved
		0,0,0,0, /// start of pixel array
	};

	fileHeader[0] = (unsigned char)('B');
	fileHeader[1] = (unsigned char)('M');
	fileHeader[2] = (unsigned char)(fileSize);
	fileHeader[3] = (unsigned char)(fileSize >> 8);
	fileHeader[4] = (unsigned char)(fileSize >> 16);
	fileHeader[5] = (unsigned char)(fileSize >> 24);
	fileHeader[10] = (unsigned char)(fileHeaderSize + infoHeaderSize);

	return fileHeader;
}

unsigned char* createBitmapInfoHeader(int height, int width) {
	static unsigned char infoHeader[] = {
		0,0,0,0, /// header size
		0,0,0,0, /// image width
		0,0,0,0, /// image height
		0,0, /// number of color planes
		0,0, /// bits per pixel
		0,0,0,0, /// compression
		0,0,0,0, /// image size
		0,0,0,0, /// horizontal resolution
		0,0,0,0, /// vertical resolution
		0,0,0,0, /// colors in color table
		0,0,0,0, /// important color count
	};

	infoHeader[0] = (unsigned char)(infoHeaderSize);
	infoHeader[4] = (unsigned char)(width);
	infoHeader[5] = (unsigned char)(width >> 8);
	infoHeader[6] = (unsigned char)(width >> 16);
	infoHeader[7] = (unsigned char)(width >> 24);
	infoHeader[8] = (unsigned char)(height);
	infoHeader[9] = (unsigned char)(height >> 8);
	infoHeader[10] = (unsigned char)(height >> 16);
	infoHeader[11] = (unsigned char)(height >> 24);
	infoHeader[12] = (unsigned char)(1);
	infoHeader[14] = (unsigned char)(bytesPerPixel * 8);

	return infoHeader;
}

struct tileParams
{
	MHWRender::MTextureDescription& desc;
	int viewWidth;
	int viewHeight;
	int maxTileWidth;
	int maxTileHeight;
	int xTileIdx;
	int yTileIdx;
	int countXTiles;
	int countYTiles;
	int srcPixSize;
	int dstPixSize;
	const unsigned char* src;

	tileParams(MHWRender::MTextureDescription& _desc) : desc(_desc) {}
};

std::vector<unsigned char> ProcessFitStretch(
	const tileParams& params,
	rpr_image_desc& img_desc
	)
{
	std::vector<unsigned char> buffer;

	const int srcWidth = params.desc.fWidth;
	const int srcHeight = params.desc.fHeight;

	float srcWidthPerPixel = (float)srcWidth / (float)params.viewWidth;
	float srcHeightPerPixel = (float)srcHeight / (float)params.viewHeight;

	const int srcFullSegWidth = (int) std::ceil(srcWidthPerPixel * params.maxTileWidth);
	const int srcTailSegWidth = srcWidth - srcFullSegWidth * (params.countXTiles - 1);
	const int srcCurrSegWidth = (params.xTileIdx == (params.countXTiles - 1)) ? srcTailSegWidth : srcFullSegWidth;

	const int srcFullSegHeight = (int) std::ceil(srcHeightPerPixel * params.maxTileHeight);
	const int srcTailSegHeight = srcHeight - srcFullSegHeight * (params.countYTiles - 1);
	const int srcCurrSegHeight = (params.yTileIdx == 0) ? srcTailSegHeight : srcFullSegHeight;

	int createdImageWidth = srcCurrSegWidth;
	int createdImageHeight = srcCurrSegHeight;

	// set data and prepare for copying
	img_desc.image_width = createdImageWidth;
	img_desc.image_height = createdImageHeight;
	img_desc.image_row_pitch = img_desc.image_width * params.dstPixSize;

	buffer.resize(img_desc.image_height * img_desc.image_row_pitch, (char)0);

	unsigned char* dst = buffer.data();

	int shiftX = params.xTileIdx * srcFullSegWidth;
	int shiftY = (params.yTileIdx > 1) ? srcTailSegHeight + (params.yTileIdx - 1) * srcFullSegHeight : params.yTileIdx * srcTailSegHeight;

	// foreach pixel in source image
	for (int y = 0; y < srcCurrSegHeight; y++)
	{
		for (int x = 0; x < srcCurrSegWidth; x++)
		{
			memcpy(
				dst + x * params.dstPixSize + y * img_desc.image_row_pitch,
				params.src + (x + shiftX) * params.srcPixSize + (y + shiftY) * params.desc.fBytesPerRow,
				params.dstPixSize);
		}
	}

	return buffer;
}

std::vector<unsigned char> ProcessFitHorizontal(
	const tileParams& params,
	rpr_image_desc& img_desc
)
{
	std::vector<unsigned char> buffer;

	const int srcWidth = params.desc.fWidth;
	const int srcHeight = params.desc.fHeight;

	float srcWidthPerPixel = (float)srcWidth / (float)params.viewWidth;
	const int srcFullSegWidth = (int) std::ceil(srcWidthPerPixel * params.maxTileWidth);
	const int srcTailSegWidth = srcWidth - srcFullSegWidth * (params.countXTiles - 1);
	const int srcCurrSegWidth = (params.xTileIdx == (params.countXTiles - 1)) ? srcTailSegWidth : srcFullSegWidth;

	const int createdImageWidth = srcCurrSegWidth;

	float srcHeightPerPixel = srcWidthPerPixel;

	// for height, we either need to add black pixels, or cut source image to preserve aspect ratio of the source picture
	const int srcFullSegHeight = (int) std::ceil(srcHeightPerPixel * params.maxTileHeight);
	const int countFullSegments = (srcHeight > srcWidth) ?
		(int) (std::trunc(params.viewHeight * srcHeightPerPixel / (float)srcFullSegHeight)) :
		(int) (std::trunc(srcHeight / (float)srcFullSegHeight));
	const int srcImageTailSegHeight = srcHeight - countFullSegments * srcFullSegHeight;
	const int scrOutputTailSegHeight = (int) ((params.viewHeight * srcHeightPerPixel) - srcFullSegHeight * (params.countYTiles - 1));
	const int countSkipTiles = (int) (std::trunc((params.countYTiles - countFullSegments - 1) / 2));
	const int srcCurrSegHeight = (params.yTileIdx == 0) ? scrOutputTailSegHeight : srcFullSegHeight;

	int srcFirstPixelOffset = 0;
	if (srcHeight > srcWidth)
	{
		srcFirstPixelOffset = (int) std::ceil((srcHeight - params.viewHeight * srcHeightPerPixel) / 2);
	}

	int createdImageHeight = (params.yTileIdx == 0) ? scrOutputTailSegHeight : srcFullSegHeight;

	// calculate offset
	// - we can have uneven number of black tiles added at the sides of actual backplate image, and the tail segment length is different
	// NIY

	// - 1-st and last tile should have eqal number of black pixels added to them
	//int offsetY = std::ceil(srcImageTailSegHeight / 2);

	// set data and prepare for copying
	img_desc.image_width = createdImageWidth;
	img_desc.image_height = createdImageHeight;
	img_desc.image_row_pitch = img_desc.image_width * params.dstPixSize;

	buffer.resize(img_desc.image_height * img_desc.image_row_pitch, (char)0);

	int countActiveTiles = countFullSegments;
	int activeSegHeight = countFullSegments * srcFullSegHeight;
	if (activeSegHeight < srcHeight)//((countFullSegments * srcFullSegHeight + scrOutputTailSegHeight) < srcHeight)
		countActiveTiles++;
	if (srcHeight - activeSegHeight > srcFullSegHeight)
		countActiveTiles++;

	if ((params.yTileIdx >= countSkipTiles) && ((params.yTileIdx - countSkipTiles) < countActiveTiles))
	{
		unsigned char* dst = buffer.data();

		//int dstOffset = (yTileIdx == countSkipTiles) ? offsetY : 0;
		//int offsetShift = (yTileIdx > countSkipTiles) ? (dstOffset) : 0;

		int shiftX = params.xTileIdx * srcFullSegWidth; // +offsetShift;

		int shiftY; // calculate source image offset depending on how many rows of tiles were skipped
		if (countSkipTiles == 0)
		{ // 1-st row height is different then that of the rest so need to handle this case separately
			shiftY = (params.yTileIdx == 0) ? 0 : scrOutputTailSegHeight;
			if (params.yTileIdx > 0)
				shiftY += (params.yTileIdx - 1) * srcFullSegHeight;
		}
		else
		{
			shiftY = (params.yTileIdx - countSkipTiles) * srcFullSegHeight;
		}

		int lastSrcPixel = srcCurrSegHeight;
		if ((params.yTileIdx - countSkipTiles) == (countActiveTiles - 1))
		{
			if (countSkipTiles == 0)
			{
				int pixelsRemaining = srcHeight - scrOutputTailSegHeight - srcFullSegHeight * countFullSegments;
				lastSrcPixel = pixelsRemaining;
			}
			else
			{
				int pixelsRemaining = srcHeight - srcFullSegHeight * countFullSegments;
				lastSrcPixel = pixelsRemaining;
			}
		}

		if (lastSrcPixel < 0)
			lastSrcPixel = 0;

		// foreach pixel in source image
		for (int y = 0; y < lastSrcPixel; y++)
		{
			for (int x = 0; x < srcCurrSegWidth; x++)
			{
				int finalSrcShift = y + shiftY + srcFirstPixelOffset;
				if (finalSrcShift < 0)
					continue;
				if (finalSrcShift > srcWidth)
					continue;

				memcpy(
					dst + x * params.dstPixSize + (y /*+ dstOffset*/)* img_desc.image_row_pitch,
					params.src + (x + shiftX) * params.srcPixSize + (y + shiftY + srcFirstPixelOffset) * params.desc.fBytesPerRow,
					params.dstPixSize);
			}
		}
	}

	return buffer;
}

std::vector<unsigned char> ProcessFitVertical(
	const tileParams& params,
	rpr_image_desc& img_desc
)
{
	std::vector<unsigned char> buffer;

	const int srcWidth = params.desc.fWidth;
	const int srcHeight = params.desc.fHeight;

	float srcHeightPerPixel = (float)srcHeight / (float)params.viewHeight;
	const int srcFullSegHeight = (int) std::ceil(srcHeightPerPixel * params.maxTileHeight);
	const int srcTailSegHeight = srcHeight - srcFullSegHeight * (params.countYTiles - 1);
	const int srcCurrSegHeight = (params.yTileIdx == 0) ? srcTailSegHeight : srcFullSegHeight;

	const int createdImageHeight = srcCurrSegHeight;

	float srcWidthPerPixel = srcHeightPerPixel;

	// for width, we either need to add black pixels, or cut source image to preserve aspect ratio of the source picture
	const int srcFullSegWidth = (int) std::ceil(srcWidthPerPixel * params.maxTileWidth);
	const int countFullSegments = (srcWidth > srcHeight) ?
		(int) (std::trunc(params.viewWidth * srcWidthPerPixel / (float)srcFullSegWidth)) :
		(int) (std::trunc(srcWidth / (float)srcFullSegWidth));
	const int srcImageTailSegWidth = srcWidth - countFullSegments * srcFullSegWidth;
	const int scrOutputTailSegWidth = (int) ((params.viewWidth * srcWidthPerPixel) - srcFullSegWidth * (params.countXTiles - 1));
	const int countSkipTiles = (int) (std::trunc((params.countXTiles - countFullSegments - 1) / 2));
	const int srcCurrSegWidth = ((params.xTileIdx - countSkipTiles) == countFullSegments) ? srcImageTailSegWidth : srcFullSegWidth;

	int srcFirstPixelOffset = 0;
	if (srcWidth > srcHeight)
	{
		srcFirstPixelOffset = (int) std::ceil((srcWidth - params.viewWidth * srcWidthPerPixel) / 2);
	}

	int createdImageWidth = (params.xTileIdx == (params.countXTiles - 1)) ? scrOutputTailSegWidth : srcFullSegWidth;

	// calculate offset
	// - we can have uneven number of black tiles added at the sides of actual backplate image, and the tail segment length is different
	// NIY

	// 1-st and last tile should have eqal number of black pixels added to them
	// currently not used
	//int offsetX = std::ceil(srcImageTailSegWidth / 2);

	// set data and prepare for copying
	img_desc.image_width = createdImageWidth;
	img_desc.image_height = createdImageHeight;
	img_desc.image_row_pitch = img_desc.image_width * params.dstPixSize;

	buffer.resize(img_desc.image_height * img_desc.image_row_pitch, (char)0);

	if ((params.xTileIdx >= countSkipTiles) && ((params.xTileIdx - countSkipTiles) <= countFullSegments))
	{
		unsigned char* dst = buffer.data();

		unsigned int shiftX = (params.xTileIdx - countSkipTiles) * srcFullSegWidth;
		unsigned int shiftY = (params.yTileIdx > 1) ? srcTailSegHeight + (params.yTileIdx - 1) * srcFullSegHeight : params.yTileIdx * srcTailSegHeight;

		// created tile could be bigger then remaining source image (when black tiles are added) and remainin source image could be bigger (when cutting image)
		int lastSrcPixel = (createdImageWidth > srcCurrSegWidth) ? srcCurrSegWidth : createdImageWidth;

		if (lastSrcPixel < 0)
			lastSrcPixel = 0;

		// foreach pixel in source image
		for (int y = 0; y < srcCurrSegHeight; y++)
		{
			for (int x = 0; x < lastSrcPixel; x++)
			{
				int finalSrcShift = x + shiftX + srcFirstPixelOffset;
				if (finalSrcShift < 0)
					continue;
				if (finalSrcShift > srcWidth)
					continue;

				memcpy(
					dst + (x /*+ dstOffset*/)* params.dstPixSize + y * img_desc.image_row_pitch,
					params.src + (x + shiftX + srcFirstPixelOffset) * params.srcPixSize + (y + shiftY) * params.desc.fBytesPerRow,
					params.dstPixSize);
			}
		}

#ifdef _DEBUG
#ifdef DEBUG_TILE_DUMP
		// debug dump
		if ((params.xTileIdx == 2) && (params.yTileIdx == 0))
		{
			std::vector<unsigned char> buffer2(img_desc.image_height * img_desc.image_width * (dstPixSize + 1), (char)255);
			unsigned char* dst2 = buffer2.data();
			for (unsigned int y = 0; y < srcCurrSegHeight; y++)
			{
				for (unsigned int x = 0; x < srcCurrSegWidth; x++)
				{
					memcpy(
						dst2 + x * (dstPixSize + 1) + (srcCurrSegHeight - 1 - y) * img_desc.image_width * (dstPixSize + 1),
						src + (x + shiftX) * srcPixSize + (y + shiftY) * params.desc.fBytesPerRow,
						dstPixSize);
				}
			}

			generateBitmapImage(dst2, img_desc.image_height, img_desc.image_width, img_desc.image_width * 4, "C:\\temp\\dbg\\1.bmp");
		}
#endif
#endif

	}

	return buffer;
}

std::vector<unsigned char> calculateTileImage(
	const tileParams& params,
	MHWRender::MTextureDescription& desc,
	FireMaya::FitType imgFit,
	rpr_image_desc& img_desc)
{
	switch(imgFit) 
	{
		case FireMaya::FitType::FitBest:
		{
			if (desc.fWidth > desc.fHeight)
				return ProcessFitHorizontal(params, img_desc);
			else
				return ProcessFitVertical(params, img_desc);
		}

		case FireMaya::FitType::FitFill:
		{
			if (desc.fWidth > desc.fHeight)
				return ProcessFitVertical(params, img_desc);
			else
				return ProcessFitHorizontal(params, img_desc);
		}

		case FireMaya::FitType::FitStretch:
		{
			return ProcessFitStretch(params, img_desc);
		}

		case FireMaya::FitType::FitHorizontal:
		{
			return ProcessFitHorizontal(params, img_desc);
		}

		case FireMaya::FitType::FitVertical:
		{
			return ProcessFitVertical(params, img_desc);
		}
	}

	// dummy return for compiler
	std::vector <unsigned char> dummy;
	return dummy;
}

frw::Image FireMaya::Scope::GetTiledImage(MString texturePath,
	int viewWidth, int viewHeight,
	int maxTileWidth, int maxTileHeight,
	int currTileWidth, int currTileHeight,
	int countXTiles, int countYTiles,
	int xTileIdx, int yTileIdx,
	MString colorSpace,
	FitType imgFit)
{
	// back-off
	if (texturePath.length() == 0)
	{
		return NULL;
	}

	// get key for texture tile
	char key[1024] = {};
	int written = snprintf(key, 1024, "%s-%d-%d-%d-%d",
		texturePath.asUTF8(),
		viewWidth, viewHeight,
		xTileIdx, yTileIdx);
	assert(written >= 0 && written < 1024);

	// try find cached texture
	auto it = m->imageCache.find(key);
	if (it != m->imageCache.end())
	{
		DebugPrint("Using cached image from imageCache...");
		return it->second;
	}

	// not cached => generate new
	frw::Image retImage = FireRenderThread::RunOnMainThread<frw::Image>([&]()
	{
		MAIN_THREAD_ONLY; // MTextureManager will not work in other threads

		frw::Image image;

		// back-offs
		auto renderer = MHWRender::MRenderer::theRenderer(); // have to use auto because these are different classes in Maya 2017 and 2018
		if (!renderer)
		{
			return image;
		}

		auto textureManager = renderer->getTextureManager(); // have to use auto because these are different classes in Maya 2017 and 2018
		if (!textureManager)
		{
			return image;
		}

		auto texture = textureManager->acquireTexture(texturePath); // have to use auto because these are different classes in Maya 2017 and 2018
		if (!texture)
		{
			return image;
		}

		// get texture file information
		MHWRender::MTextureDescription desc = {};
		texture->textureDescription(desc);

		size_t slicePitch = 0;

		int rowPitch = 0;
		void* rawData = texture->rawData(rowPitch, slicePitch);
		if (!rawData)
		{
			textureManager->releaseTexture(texture);

			return image;
		}

		const unsigned char* src = static_cast<const unsigned char*>(rawData);

		int channels = 0;
		int pixelStride = 0;
		bool isImageFormatSupported = false;
		std::tie(channels, pixelStride, isImageFormatSupported) = getTexturePixelStride(desc.fFormat);

		// create rpr image structures
		rpr_image_format format = {};
		format.num_components = channels >= 3 ? 3 : 1;
		format.type = (pixelStride == 4) ? RPR_COMPONENT_TYPE_FLOAT32 :
			(pixelStride == 2) ? RPR_COMPONENT_TYPE_FLOAT16 :
			RPR_COMPONENT_TYPE_UINT8;

		rpr_image_desc img_desc = {};

		// get segment of background image corresponding to tile
		tileParams params(desc);
		params.viewWidth = viewWidth;
		params.viewHeight = viewHeight;
		params.maxTileWidth = maxTileWidth;
		params.maxTileHeight = maxTileHeight;
		params.xTileIdx = xTileIdx;
		params.yTileIdx = yTileIdx;
		params.countXTiles = countXTiles;
		params.countYTiles = countYTiles;
		params.src = src;
		params.srcPixSize = pixelStride * channels;
		params.dstPixSize = pixelStride * format.num_components;

		// - buffer for background image tile
		std::vector<unsigned char> buffer = calculateTileImage(
			params,
			desc,
			imgFit,
			img_desc);

		image = frw::Image(m->context, format, img_desc, buffer.data());
		image.SetName(key);

		rprImageSetWrap(image.Handle(), RPR_IMAGE_WRAP_TYPE_MIRRORED_REPEAT);

		texture->freeRawData(rawData);

		return image;
	});

	// store image in image cache
	if (retImage)
		m->imageCache[key] = retImage;

	return retImage;
}

frw::Image FireMaya::Scope::GetImage(MString texturePath, MString colorSpace, const MString& ownerNodeName) const
{
	if (texturePath.length() == 0)
	{
		return NULL;
	}

	std::string key = (texturePath + ":" + colorSpace).asUTF8();

	auto it = m->imageCache.find(key);
	if (it != m->imageCache.end())
		return it->second;

	frw::Image retImage = FireRenderThread::RunOnMainThread<frw::Image>([this, texturePath, key, colorSpace, ownerNodeName]() -> frw::Image
	{
		MAIN_THREAD_ONLY; // MTextureManager will not work in other threads
		DebugPrint("Loading Image: %s in colorSpace: %s", texturePath.asUTF8(), colorSpace.asUTF8());

		std::string processedTexturePath = ProcessEnvVarsInFilePath<std::string, char>(texturePath.asChar());

		frw::Image image;

		image = frw::Image(m->context, processedTexturePath.c_str());

		if (!image)
		{
			image = LoadImageUsingMTexture(MString(processedTexturePath.c_str()), colorSpace, ownerNodeName);
		}

		if (image)
		{
			m->imageCache[key] = image;

			// recent RPR API is friendly with UTF-8.
			// RPRS and GLTF lib use RPR Object Name (set with rprObjectSetName) to get image file path for quick export. They support this path as UTF-8.
			image.SetName(texturePath.asUTF8());

		}

		return image;
	});

	return retImage;
}

frw::Image FireMaya::Scope::LoadImageUsingMTexture(MString texturePath, MString colorSpace, const MString& ownerNodeName) const
{
	frw::Image img;

	if (auto renderer = MHWRender::MRenderer::theRenderer())
	{
		if (auto textureManager = renderer->getTextureManager())
		{
			try
			{
				if (auto texture = textureManager->acquireTexture(texturePath, ownerNodeName))
				{
					MHWRender::MTextureDescription desc = {};
					texture->textureDescription(desc);

					int rowPitch = 0;

#if MAYA_API_VERSION >= 20180000
					size_t slicePitch = 0;
#else
					int slicePitch = 0;
#endif

					if (const auto rawData_to_free = texture->rawData(rowPitch, slicePitch))
					{
						auto srcData = rawData_to_free;
						auto width = desc.fWidth;
						auto height = desc.fHeight;

						auto channels = 1;
						auto componentSize = 1;
						bool isFormatSupported = false;

						std::tie(channels, componentSize, isFormatSupported) = getTexturePixelStride(desc.fFormat);

						if (!isFormatSupported)
						{
							std::ostringstream stringStream;
							stringStream << "Error Loading Image : " << texturePath.asUTF8() <<
								". Unsupported MRasterFormat format: " << desc.fFormat;
							std::string errMsg = stringStream.str();

							texture->freeRawData(rawData_to_free);
							textureManager->releaseTexture(texture);

							throw std::runtime_error(errMsg.c_str());
						}

						img = CreateImageInternal(colorSpace, desc.fWidth, desc.fHeight, 
													srcData, channels, componentSize, rowPitch);

						texture->freeRawData(rawData_to_free);
					}
					textureManager->releaseTexture(texture);
				}
			}
			catch (std::exception& e)
			{
				ErrorPrint(e.what());
			}
			catch (...)
			{
				ErrorPrint("Error Loading Image: %s", texturePath.asUTF8());
			}
		}
	}

	return img;
}

frw::Image FireMaya::Scope::CreateImageInternal(MString colorSpace, 
												unsigned int width, 
												unsigned int height,
												void* srcData,
												unsigned int channels,
												unsigned int componentSize,
												unsigned int rowPitch,
												bool flipY) const
{
	int srcRowPitch = rowPitch;// desc.fBytesPerRow;

	std::vector<float> tempBuffer;
	if (componentSize == 2)	// we don't support half size floats at the moment!
	{
		tempBuffer.resize(width * height * channels);
		for (auto y = 0u; y < height; y++)
		{
			auto src = static_cast<const unsigned short*>(srcData) + y * rowPitch / sizeof(unsigned short);
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
		unsigned char* dst = nullptr;
		
		if (!flipY)
		{
			dst = buffer.data() + y * img_desc.image_row_pitch;
		}
		else
		{
			dst = buffer.data() + (height - y - 1) * img_desc.image_row_pitch;
		}

		for (unsigned int x = 0; x < width; x++)
		{
			memcpy(dst + x * dstPixSize, src + x * srcPixSize, dstPixSize);
		}
	}

	convertColorSpace(colorSpace, format, img_desc, buffer);
	return frw::Image(m->context, format, img_desc, buffer.data());
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

#if MAYA_API_VERSION >= 20180000
					size_t slicePitch = 0;
#else
					int slicePitch = 0;
#endif
					int rowPitch = 0;
					if (void* rawData = texture->rawData(rowPitch, slicePitch))
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

						// double srcAspect = double(desc.fWidth) / double(desc.fHeight);
						// double dstAspect = double(img_desc.image_width) / double(img_desc.image_height);


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

		return image;
		});
	}

frw::Value FireMaya::Scope::ParseValue(MObject node, const MString &outPlugName) const
{
	if (node.isNull())
	{
		return nullptr;
	}

	MFnDependencyNode shaderNode(node);

	// handle native fire render value nodes
	const FireMaya::ValueNode* valueNode = dynamic_cast<FireMaya::ValueNode*>(shaderNode.userNode());
	if (valueNode != nullptr)
	{
		return valueNode->GetValue(*this);
	}

	MTypeId mayaId = shaderNode.typeId();
	unsigned int intId = mayaId.id();

	// interpret Maya nodes
	return MayaStandardNodeConverters::NodeConverterUtil::Convert(
		{ shaderNode, *this, outPlugName }, 
		MayaStandardNodeConverters::MayaValueId(intId)
	);
}

frw::Value FireMaya::Scope::createImageFromShaderNodeUsingFileNode(MObject node, MString plugName) const
{
	int width = 0;
	int height = 0;

	if (FindFileNodeRecursive(node, width, height) &&
		(width > 0 && height > 0))
	{
		return createImageFromShaderNode(node, plugName, width, height);
	}
	else
	{
		return createImageFromShaderNode(node, plugName);
	}
}

bool FireMaya::Scope::FindFileNodeRecursive(MObject objectNode, int& width, int& height) const
{
	if (!objectNode.hasFn(MFn::kDependencyNode))
	{
		return false;
	}

	// Check if we have got file node
	if (objectNode.hasFn(MFn::kFileTexture))
	{
		MFnDependencyNode fileNode(objectNode);

		MPlug sizePlug = fileNode.findPlug("outSizeX");
		if (!sizePlug.isNull())
		{
			width = (int)sizePlug.asFloat();
		}

		sizePlug = fileNode.findPlug("outSizeY");
		if (!sizePlug.isNull())
		{
			height = (int)sizePlug.asFloat();
		}

		return true;
	}

	MPlugArray connections;
	MStatus status;

	MFnDependencyNode node(objectNode);
	if (node.getConnections(connections) == MStatus::kSuccess)
	{
		for (MPlug& plug : connections)
		{
			MString name = plug.name(&status);

			if (plug.attribute().hasFn(MFn::kMessageAttribute))
			{
				continue;
			}

			if (plug.isDestination())
			{
				MObject node = GetConnectedNode(plug);

				if (FindFileNodeRecursive(node, width, height))
				{
					return true;
				}
			}
		}
	}

	return false;
}

frw::Value FireMaya::Scope::createImageFromShaderNode(MObject node, MString plugName, int width, int height) const
{
	unsigned int max_width = 1;
	unsigned int max_height = 1;
	bool shouldResize = GetIContextInfo() && GetIContextInfo()->ShouldResizeTexture(max_width, max_height);

	return FireRenderThread::RunOnMainThread<frw::Value>([&]()
	{
		if (shouldResize)
		{
			width = max_width;
			height = max_height;
		}

		frw::Value ret;

		MAIN_THREAD_ONLY; // MTextureManager will not work in other threads

		MFnDependencyNode shaderNode(node);
		MUuid uid = shaderNode.uuid();
		MString uid_str = uid.asString();

		// try get resized image from cache
		if (shouldResize)
		{
			frw::Image cachedImage = GetCachedImage(uid_str);
			if (cachedImage.IsValid())
			{
				frw::ImageNode imageNode(m->materialSystem);
				imageNode.SetMap(cachedImage);
				ret = imageNode; // need to return frv::value
				return ret;
			}
		}

		// not in cache => create new image
		if (auto renderer = MHWRender::MRenderer::theRenderer())
		{
			if (auto textureManager = renderer->getTextureManager())
			{
				// Get first output connection to cover such cases as outputColor, outputValue etc.
			
				MPlug outColorPlug = shaderNode.findPlug(plugName);

				if (auto texture = textureManager->acquireTexture("", outColorPlug, width, height, false))
				{
					std::vector<unsigned char> buffer(width * height * 3, 128);

#if MAYA_API_VERSION >= 20180000
					size_t slicePitch = 0;
#else
					int slicePitch = 0;
#endif
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

						SetCachedImage(uid, image);
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
	auto color = GetValueForDiffuseColor(node.findPlug("color"));
	auto diffuse = GetValueWithCheck(node, "diffuse");
	auto specularColor = GetValueWithCheck(node, "specularColor");
	auto eccentricity = GetValueWithCheck(node, "eccentricity");
	auto specularRollOff = GetValueWithCheck(node, "specularRollOff");

	frw::MaterialSystem materialSystem = m->materialSystem;

	frw::Shader result(materialSystem, frw::ShaderTypeDiffuse);

	auto res = materialSystem.ValueMul(color, diffuse);
	result.SetValue(RPR_MATERIAL_INPUT_COLOR, res);

	frw::Shader shader(materialSystem, frw::ShaderTypeMicrofacet);

	shader.SetValue(RPR_MATERIAL_INPUT_COLOR, specularColor);

	auto val = materialSystem.ValuePow(eccentricity, frw::Value(0.5));
	val = materialSystem.ValueSub(val, frw::Value(0.1));
	val = materialSystem.ValueMax(val, frw::Value(0.0));
	val = materialSystem.ValueMul(val, 0.45f);

	// since Core 1.35 we need to pass square root of initial roughness
	shader.SetValue(RPR_MATERIAL_INPUT_ROUGHNESS, materialSystem.ValuePow(val, 0.5));
	shader.SetValue(RPR_MATERIAL_INPUT_IOR, frw::Value(1.0f));

	auto blendVal = materialSystem.ValueMul(specularRollOff, specularRollOff);
	blendVal = materialSystem.ValueMul(blendVal, frw::Value(0.15));
	blendVal = materialSystem.ValueAdd(blendVal, frw::Value(0.002));

	return materialSystem.ShaderBlend(result, shader, blendVal);
}


frw::Shader FireMaya::Scope::convertMayaPhong(const MFnDependencyNode &node)
{
	auto color = GetValueForDiffuseColor(node.findPlug("color"));
	auto diffuse = GetValueWithCheck(node, "diffuse");
	auto cosinePower = GetValueWithCheck(node, "cosinePower");
	auto specularColor = GetValueWithCheck(node, "specularColor");

	frw::MaterialSystem materialSystem = m->materialSystem;

	frw::Shader result(materialSystem, frw::ShaderTypeDiffuse);

	auto res = materialSystem.ValueMul(color, diffuse);
	result.SetValue(RPR_MATERIAL_INPUT_COLOR, res);

	frw::Shader shader(materialSystem, frw::ShaderTypeMicrofacet);

	shader.SetValue(RPR_MATERIAL_INPUT_COLOR, specularColor);

	auto val = CosinePowerToRoughness(cosinePower);

	// since Core 1.35 we need to pass square root of initial roughness
	shader.SetValue(RPR_MATERIAL_INPUT_ROUGHNESS, materialSystem.ValuePow(val, 0.5));

	shader.SetValue(RPR_MATERIAL_INPUT_IOR, frw::Value(1.0f));

	return materialSystem.ShaderBlend(result, shader, frw::Value(0.35, 0.35, 0.35));
}


frw::Shader FireMaya::Scope::convertMayaPhongE(const MFnDependencyNode &node)
{
	auto color = GetValueForDiffuseColor(node.findPlug("color"));
	auto diffuse = GetValueWithCheck(node, "diffuse");
	auto roughness = GetValueWithCheck(node, "roughness");
	auto whiteness = GetValueWithCheck(node, "whiteness");
	auto highlightSize = GetValueWithCheck(node, "highlightSize");
	auto specularColor = GetValueWithCheck(node, "specularColor");

	frw::MaterialSystem materialSystem = m->materialSystem;

	frw::Shader result(materialSystem, frw::ShaderTypeDiffuse);

	auto res = materialSystem.ValueMul(color, diffuse);
	result.SetValue(RPR_MATERIAL_INPUT_COLOR, res);

	frw::Shader shader(materialSystem, frw::ShaderTypeMicrofacet);

	auto secondColor = materialSystem.ValueMul(specularColor, whiteness);
	specularColor = materialSystem.ValueMix(specularColor, secondColor, frw::Value(0.4));

	shader.SetValue(RPR_MATERIAL_INPUT_COLOR, specularColor);

	auto val = materialSystem.ValueMul(roughness, highlightSize);
	val = materialSystem.ValueMul(val, 0.5f);

	// since Core 1.35 we need to pass square root of initial roughness
	shader.SetValue(RPR_MATERIAL_INPUT_ROUGHNESS, materialSystem.ValuePow(val, 0.5));

	shader.SetValue(RPR_MATERIAL_INPUT_IOR, frw::Value(1.0f));

	auto blendVal = materialSystem.ValueMul(roughness, frw::Value(0.3));
	blendVal = materialSystem.ValueAdd(blendVal, frw::Value(0.01));

	return materialSystem.ShaderBlend(result, shader, blendVal);
}

frw::Shader FireMaya::Scope::ParseShader(MObject node)
{
	if (node.isNull())
		return nullptr;

	// force output plugs get evaluated to let Maya to calculate, clean and cache all necessary plugs in network.
	// Without that IPR may not work on quite big dependency graphs because shaders stay dirty and ShaderDirty callback will not be invoked on geometry nodes
	FireMaya::Node::ForceEvaluateAllAttributes(node, false);

	frw::Shader result;

	MFnDependencyNode shaderNode(node);
	frw::MaterialSystem materialSystem = m->materialSystem;

	if (auto n = dynamic_cast<FireMaya::ShaderNode*>(shaderNode.userNode()))
	{
		if (m_pContextInfo->IsShaderNodeSupported(n))
		{
			result = n->GetShader(*this);
		}
		else
		{
			frw::Value color = GetValueForDiffuseColor(shaderNode.findPlug("color"));
			result = m_pContextInfo->GetDefaultColorShader(color);
		}
	}
	else
	{
		// interpret Maya nodes (complex materials, not utilities and maps)
		MTypeId typeId = shaderNode.typeId();
		auto id = MayaSurfaceId(shaderNode.typeId().id());
		auto name = shaderNode.typeName();

		DebugPrint("Shader: %s [0x%x]", name.asUTF8(), id);

		if (!m_pContextInfo->IsShaderSupported(frw::ShaderType::ShaderTypeDiffuse))
		{
			// This is Hybrid related code pass
			frw::Value color = GetValueForDiffuseColor(shaderNode.findPlug("color"));
			frw::Shader shader = m_pContextInfo->GetDefaultColorShader(color);

			return shader;
		}

		switch (id)
		{
		case MayaNodeLambert:
		{
			frw::DiffuseShader diffuseShader(materialSystem);
			frw::Value color = GetValueForDiffuseColor(shaderNode.findPlug("color"));
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
			frw::Shader specularShader(materialSystem, frw::ShaderTypeMicrofacetAnisotropicReflection);

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

			frw::Value anisotropic = materialSystem.ValueSub(mayaSpreadX, mayaSpreadY);
			anisotropic = materialSystem.ValueDiv(anisotropic, frw::Value(100.0));
			specularShader.SetValue(RPR_MATERIAL_INPUT_ANISOTROPIC, anisotropic);

			specularShader.SetValue(RPR_MATERIAL_INPUT_ROUGHNESS, roughness);

			specularShader.SetValue(RPR_MATERIAL_INPUT_COLOR, specularColor);

			double rads = -1.0 * angle.GetX() * M_PI / 180.0;
			frw::Value radians = frw::Value(rads, rads, rads);
			specularShader.SetValue(RPR_MATERIAL_INPUT_ROTATION, radians);
		

			result = materialSystem.ShaderBlend(diffuseShader, specularShader, 0.5);
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
			result.SetValue(RPR_MATERIAL_INPUT_NORMAL, normals);
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
		// TODO
		// Interpret Maya nodes (complex materials, not utilities and maps)
	}
	return frw::Shader();
}


void FireMaya::Scope::RegisterCallback(MObject node, std::string* pOverridenUUID /*= nullptr*/)
{
	if (pOverridenUUID != nullptr)
	{
		if (m->m_nodeDirtyCallbacks.find(*pOverridenUUID) == m->m_nodeDirtyCallbacks.end())
		{
			m->m_nodeDirtyCallbacks[*pOverridenUUID] = MNodeMessage::addNodeDirtyCallback(node, NodeDirtyCallback, this);
		}

		return;
	}

	std::string uuid = getNodeUUid(node);
	if (m->m_nodeDirtyCallbacks.find(uuid) == m->m_nodeDirtyCallbacks.end())
	{
		m->m_nodeDirtyCallbacks[uuid] = MNodeMessage::addNodeDirtyCallback(node, NodeDirtyCallback, this);
	}
}

void FireMaya::Scope::NodeDirtyCallback(MObject& ob)
{
	try
	{
		MFnDependencyNode node(ob);
		DebugPrint("Callback: %s dirty", node.typeName().asUTF8());

		std::string shdrName = node.name().asChar();
		auto shaderId = getNodeUUid(ob);
		shaderId += shdrName;

		if (auto shader = GetCachedShader(shaderId)) {
			shader.SetDirty(true);
		}
		if (auto shader = GetCachedVolumeShader(shaderId)) {
			shader.SetDirty(true);
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
	MFnDependencyNode fnShdr(node);
	std::string shdrName = fnShdr.name().asChar();

	Scope* self = static_cast<Scope*>(clientData);
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

std::pair<FireMaya::Scope::MMapNodeIdIterator, FireMaya::Scope::MMapNodeIdIterator> FireMaya::Scope::GetCachedShaderIds(const NodeId& lightId)
{
	return m->lightShaderMap.equal_range(lightId);
}

void FireMaya::Scope::SetCachedShaderId(const NodeId& lightId, NodeId& shaderId)
{
	m->lightShaderMap.insert(std::pair<NodeId, NodeId>(lightId, shaderId));
}

void FireMaya::Scope::ClearCachedShaderIds(const NodeId& lightId)
{
	m->lightShaderMap.erase(lightId);
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

frw::Image FireMaya::Scope::GetCachedImage(const MString& key) const
{
	auto it = m->imageCache.find(std::string(key.asChar()));

	if (it != m->imageCache.end())
		return it->second;

	return nullptr;
}

void FireMaya::Scope::SetCachedImage(const MString& key, frw::Image img) const
{
	if (!img)
		m->imageCache.erase(std::string(key.asChar()));
	else
		m->imageCache[std::string(key.asChar())] = img;
}

FireMaya::Scope::Scope()
	: m_pContextInfo(nullptr)
{
}

FireMaya::Scope::~Scope()
{
	Reset();
}


frw::Shader FireMaya::Scope::GetShader(MObject node, const FireRenderMeshCommon* pMesh, bool forceUpdate)
{
	if (node.isNull())
	{
		return frw::Shader();
	}

	MFnDependencyNode fnShdr(node);
	std::string shdrName = fnShdr.name().asChar();
	std::string shaderId = getNodeUUid(node);
	shaderId += shdrName;
	frw::Shader shader = GetCachedShader(shaderId);

	bool shdrIsVaild = shader.IsValid();
	bool shdrNotDirty = !shader.IsDirty();
	if (!forceUpdate && shader.IsValid() && !shader.IsDirty())
	{
		return shader;
	}

	// register callbacks if one doesn't already exist
	if (!shader.IsValid())
	{
		RegisterCallback(node, &shaderId);
	}

	DebugPrint("Parsing shader: %s (forceUpdate=%d, shader.IsDirty()=%d)", shaderId.c_str(), forceUpdate, shader.IsDirty());
	m->m_pCurrentlyParsedMesh = pMesh;
	m->m_pLastLinkedLight = MObject::kNullObj;

	// create now
	shader = ParseShader(node);
	if (shader.IsValid())
	{
		SetCachedShader(shaderId, shader);
		shader.SetDirty(false);

		if (m->m_pLastLinkedLight != MObject::kNullObj)
		{
			std::string lightId = getNodeUUid(m->m_pLastLinkedLight);

			MFnDependencyNode fnNode(m->m_pLastLinkedLight);
			std::string fnName = fnNode.name().asChar();

			SetCachedShaderId(lightId, shaderId);

			m->m_pLastLinkedLight = MObject::kNullObj;
		}
	}

	m->m_pCurrentlyParsedMesh = nullptr;
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

frw::Shader FireMaya::Scope::GetShadowCatcherShader()
{
	for (auto shader : m->shaderMap)
	{
		if (shader.second.IsShadowCatcher()) 
			return shader.second;
	}
	return nullptr;
}

frw::Shader FireMaya::Scope::GetReflectionCatcherShader()
{
	for (auto shader : m->shaderMap)
	{
		if (shader.second.IsReflectionCatcher()) 
			return shader.second;
	}
	return nullptr;
}

frw::Value FireMaya::Scope::GetValue(MObject ob, const MString &outPlugName) const
{
	return ParseValue(ob, outPlugName);
}

frw::Value FireMaya::Scope::GetConnectedValue(const MPlug& plug) const
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

frw::Value FireMaya::Scope::GetValue(const MPlug& plug) const
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

frw::Value FireMaya::Scope::GetValueForDiffuseColor(const MPlug& plug)
{
	m_IsLastPassTextureMissing = false;
	frw::Value value = GetValue(plug);

	if (value.GetNodeType() == -1)
	{
		MPlugArray shaderConnections;
		plug.connectedTo(shaderConnections, true, false);
		if (shaderConnections.length() != 0 && m_IsLastPassTextureMissing)
		{
			//Use default value for missing texture
			frw::ImageNode imageNode(MaterialSystem());
			imageNode.SetMap(frw::Image(Context(), .5f, .5f, 1.f));
			value = imageNode;
		}
	}

	return value;
}

frw::Value FireMaya::Scope::GetValueWithCheck(const MFnDependencyNode &node, const char * plugName) const
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
	: m_pCurrentlyParsedMesh(nullptr)
{
}

FireMaya::Scope::Data::~Data()
{
	RPR_THREAD_ONLY;

	for (const auto& it : m_nodeDirtyCallbacks)
	{
		MNodeMessage::removeCallback(it.second);
	}

	for (const auto& it : m_AttributeChangedCallbacks)
	{
		MNodeMessage::removeCallback(it.second);
	}

	scene.Reset();
	materialSystem.Reset();
	context.Reset();

	// delete shaders
	shaderMap.clear();

	// everything else destroyed automatically
}

void FireMaya::Scope::Reset()
{
	m.reset();
}

void FireMaya::Scope::Init(rpr_context handle, bool destroyMaterialSystemOnDelete, bool createScene)
{
	RPR_THREAD_ONLY;

	m = std::make_shared<Data>();

	m->context = frw::Context(handle);
	m->materialSystem = frw::MaterialSystem(m->context, nullptr, destroyMaterialSystemOnDelete);

	if (createScene)
	{
		m->scene = m->context.CreateScene();
		m->scene.SetActive();
	}
}

void FireMaya::Scope::CreateScene()
{
	m->scene = m->context.CreateScene();
	m->scene.SetActive();
}

void FireMaya::Scope::SetContextInfo(IFireRenderContextInfo* pCtxInfo)
{
	if (pCtxInfo == nullptr)
		return;

	m_pContextInfo = pCtxInfo;
}

const IFireRenderContextInfo* FireMaya::Scope::GetIContextInfo(void) const
{
	return m_pContextInfo;
}

IFireRenderContextInfo* FireMaya::Scope::GetIContextInfo()
{
	return m_pContextInfo;
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

	FillOutputAttributeNames();
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

void FireMaya::Node::ForceEvaluateAllAttributes(bool evaluateInputs)
{
	ForceEvaluateAllAttributes(thisMObject(), evaluateInputs);
}

void FireMaya::Node::ForceEvaluateAllAttributes(MObject node, bool evaluateInputs)
{
	MFnDependencyNode depNode(node);

	unsigned int count = depNode.attributeCount();

	MStatus status;
	for (unsigned int i = 0; i < count; ++i)
	{
		MObject attrObj = depNode.attribute(i);
		MFnAttribute attr(attrObj);
		MPlug plug = depNode.findPlug(attr.object(), &status);
		CHECK_MSTATUS(status);
		bool attrProcessed = false;

		bool isOutput = IsOutputAttribute(attrObj, !evaluateInputs);
		if (isOutput == evaluateInputs)
		{
			continue;
		}

		if (attrObj.hasFn(MFn::kNumericAttribute))
		{
			if (plug.numChildren() > 0)
			{
				continue;
			}
			else
			{
				float val;
				MStatus status = plug.getValue(val);
				CHECK_MSTATUS(status);

				attrProcessed = true;
			}
		}
		else if (attrObj.hasFn(MFn::kEnumAttribute))
		{
			int val;
			MStatus status = plug.getValue(val);
			CHECK_MSTATUS(status);

			attrProcessed = true;
		}
		else if (attrObj.hasFn(MFn::kTypedAttribute))
		{
			MFnTypedAttribute typedAttr(attrObj);

			if (typedAttr.attrType() == MFnData::Type::kString)
			{
				MString str;
				MStatus status = plug.getValue(str);
				CHECK_MSTATUS(status);

				attrProcessed = true;
			}
		}

		if (!attrProcessed)
		{
			// It may be bad (if we missed something above), may be ok, just log it in case of necessity
			DebugPrint("Plug hasn't been processed: %s", plug.name().asChar());
		}
	}
}

void FireMaya::Node::GetOutputAttributes(std::vector<MObject>& outputVec)
{
	outputVec.clear();

	MFnDependencyNode depNode(thisMObject());
	size_t count = m_outputAttributeNames.size();

	outputVec.resize(count);

	MStatus status;
	for (int i = 0; i < count; i++)
	{
		outputVec[i] = depNode.attribute(m_outputAttributeNames[i], &status);
		CHECK_MSTATUS(status);
	}
}

// Just request all inputs and set as cleaned requested output
MStatus FireMaya::Node::compute(const MPlug& plug, MDataBlock& block)
{
	std::vector<MObject> outputVec;
	GetOutputAttributes(outputVec);

	for (size_t i = 0; i < outputVec.size(); i++)
	{
		if (plug == outputVec[i] || plug.parent() == outputVec[i])
		{
			ForceEvaluateAllAttributes(true);

			MDataHandle outHandle = block.outputValue(plug.attribute());
			outHandle.setClean();

			block.setClean(plug);
			return MS::kSuccess;
		}
	}

	return MStatus::kUnknownParameter;
}

void FireMaya::Node::FillOutputAttributeNames()
{
	MFnDependencyNode depNode(thisMObject());

	unsigned int count = depNode.attributeCount();

	m_outputAttributeNames.clear();

	MStatus status;
	for (unsigned int i = 0; i < count; ++i)
	{
		MObject attrObj = depNode.attribute(i);
		MFnAttribute attr(attrObj);

		if (IsOutputAttribute(attrObj, true))
		{
			m_outputAttributeNames.push_back(attr.name());
		}
	}
}

bool FireMaya::Node::IsOutputAttribute(MObject attrObj, bool parentsOnly)
{
	MFnAttribute attr(attrObj);

	if (attr.isWritable() || (!attr.parent().isNull() && parentsOnly))
	{
		return false;
	}

	if (std::string(attr.name().asChar()).find("out") == 0)
	{
		return true;
	}

	return false;
}

FireMaya::Node::~Node()
{
	if (openCallback != 0) {
		MNodeMessage::removeCallback(openCallback);
	}

	if (importCallback != 0) {
		MNodeMessage::removeCallback(importCallback);
	}
}
