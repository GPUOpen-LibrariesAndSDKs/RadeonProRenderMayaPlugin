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
#include "frWrap.h"

#include "FireRenderAOV.h"
#include "Context/FireRenderContext.h"
#include "FireRenderImageUtil.h"
#include "RenderStamp.h"
#include <maya/MCommonSystemUtils.h>
#include <maya/MViewport2Renderer.h>
#include <maya/MGlobal.h>


void PixelBuffer::resize(size_t newCount)
{
	size_t newSize = sizeof(RV_PIXEL) * newCount;
	if (newSize != m_size)
	{
		void * newBuffer = nullptr;
#ifdef WIN32
		newBuffer = _aligned_realloc(m_pBuffer, newSize, 128);
#else
		newBuffer = malloc(newSize);
		if(m_pBuffer)
		{
			free(m_pBuffer);
			m_pBuffer = nullptr;
		}
#endif

		m_pBuffer = static_cast<RV_PIXEL*>(newBuffer);
		m_size = newSize;
	}
}

void PixelBuffer::overwrite(const RV_PIXEL* input, const RenderRegion& region, unsigned int totalHeight, unsigned int totalWidth, int aov_id /*= 0*/)
{
	// ensure valid input
	assert(input != nullptr);

	if (region.top > totalHeight)
		return;

	if (region.right > totalWidth)
		return;

	// Get region dimensions.
	unsigned int regionWidth = region.getWidth();
	unsigned int regionHeight = region.getHeight();

	// copy line by line
	for (unsigned int y = 0; y < regionHeight; y++)
	{
		unsigned int inputIndex = (regionHeight - 1 - y) * regionWidth; // writing to self

		// - keep in mind that y is inverted
		//unsigned int destShiftY = (totalHeight - 1) - region.top;
		//unsigned int destIndex = region.left + (destShiftY + y) * totalWidth;
		unsigned int destShiftY = region.top - y;
		unsigned int destIndex = region.left + (destShiftY) * totalWidth;

		memcpy(&m_pBuffer[destIndex], &input[inputIndex], sizeof(RV_PIXEL) * regionWidth);
	}

#ifdef _DEBUG
#ifdef DUMP_PIXELS_PIXELBUFF
	debugDump(totalHeight, totalWidth, std::string(FireRenderAOV::GetAOVName(aov_id) + "_tile_"));
#endif
#endif
}

#ifdef _DEBUG
void generateBitmapImage(unsigned char *image, int height, int width, int pitch, const char* imageFileName);
#endif

void PixelBuffer::debugDump(unsigned int totalHeight, unsigned int totalWidth, std::string& fbName)
{
#ifdef _DEBUG
#ifdef DUMP_PIXELS_PIXELBUFF
	assert(sizeof(RV_PIXEL) * totalHeight * totalWidth == m_size);

	std::vector<RV_PIXEL> sourcePixels;
	sourcePixels.reserve(totalHeight * totalWidth);

	for (unsigned int y = 0; y < totalHeight; y++)
	{
		for (unsigned int x = 0; x < totalWidth; x++)
		{
			RV_PIXEL pixel = m_pBuffer[x + y * totalWidth];
			sourcePixels.push_back(pixel);
		}
	}

	std::vector<unsigned char> buffer2;
	buffer2.reserve(totalHeight * totalWidth);

	for (unsigned int y = 0; y < totalHeight; y++)
	{
		for (unsigned int x = 0; x < totalWidth; x++)
		{
			RV_PIXEL& pixel = sourcePixels[x + y * totalWidth];
			char r = 255 * pixel.r;
			char g = 255 * pixel.g;
			char b = 255 * pixel.b;

			buffer2.push_back(b);
			buffer2.push_back(g);
			buffer2.push_back(r);
			buffer2.push_back(255);
		}
	}

	static int debugDumpIdx = 0;
	std::string dumpAddr = "C:\\temp\\dbg\\" + fbName +std::to_string(debugDumpIdx++) + ".bmp";
	unsigned char* dst2 = buffer2.data();
	generateBitmapImage(dst2, totalHeight, totalWidth, totalWidth * 4, dumpAddr.c_str());
#endif
#endif
}

// Life Cycle
// -----------------------------------------------------------------------------
FireRenderAOV::FireRenderAOV(const FireRenderAOV& other):
	id(other.id),
	attribute(other.attribute),
	name(other.name),
	folder(other.folder),
	description(other.description),
	active(other.active),
	pixels(),
	m_frameWidth(other.m_frameWidth),
	m_frameHeight(other.m_frameHeight),
	m_region(other.m_region)
{
	auto size = m_region.getArea();

	if (size && other.pixels)
	{
		pixels.resize(size);

		memcpy(pixels.get(), other.pixels.get(), size);
	}
}

FireRenderAOV::FireRenderAOV(unsigned int id, const MString& attribute, const MString& name,
	const MString& folder, AOVDescription description) :
	id(id),
	attribute(attribute),
	name(name),
	folder(folder),
	description(description),
	active(false),
	pixels(),
	m_frameWidth(0),
	m_frameHeight(0)
{
}

FireRenderAOV & FireRenderAOV::operator=(const FireRenderAOV & other)
{
	id = other.id;
	attribute = other.attribute;
	name = other.name;
	folder = other.folder;
	description = other.description;
	active = other.active;
	m_frameWidth = other.m_frameWidth;
	m_frameHeight = other.m_frameHeight;
	m_region = other.m_region;

	auto size = m_region.getArea();

	if (size && other.pixels)
	{
		pixels.resize(size);

		memcpy(pixels.get(), other.pixels.get(), size);
	}

	return  *this;
}


// Public Methods
// -----------------------------------------------------------------------------
void FireRenderAOV::setRegion(const RenderRegion& region,
	unsigned int frameWidth, unsigned int frameHeight)
{
	m_region = region;
	m_frameWidth = frameWidth;
	m_frameHeight = frameHeight;
}

// -----------------------------------------------------------------------------
void FireRenderAOV::allocatePixels()
{
	// Only allocate pixels for active AOVs with valid dimensions.
	if (!active || m_region.isZeroArea())
		return;

	pixels.resize(m_region.getArea());
	m_renderStamp = std::unique_ptr<FireMaya::RenderStamp>(new FireMaya::RenderStamp());
}

// -----------------------------------------------------------------------------
void FireRenderAOV::freePixels()
{
	// Check that the pixels have been allocated.
	pixels.reset();
	m_renderStamp.reset();
}

// Check that the AOV is active and in a valid state.
bool FireRenderAOV::IsValid(const FireRenderContext& context) const
{
	if (!active || !pixels || m_region.isZeroArea() || !context.IsAOVSupported(id))
		return false;

	return true;
}

// -----------------------------------------------------------------------------
void FireRenderAOV::readFrameBuffer(FireRenderContext& context, bool flip, bool isDenoiserDisabled /*= false*/)
{
	// Check that the AOV is active and in a valid state.
	if (!active || !pixels || m_region.isZeroArea() || !context.IsAOVSupported(id))
		return;

	bool opacityMerge = context.camera().GetAlphaMask() && context.isAOVEnabled(RPR_AOV_OPACITY);

	// setup params
	FireRenderContext::ReadFrameBufferRequestParams params(m_region);
	params.pixels = pixels.get();
	params.aov = id;
	params.width = m_frameWidth;
	params.height = m_frameHeight;
	params.flip = flip;
	params.mergeOpacity = context.camera().GetAlphaMask() && context.isAOVEnabled(RPR_AOV_OPACITY);
	params.mergeShadowCatcher = true;
	params.isDenoiserDisabled = isDenoiserDisabled;
	params.shadowColor = context.m_shadowColor;
	params.bgColor = context.m_bgColor;
	params.bgWeight = context.m_bgWeight;
	params.shadowTransp = context.m_shadowTransparency;
	params.bgTransparency = context.m_backgroundTransparency;
	params.shadowWeight = context.m_shadowWeight;

	// process frame buffer
	context.readFrameBuffer(params);

	PostProcess();

	// Render stamp, but only when region matches the whole frame buffer
	if (m_region.getHeight() == m_frameHeight && m_region.getWidth() == m_frameWidth && renderStamp.numChars() > 0)
	{
		m_renderStamp->AddRenderStamp(context, pixels.get(), m_frameWidth, m_frameHeight, flip, renderStamp.asChar());
	}
}

// -----------------------------------------------------------------------------
void FireRenderAOV::sendToRenderView()
{
	// Send pixels to the render view.
	MRenderView::updatePixels(m_region.left, m_region.right,
		m_region.bottom, m_region.top, pixels.get(), true);

	// Refresh the render view.
	MRenderView::refresh(0, m_frameWidth - 1, 0, m_frameHeight - 1);
}

// -----------------------------------------------------------------------------
void FireRenderAOV::setRenderStamp(const MString& inRenderStamp)
{
	renderStamp = inRenderStamp;
}

// -----------------------------------------------------------------------------
bool FireRenderAOV::writeToFile(const MString& filePath, bool colorOnly, unsigned int imageFormat, FileWrittenCallback fileWrittenCallback) const
{
	// Check that the AOV is active and in a valid state.
	if (!active || !pixels || m_region.isZeroArea())
		return false;

	// Use the incoming path if only outputting the color AOV,
	// otherwise, get a new path that includes a folder for the AOV.
	MString path = colorOnly ? filePath : getOutputFilePath(filePath);

	// Save the pixels to file.
	FireRenderImageUtil::save(path, m_region.getWidth(), m_region.getHeight(), pixels.get(), imageFormat);

	if (fileWrittenCallback != nullptr)
	{
		fileWrittenCallback(path);
	}

	// For layered PSDs, also save the color AOV file to the
	// base path, as this is where Maya will look for it when
	// creating the PSD file during the post render operation.
	if (id == RPR_AOV_COLOR && !colorOnly && imageFormat == 36)
	{
		FireRenderImageUtil::save(filePath, m_region.getWidth(), m_region.getHeight(), pixels.get(), imageFormat);

		if (fileWrittenCallback != nullptr)
		{
			fileWrittenCallback(filePath);
		}
	}

	return true;
}

const std::string& FireRenderAOV::GetAOVName(int aov_id)
{
	static std::map<unsigned int, std::string> id2name =
	{
		 {RPR_AOV_COLOR, "RPR_AOV_COLOR"}
		,{RPR_AOV_OPACITY, "RPR_AOV_OPACITY" }
		,{RPR_AOV_WORLD_COORDINATE, "RPR_AOV_WORLD_COORDINATE" }
		,{RPR_AOV_UV, "RPR_AOV_UV" }
		,{RPR_AOV_MATERIAL_IDX, "RPR_AOV_MATERIAL_IDX" }
		,{RPR_AOV_GEOMETRIC_NORMAL, "RPR_AOV_GEOMETRIC_NORMAL" }
		,{RPR_AOV_SHADING_NORMAL, "RPR_AOV_SHADING_NORMAL" }
		,{RPR_AOV_DEPTH, "RPR_AOV_DEPTH" }
		,{RPR_AOV_OBJECT_ID, "RPR_AOV_OBJECT_ID" }
		,{RPR_AOV_OBJECT_GROUP_ID, "RPR_AOV_OBJECT_GROUP_ID" }
		,{RPR_AOV_SHADOW_CATCHER, "RPR_AOV_SHADOW_CATCHER" }
		,{RPR_AOV_BACKGROUND, "RPR_AOV_BACKGROUND" }
		,{RPR_AOV_EMISSION, "RPR_AOV_EMISSION" }
		,{RPR_AOV_VELOCITY, "RPR_AOV_VELOCITY" }
		,{RPR_AOV_DIRECT_ILLUMINATION, "RPR_AOV_DIRECT_ILLUMINATION" }
		,{RPR_AOV_INDIRECT_ILLUMINATION, "RPR_AOV_INDIRECT_ILLUMINATION"}
		,{RPR_AOV_AO, "RPR_AOV_AO" }
		,{RPR_AOV_DIRECT_DIFFUSE, "RPR_AOV_DIRECT_DIFFUSE" }
		,{RPR_AOV_DIRECT_REFLECT, "RPR_AOV_DIRECT_REFLECT" }
		,{RPR_AOV_INDIRECT_DIFFUSE, "RPR_AOV_INDIRECT_DIFFUSE" }
		,{RPR_AOV_INDIRECT_REFLECT, "RPR_AOV_INDIRECT_REFLECT" }
		,{RPR_AOV_REFRACT, "RPR_AOV_REFRACT" }
		,{RPR_AOV_VOLUME, "RPR_AOV_VOLUME" }
		,{RPR_AOV_LIGHT_GROUP0, "RPR_AOV_LIGHT_GROUP0" }
		,{RPR_AOV_LIGHT_GROUP1, "RPR_AOV_LIGHT_GROUP1" }
		,{RPR_AOV_LIGHT_GROUP2, "RPR_AOV_LIGHT_GROUP2" }
		,{RPR_AOV_LIGHT_GROUP3, "RPR_AOV_LIGHT_GROUP3" }
		,{RPR_AOV_DIFFUSE_ALBEDO, "RPR_AOV_DIFFUSE_ALBEDO" }
		,{RPR_AOV_VARIANCE, "RPR_AOV_VARIANCE" }
		,{RPR_AOV_VIEW_SHADING_NORMAL, "RPR_AOV_VIEW_SHADING_NORMAL" }
		,{RPR_AOV_REFLECTION_CATCHER, "RPR_AOV_REFLECTION_CATCHER" }
	};

	auto it = id2name.find(aov_id);

	if (it != id2name.end())
		return it->second;

	static std::string errorMsg("ERROR");
	return errorMsg;
}

// Private Methods
// -----------------------------------------------------------------------------
MString FireRenderAOV::getOutputFilePath(const MString& filePath) const
{
	// Split the path at the file name.
	int i = filePath.rindex('/');
	MString path = filePath.substring(0, i);
	MString file = filePath.substring(i + 1, filePath.length() - 1);

	// Add the AOV folder to the path.
	path = path + folder + "/";

	// Ensure the folder exists.
	MCommonSystemUtils::makeDirectory(path);

	// Recombine the file and path.
	return path + file;
}

