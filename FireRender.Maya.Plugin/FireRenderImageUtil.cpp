#include "common.h"
#include "frWrap.h"
#include "FireRenderImageUtil.h"
#include <maya/MGlobal.h>
#include <maya/MImage.h>
#include <string>
#include <memory>

using namespace std;
using namespace OIIO;

// -----------------------------------------------------------------------------
void FireRenderImageUtil::save(MString filePath, unsigned int width, unsigned int height,
	RV_PIXEL* pixels, unsigned int imageFormat)
{
	// Get the UTF8 file name.
	auto fileName = filePath.asUTF8();

	// Create and verify the image output.
	auto output = unique_ptr<ImageOutput>(ImageOutput::create(fileName));
	if (!output)
	{
		// Handle any case where the image format
		// suffix was not included with the file name.
		auto nameWithExt = filePath + "." + getImageFormatExtension(imageFormat);
		output = unique_ptr<ImageOutput>(ImageOutput::create(nameWithExt.asUTF8()));

		// Fall back to Maya image saving if OpenImageIO
		// was not able to create the image output.
		if (!output)
		{
			saveMayaImage( filePath, width, height, pixels, imageFormat);
			return;
		}
	}

	// Not using more complex constructor as OIIO allocates data in a different heap
	// and modifying std::vector in our heap crashes(seems line CRTs don't match for
	// the plugin and OpenImageIO.dll that gets loaded).
	ImageSpec imgSpec(width, height, 4, TypeDesc::FLOAT);
	auto comments = "Created with "s + FIRE_RENDER_NAME + " "s + PLUGIN_VERSION;
	imgSpec.attribute("ImageDescription", comments.c_str());

	// Try to open and write to the file.
	if (output->open(fileName, imgSpec))
	{
		output->write_image(TypeDesc::FLOAT, reinterpret_cast<uint8_t*>(pixels));
		output->close();
	}
	else
	{
		int extDot = filePath.rindex('.');
		int len = filePath.length();
		auto ext = filePath.substring(extDot + 1, len);

		if (ext.toLowerCase() != MString("cin"))
		{
			// Fall back to Maya image saving if
			// OpenImageIO wasn't able to write the file.
			saveMayaImage( filePath, width, height, pixels, imageFormat);
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
void FireRenderImageUtil::saveMultichannelAOVs(MString filePath,
	unsigned int width, unsigned int height, unsigned int imageFormat, FireRenderAOVs& aovs)
{
	int aovs_component_count[RPR_AOV_MAX] = { 0 };

	auto outImage = OIIO::ImageOutput::create(filePath.asUTF8());
	if( !outImage )
	{
		auto nameWithExt = filePath + "." + getImageFormatExtension( imageFormat );
		outImage = ImageOutput::create( nameWithExt.asUTF8() );
		if( !outImage )
		{
			return;
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

	//fill image spec setting up channels for each aov
	for (int i = 0; i != aovs.getNumberOfAOVs(); ++i)
	{
		FireRenderAOV& aov = aovs.getAOV(i);

		if (aov.active)
		{
			int aov_component_count = 0;

			for (auto c : aov.description.components)
			{
				if (c)
				{
				++aov_component_count;
					++imgSpec.nchannels;
					imgSpec.channelformats.push_back(TypeDesc::FLOAT);

					std::string name;
					if (0 == i)
						name = c;//standard name for COLOR channel
					else
						name = std::string(aov.folder.asChar()) + "." + c;

					imgSpec.channelnames.push_back(name);
				}
			}
			aovs_component_count[i] = aov_component_count;
		}
	}

	size_t pixel_size = imgSpec.pixel_bytes(true) / sizeof(float);
	std::vector<float> pixels_for_oiio;
	pixels_for_oiio.resize(imgSpec.image_pixels() * pixel_size * aovs.getNumberOfAOVs());

	//interleave aov components for OIIO(each pixel contains all channels data)
	for (int y = 0; y != height; ++y)
	{
		for (int x = 0; x != width; ++x)
		{
			int pixel_index = x + y * width;

			float* pixel = pixels_for_oiio.data() + pixel_size * pixel_index;
			for (int i = 0; i != RPR_AOV_MAX; ++i)
			{
				FireRenderAOV& aov = aovs.getAOV(i);
				int aov_component_count = aovs_component_count[i];
				if (aov_component_count)
				{
					auto aov_pixel = aov.pixels.get()[pixel_index];
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
}

// -----------------------------------------------------------------------------
MString FireRenderImageUtil::getImageFormatExtension(unsigned int format)
{
	// Convert the code to an image format string.
	MString extension;

	switch (format)
	{
	case 0: extension = "gif"; break;
	case 1: extension = "si"; break;
	case 2: extension = "rla"; break;
	case 3: extension = "tif"; break;
	case 4: extension = "tif"; break;
	case 5: extension = "sgi"; break;
	case 6: extension = "als"; break;
	case 7: extension = "iff"; break;
	case 8: extension = "jpg"; break;
	case 9: extension = "eps"; break;
	case 10: extension = "iff"; break;
	case 11: extension = "cin"; break;
	case 12: extension = "yuv"; break;
	case 13: extension = "sgi"; break;
	case 19: extension = "tga"; break;
	case 20: extension = "bmp"; break;
	case 21: extension = "sgimv"; break;
	case 22: extension = "mov"; break;
	case 23: extension = "avi"; break;
	case 24: extension = "mov"; break;
	case 31: extension = "psd"; break;
	case 32: extension = "png"; break;
	case 33: extension = "mov"; break;
	case 34: extension = "mov"; break;
	case 35: extension = "dds"; break;
	case 36: extension = "iff"; break;
	case 40: extension = "exr"; break;
	case 50: extension = "imf"; break;
	case 51: extension = "custom"; break;
	case 60: extension = "swf"; break;
	case 61: extension = "ai"; break;
	case 62: extension = "svg"; break;
	case 63: extension = "swft"; break;
	case 163: extension = "exr"; break;

		// Format not supported, use the Maya native iff format.
	default:
		extension = "iff";
	}

	return extension;
}
