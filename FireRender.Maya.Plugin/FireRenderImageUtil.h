#pragma once

#include "FireRenderAOVs.h"
#include <maya/MRenderView.h>
#include <maya/MString.h>

class FireRenderImageUtil
{
public:

	/** Save pixels to file. */
	static void save(MString filePath, unsigned int width, unsigned int height,
		RV_PIXEL* pixels, unsigned int imageFormat);

	/** Save pixels to file using Maya. */
	static void saveMayaImage(MString filePath, unsigned int width, unsigned int height,
		RV_PIXEL* pixels, unsigned int imageFormat);

	/** Save AOVs to a multi-channel file. */
	static void saveMultichannelAOVs(MString filePath,
		unsigned int width, unsigned int height, unsigned int imageFormat, FireRenderAOVs& aovs);

	/** Get an image format string for the given format value. */
	static MString getImageFormatExtension(unsigned int format);
};
