#pragma once
#include <maya/MString.h>
#include "RenderRegion.h"
#include "RenderStamp.h"
#include <memory>

// Maya 2015 has min/max defined, what prevents imageio.h from being compiled
#undef min
#undef max

#ifdef OSMac_
#include <OpenImageIO/imageio.h>
#else
#include <imageio.h>
#endif


// Forward declarations.
class FireRenderContext;
#if MAYA_API_VERSION < 20180000
class MFnDependencyNode;
#endif
struct RV_PIXEL;


/** AOV description of channel components and data types. */
struct AOVDescription
{
	const char* components[4];
	OIIO::TypeDesc types;
};

/** Automated handler for RV_PIXEL data.
	Uses aligned memory manager and prevents unnecessary re-allocations */
class PixelBuffer
{
	RV_PIXEL * m_pBuffer;
	size_t m_size;
	size_t m_width;
	size_t m_height;

public:
	PixelBuffer() 
		:	m_pBuffer(nullptr)
		,	m_size(0)
		,	m_width(0)
		,	m_height(0)
	{
	}
	virtual ~PixelBuffer()
	{
		reset();
	}

public:
	operator bool() const
	{
		return m_pBuffer != nullptr;
	}

	RV_PIXEL * const get() const
	{
		return m_pBuffer;
	}

	size_t size() const
	{
		return m_size;
	}

	size_t width() const
	{
		return m_width;
	}

	size_t height() const
	{
		return m_height;
	}

	float* data()
	{
		return (float*)m_pBuffer;
	}

	void resize(size_t newCount);

	void resize(size_t width, size_t height)
	{
		m_width = width;
		m_height = height;

		resize(width*height);
	}

	void reset()
	{
		if (m_pBuffer)
		{
#ifdef WIN32
			_aligned_free(m_pBuffer);
#else
			free(m_pBuffer);
#endif
		}
		m_pBuffer = nullptr;
		m_size = 0;
	}

	void overwrite(const RV_PIXEL* input, const RenderRegion& region, unsigned int totalHeight, unsigned int totalWidth, int aov_id = 0);

	void debugDump(unsigned int totalHeight, unsigned int totalWidth, std::string& fbName);
};

/** AOV data. */
class FireRenderAOV
{
public:

	// Life Cycle
	// -----------------------------------------------------------------------------
	FireRenderAOV(const FireRenderAOV& other);

	FireRenderAOV(unsigned int id, const MString& attribute, const MString& name,
		const MString& folder, AOVDescription description);

	virtual ~FireRenderAOV() {}

public:
	FireRenderAOV& operator=(const FireRenderAOV& other);

	// Public Methods
	// -----------------------------------------------------------------------------
	/** Check that the AOV is active and in a valid state. */
	bool IsValid(const FireRenderContext& context) const;

	/** Set the size of the AOVs, including the full frame buffer size. */
	void setRegion(const RenderRegion& region,
		unsigned int frameWidth, unsigned int frameHeight);

	/** Allocate pixels for active AOVs. */
	void allocatePixels();

	/** Free pixels for active AOVs. */
	void freePixels();

	/** Read the frame buffer pixels for this AOV. */
	void readFrameBuffer(FireRenderContext& context, bool flip, bool isDenoiserDisabled = false);

	/** Send the AOV pixels to the Maya render view. */
	void sendToRenderView();

	/** Write the AOV to file. */
	typedef void(*FileWrittenCallback)(const MString&);
	bool writeToFile(const MString& filePath, bool colorOnly, unsigned int imageFormat, FileWrittenCallback fileWrittenCallback = nullptr) const;

	/** Get an AOV output path for the given file path. */
	MString getOutputFilePath( const MString& filePath ) const;

	/** Setup render stamp */
	void setRenderStamp(const MString& renderStamp);

	// Properties
	// -----------------------------------------------------------------------------

	/** The numeric AOV ID. */
	unsigned int id;

	/** The RPR globals attribute name. */
	MString attribute;

	/** The AOV name for display in the UI. */
	MString name;

	/** The AOV output folder (or channel in a multi-channel EXR). */
	MString folder;

	/** Render stamp template. Empty string when not used. */
	MString renderStamp;

	/** The AOV data format description. */
	AOVDescription description;

	/** True if this AOV is active for the current render. */
	bool active;

	/** AOV pixel data. Allocated if the buffer is active. */
	PixelBuffer pixels;

	/** Getting settings for making post processing*/
	virtual void ReadFromGlobals(const MFnDependencyNode& globals) {}

	static const std::string& GetAOVName(int aov_id);

protected:
	/** Make post processing for the specific AOV if required*/
	virtual void PostProcess(){}

protected:
	// Members
	// -----------------------------------------------------------------------------

	/** AOV region. */
	RenderRegion m_region;

	/** Frame buffer width. */
	unsigned int m_frameWidth;

	/** Frame buffer height. */
	unsigned int m_frameHeight;

	/** Render Stamp */
	std::unique_ptr<FireMaya::RenderStamp>	m_renderStamp;
};
