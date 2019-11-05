#include "common.h"
#include "frWrap.h"
#include "FireRenderImageUtil.h"
#include <maya/MGlobal.h>
#include <maya/MImage.h>
#include <string>
#include <memory>
#ifdef __APPLE__
#include <OpenImageIO/color.h>
#else
#include <color.h>
#endif

// -----------------------------------------------------------------------------
void FireRenderImageUtil::save(MString filePath, unsigned int width, unsigned int height,
	RV_PIXEL* pixels, unsigned int imageFormat)
{
	// Get the UTF8 file name.
	const char* fileName = filePath.asUTF8();

	// Create and verify the image output.
	std::unique_ptr<ImageOutput> output = std::unique_ptr<ImageOutput>(ImageOutput::create(fileName));

	if (!output)
	{
		// Handle any case where the image format
		// suffix was not included with the file name.
		auto nameWithExt = filePath + "." + getImageFormatExtension(imageFormat);
		output = std::unique_ptr<ImageOutput>(ImageOutput::create(nameWithExt.asUTF8()));

		// Fall back to Maya image saving if OpenImageIO
		// was not able to create the image output.
		if (!output)
		{
			saveMayaImage(filePath, width, height, pixels, imageFormat);
			return;
		}
	}

	// Not using more complex constructor as OIIO allocates data in a different heap
	// and modifying std::vector in our heap crashes(seems line CRTs don't match for
	// the plugin and OpenImageIO.dll that gets loaded).
	const int numberOfChannels = sizeof(RV_PIXEL) / sizeof(float);
	ImageSpec imgSpec(width, height, numberOfChannels, TypeDesc::FLOAT);
	std::string comments = std::string() + "Created with " + FIRE_RENDER_NAME + " " + PLUGIN_VERSION;
	imgSpec.attribute("ImageDescription", comments.c_str());

	bool saveSuccessful = false;
	std::vector<float> buff;

	bool isEXR = getImageFormatExtension(imageFormat) == "exr";

	if (isEXR) //The RGB values in OpenEXR are linear, thus conversion is required
	{
		int buffSize = width * height * numberOfChannels;
		buff.reserve(buffSize);
		float* fpixels = reinterpret_cast<float*> (pixels);

		for (int idx = 0; idx < buffSize; ++idx)
			buff.push_back(sRGB_to_linear(fpixels[idx]));
	}

	// Try to open and write to the file.
	if (output->open(fileName, imgSpec))
	{
		const uint8_t* ptr = isEXR ? reinterpret_cast<uint8_t*>(buff.data()) : reinterpret_cast<uint8_t*>(pixels);
		saveSuccessful = output->write_image(TypeDesc::FLOAT, ptr);
		output->close();
	}
	
	if (!saveSuccessful)
	{
		int extDot = filePath.rindex('.');
		int len = filePath.length();
		auto ext = filePath.substring(extDot + 1, len);

		if (ext.toLowerCase() != MString("cin"))
		{
			// Fall back to Maya image saving if
			// OpenImageIO wasn't able to write the file.
			saveMayaImage(filePath, width, height, pixels, imageFormat);
		}
	}
}

// -----------------------------------------------------------------------------
void FireRenderImageUtil::saveMayaImage(MString filePath, unsigned int width, unsigned int height,
	RV_PIXEL* pixels, unsigned int imageFormat)
{
	// Maya requires the red and blue channels to be
	// switched. MImage.setRGBA() looks like it should
	// be able to do this, but it has no effect.
	unsigned int pixelCount = width * height;

	for (unsigned int i = 0; i < pixelCount; i++)
	{
		float r = pixels[i].r;
		pixels[i].r = pixels[i].b;
		pixels[i].b = r;
	}

	MStatus status;

	try
	{
		// Save the image.
		MImage image;
		image.create(width, height, 4u, MImage::kFloat);
		image.setRGBA(true);
		image.setFloatPixels(reinterpret_cast<float*>(pixels), width, height);
		image.convertPixelFormat(MImage::kByte);
		image.verticalFlip();

		auto extension = getImageFormatExtension(imageFormat);
		if (imageFormat == 50)	// Special case for Sony & XPM file formats
		{
			int extDot = filePath.rindex('.');
			if (extDot > 0)
			{
				int len = filePath.length();
				auto ext = filePath.substring(extDot + 1, len);

				if (ext.length() == 3)
					extension = ext;
			}
		}

		status = image.writeToFile(filePath, extension);
	}
	catch (...)
	{
		status = MStatus::kInvalidParameter;
	}

	if (status != MS::kSuccess)
		MGlobal::displayError("Unable to save " + filePath);
}

// -----------------------------------------------------------------------------
bool FireRenderImageUtil::saveMultichannelAOVs(MString filePath,
	unsigned int width, unsigned int height, unsigned int imageFormat, FireRenderAOVs& aovs)
{
	int aovs_component_count[RPR_AOV_MAX] = { 0 };

	auto outImage = OIIO::ImageOutput::create(filePath.asUTF8());
	if (!outImage)
	{
		auto nameWithExt = filePath + "." + getImageFormatExtension(imageFormat);
		outImage = ImageOutput::create(nameWithExt.asUTF8());
		if (!outImage)
		{
			return false;
		}
	}

	// Not using more complex constructor as OIIO allocates data in a different heap
	// and modifying std::vector in our heap crashes(seems line CRTs don't match for
	// the plugin and OpenImageIO.dll that gets loaded).
	OIIO::ImageSpec imgSpec;
	imgSpec.width = width;
	imgSpec.height = height;

	const char* comments = "Created with " FIRE_RENDER_NAME " " PLUGIN_VERSION;
	imgSpec.attribute("ImageDescription", comments);
	imgSpec.attribute("compression", aovs.GetEXRCompressionType().asChar());

	imgSpec.set_format(aovs.GetChannelFormat());

	//fill image spec setting up channels for each aov
	for (int i = 0; i < RPR_AOV_MAX; ++i)
	{
		FireRenderAOV* aov = aovs.getAOV(i);

		if (aov != nullptr && aov->active)
		{
			int aov_component_count = 0;

			for (auto c : aov->description.components)
			{
				if (c)
				{
					++aov_component_count;
					++imgSpec.nchannels;

					std::string name;
					if (0 == i)
						name = c;//standard name for COLOR channel
					else
						name = std::string(aov->folder.asChar()) + "." + c;

					imgSpec.channelnames.push_back(name);
				}
			}
			aovs_component_count[i] = aov_component_count;
		}
	}

	size_t pixel_size = imgSpec.nchannels;
	std::vector<float> pixels_for_oiio;
	pixels_for_oiio.resize(imgSpec.image_pixels() * pixel_size);

	//interleave aov components for OIIO(each pixel contains all channels data)
	for (unsigned int y = 0; y < height; ++y)
	{
		for (unsigned int x = 0; x < width; ++x)
		{
			unsigned int pixel_index = x + y * width;

			float* pixel = pixels_for_oiio.data() + pixel_size * pixel_index;

			for (int i = 0; i < RPR_AOV_MAX; ++i)
			{
				FireRenderAOV* aov = aovs.getAOV(i);
				int aov_component_count = aovs_component_count[i];
				if (aov_component_count)
				{
					auto aov_pixel = aov->pixels.get()[pixel_index];
					pixel = std::copy(&aov_pixel.r, (&aov_pixel.r) + aov_component_count, pixel);
				}
			}
		}
	}

	if (outImage->open(filePath.asUTF8(), imgSpec))
	{
		outImage->write_image(OIIO::TypeDesc::FLOAT, pixels_for_oiio.data());
		outImage->close();
	}

	delete outImage;

	//This is to deallocate from our module, else it will be released in the openimageio.dll
	//(in obj destructor) and lead to crash. Because we have filled those arrays in our code,
	//oiio has no api functions to fill those safely(so that vector operations are called from ooio code)
	//unfortunately
	std::vector<OIIO::TypeDesc> temp0 = std::vector<OIIO::TypeDesc>();
	imgSpec.channelformats.swap(temp0);
	std::vector<std::string> temp1 = std::vector<std::string>();
	imgSpec.channelnames.swap(temp1);

	return true;
}

static const std::map<unsigned int, std::string> imageFormats =
{
	{ 0, "gif" },
	{ 1, "si" },
	{ 2, "rla" },
	{ 3, "tif" },
	{ 4, "tif" },
	{ 5, "sgi" },
	{ 6, "als" },
	{ 7, "iff" },
	{ 8, "jpg" },
	{ 9, "eps" },
	{ 10, "iff" },
	{ 11, "cin" },
	{ 12, "yuv" },
	{ 13, "sgi" },
	{ 19, "tga" },
	{ 20, "bmp" },
	{ 21, "sgimv" },
	{ 22, "mov" },
	{ 23, "avi" },
	{ 24, "mov" },
	{ 31, "psd" },
	{ 32, "png" },
	{ 33, "mov" },
	{ 34, "mov" },
	{ 35, "dds" },
	{ 36, "iff" },
	{ 40, "exr" },
	{ 50, "imf" },
	{ 51, "custom" },
	{ 60, "swf" },
	{ 61, "ai" },
	{ 62, "svg" },
	{ 63, "swft" },
	{ 163, "exr" },
};

// -----------------------------------------------------------------------------
MString FireRenderImageUtil::getImageFormatExtension(unsigned int format)
{
	// Convert the code to an image format string.
	auto it = imageFormats.find(format);

	MString extension = (it != imageFormats.end()) ? it->second.c_str() : "iff"; // if format not supported, use the Maya native iff format.

	return extension;
}
