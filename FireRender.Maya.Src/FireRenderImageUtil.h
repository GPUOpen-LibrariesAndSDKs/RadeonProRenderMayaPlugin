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
#pragma once

#include "FireRenderAOVs.h"
#include <maya/MRenderView.h>
#include <maya/MString.h>

enum EXRCompressionMethod
{
	EXRCM_NONE = 0,
	EXRCM_RLE,
	EXRCM_ZIP,
	EXRCM_PIZ,
	EXRCM_PXR24,
	EXRCM_B44,
	EXRCM_B44A,
	EXRCM_DWAA,
	EXRCM_DWAB
};

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
	static bool saveMultichannelAOVs(MString filePath,
		unsigned int width, unsigned int height, unsigned int imageFormat, FireRenderAOVs& aovs);

	/** Get an image format string for the given format value. */
	static MString getImageFormatExtension(unsigned int format);
};
