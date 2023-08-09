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

#include "RenderViewUpdater.h"

#include <maya/MCommonSystemUtils.h>
#include <maya/MViewport2Renderer.h>
#include <maya/MGlobal.h>

#include <ostream>


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
		unsigned int inputIndex = y * regionWidth;

		unsigned int destShiftY = y + totalHeight - region.top - 1;
		unsigned int destIndex = region.left + (destShiftY) * totalWidth;

		memcpy(&m_pBuffer[destIndex], &input[inputIndex], sizeof(RV_PIXEL) * regionWidth);
	}

#ifdef _DEBUG
#ifdef DUMP_PIXELS_PIXELBUFF
	const std::string pathToFile = "C://debug//fb//";

	debugDump(totalHeight, totalWidth, std::string(FireRenderAOV::GetAOVName(aov_id) + "_tile_"), pathToFile);
#endif
#endif
}

#ifdef _DEBUG
void generateBitmapImage(unsigned char *image, int height, int width, int pitch, const char* imageFileName);
#endif

void PixelBuffer::debugDump(unsigned int height, unsigned int width, const std::string& fbName, const std::string& pathToFile)
{
#ifdef _DEBUG
	assert(sizeof(RV_PIXEL) * height * width <= m_size);

	std::vector<RV_PIXEL> sourcePixels;
	sourcePixels.reserve(height * width);

	for (unsigned int y = 0; y < height; y++)
	{
		for (unsigned int x = 0; x < width; x++)
		{
			RV_PIXEL pixel = m_pBuffer[x + y * width];
			sourcePixels.push_back(pixel);
		}
	}

	std::vector<unsigned char> buffer2;
	buffer2.reserve(height * width);

	for (unsigned int y = 0; y < height; y++)
	{
		for (unsigned int x = 0; x < width; x++)
		{
			RV_PIXEL& pixel = sourcePixels[x + y * width];
			char r = (char) (255 * pixel.r);
			char g = (char) (255 * pixel.g);
			char b = (char) (255 * pixel.b);

			buffer2.push_back(b);
			buffer2.push_back(g);
			buffer2.push_back(r);
			buffer2.push_back(255);
		}
	}

	static int debugDumpIdx = 0;
	std::string dumpAddr = pathToFile + fbName +std::to_string(debugDumpIdx++) + ".bmp";
	unsigned char* dst2 = buffer2.data();
	generateBitmapImage(dst2, height, width, width * 4, dumpAddr.c_str());
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
void FireRenderAOV::readFrameBuffer(FireRenderContext& context)
{
	// Check that the AOV is active and in a valid state.
	if (!active || !pixels || m_region.isZeroArea() || !context.IsAOVSupported(id))
		return;

	// It special aov, skip it here
	if (id == RPR_AOV_DEEP_COLOR)
	{
		return;
	}

	bool hasAlphaMask = context.camera().GetAlphaMask();
	bool opacityMerge = hasAlphaMask && context.isAOVEnabled(RPR_AOV_OPACITY) && !context.IsTileRender();

	// setup params
	FireRenderContext::ReadFrameBufferRequestParams params(m_region);
	params.pixels = pixels.get();
	params.aov = id;
	params.width = m_frameWidth;
	params.height = m_frameHeight;
	params.mergeOpacity = opacityMerge;
	params.mergeShadowCatcher = true;
	params.shadowColor = context.m_shadowColor;
	params.shadowTransp = context.m_shadowTransparency;
	params.shadowWeight = context.m_shadowWeight;

	// process frame buffer
	context.readFrameBuffer(params);

	PostProcess();

	// Render stamp, but only when region matches the whole frame buffer
	if (m_region.getHeight() == m_frameHeight && m_region.getWidth() == m_frameWidth && renderStamp.numChars() > 0 && !context.IsTileRender())
	{
		m_renderStamp->AddRenderStamp(context, pixels.get(), m_frameWidth, m_frameHeight, renderStamp.asChar());
	}
}

// -----------------------------------------------------------------------------
void FireRenderAOV::sendToRenderView()
{
	RenderViewUpdater::UpdateAndRefreshRegion(
		pixels.get(),
		m_region.getWidth(),
		m_region.getHeight(),
		m_region);
}

// -----------------------------------------------------------------------------
void FireRenderAOV::setRenderStamp(const MString& inRenderStamp)
{
	renderStamp = inRenderStamp;
}

// -----------------------------------------------------------------------------

bool FireRenderAOV::IsCryptomateiralAOV(void) const
{
	if		(id == RPR_AOV_CRYPTOMATTE_MAT0)		{ return true; } 
	else if (id == RPR_AOV_CRYPTOMATTE_MAT1)		{ return true; }
	else if	(id == RPR_AOV_CRYPTOMATTE_MAT2)		{ return true; }
	else if (id == RPR_AOV_CRYPTOMATTE_OBJ0)		{ return true; }
	else if (id == RPR_AOV_CRYPTOMATTE_OBJ1)		{ return true; }
	else if (id == RPR_AOV_CRYPTOMATTE_OBJ2)		{ return true; }

	return false;
}

void FireRenderAOV::SaveDeepExrFrameBuffer(FireRenderContext& context, const std::string& filePath) const
{
	std::string exrFilePath;
	size_t index = filePath.find_last_of(".");
	const std::string exrExt = ".exr";

	if (index == std::string::npos)
	{
		exrFilePath += exrExt;
	}
	else
	{
		exrFilePath = filePath.substr(0, index) + exrExt;
	}

	rprFrameBufferSaveToFile(context.frameBufferAOV(RPR_AOV_DEEP_COLOR), exrFilePath.c_str());
}

// -----------------------------------------------------------------------------
bool FireRenderAOV::writeToFile(FireRenderContext& context, const MString& filePath, bool colorOnly, unsigned int imageFormat, FileWrittenCallback fileWrittenCallback) const
{
	// Check that the AOV is active and in a valid state.
	if (!active || !pixels || m_region.isZeroArea())
		return false;

	// do not write Shading normal, Object ID, Material Index, and UV for contour as they are overwritten per iteration
	bool isContour = context.Globals().contourIsEnabled;
	if (isContour && (id == RPR_AOV_MATERIAL_ID || id == RPR_AOV_SHADING_NORMAL || id == RPR_AOV_OBJECT_ID || id == RPR_AOV_UV))
	{
		return true;
	}

	// Use the incoming path if only outputting the color AOV,
	// otherwise, get a new path that includes a folder for the AOV.
	MString path = colorOnly ? filePath : getOutputFilePath(filePath);

	if (id == RPR_AOV_DEEP_COLOR)
	{
		SaveDeepExrFrameBuffer(context, path.asChar());

		return true;
	}

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
		,{RPR_AOV_MATERIAL_ID, "RPR_AOV_MATERIAL_IDX" }
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
		,{RPR_AOV_CAMERA_NORMAL, "RPR_AOV_CAMERA_NORMAL"}
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
	// replace '\' with '/'
	std::string tmp = filePath.asChar();
	std::replace(tmp.begin(), tmp.end(), '\\', '/');
	// MString only accepts char*, not std::string
	MString newFilePath = tmp.c_str();

	// Split the path at the file name.
	int i = newFilePath.rindex('/');
	MString path = newFilePath.substring(0, i);
	MString file = newFilePath.substring(i + 1, newFilePath.length() - 1);

	// Add the AOV folder to the path.
	path = path + folder + "/";

	// Ensure the folder exists.
	MCommonSystemUtils::makeDirectory(path);

	// Recombine the file and path.
	return path + file;
}

