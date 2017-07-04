#include "frWrap.h"

#include "FireRenderAOV.h"
#include "FireRenderContext.h"
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

// -----------------------------------------------------------------------------
void FireRenderAOV::readFrameBuffer(FireRenderContext& context, bool flip)
{
	// Check that the AOV is active and in a valid state.
	if (!active || !pixels || m_region.isZeroArea())
		return;

	bool isColor = id == RPR_AOV_COLOR;
	context.readFrameBuffer(pixels.get(),
		context.frameBufferAOV_Resolved(id),
		m_frameWidth,
		m_frameHeight,
		m_region,
		flip,
		isColor,
		context.framebufferAOV(RPR_AOV_OPACITY));

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

	// No further action is required for Maya versions higher than 2016.
	if (MGlobal::apiVersion() >= 201650)
		return;

#ifndef MAYA2015

	// Further action is only required for the core profile renderer.
	MHWRender::MRenderer* renderer = MHWRender::MRenderer::theRenderer();
	if (renderer->drawAPI() != MHWRender::kOpenGLCoreProfile)
		return;

	// Stop the render. This forces a render view
	// refresh that is not correctly performed by the
	// MRenderView::refresh in Maya 2016 when using the
	// Core OpenGL rendering engine. This also prevents
	// a memory leak in Maya.
	MRenderView::endRender();

	// Restart the render.
	if (m_region.getWidth() == m_frameWidth && m_region.getHeight() == m_frameHeight)
	{
		MRenderView::startRegionRender(m_frameWidth, m_frameHeight,
			m_region.left, m_region.right,
			m_region.bottom, m_region.top, true, false);
	}
	else
		MRenderView::startRender(m_frameWidth, m_frameHeight, true, false);
#endif
}

// -----------------------------------------------------------------------------
void FireRenderAOV::setRenderStamp(const MString& inRenderStamp)
{
	renderStamp = inRenderStamp;
}

// -----------------------------------------------------------------------------
bool FireRenderAOV::writeToFile(const MString& filePath, bool colorOnly, unsigned int imageFormat) const
{
	// Check that the AOV is active and in a valid state.
	if (!active || !pixels || m_region.isZeroArea())
		return false;

	// Use the incoming path if only outputting the color AOV,
	// otherwise, get a new path that includes a folder for the AOV.
	MString path = colorOnly ? filePath : getOutputFilePath(filePath);

	// Save the pixels to file.
	FireRenderImageUtil::save(path, m_region.getWidth(), m_region.getHeight(), pixels.get(), imageFormat);

	// For layered PSDs, also save the color AOV file to the
	// base path, as this is where Maya will look for it when
	// creating the PSD file during the post render operation.
	if (id == RPR_AOV_COLOR && !colorOnly && imageFormat == 36)
		FireRenderImageUtil::save(filePath, m_region.getWidth(), m_region.getHeight(), pixels.get(), imageFormat);

	return true;
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

