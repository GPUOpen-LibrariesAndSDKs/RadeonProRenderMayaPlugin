#include "FireRenderImageComparing.h"

#include <RadeonProRender.h> // for get FR_API_VERSION
#include "common.h"
#include <sstream>

#include <maya/MDoubleArray.h>
#include <maya/MStringArray.h>
#include <maya/MImage.h>

void * FireRenderImageComparing::creator()
{
	return new FireRenderImageComparing;
}

MSyntax FireRenderImageComparing::newSyntax()
{
	MStatus status;
	MSyntax syntax;

	CHECK_MSTATUS(syntax.addFlag(kImageGPU, kImageGPULong, MSyntax::kString));
	CHECK_MSTATUS(syntax.addFlag(kImageBaseLineGPU, kImageBaseLineGPULong, MSyntax::kString));
	CHECK_MSTATUS(syntax.addFlag(kImageCPU, kImageCPULong, MSyntax::kString));
	CHECK_MSTATUS(syntax.addFlag(kImageBaseLineCPU, kImageBaseLineCPULong, MSyntax::kString));
	CHECK_MSTATUS(syntax.addFlag(kImageMixed, kImageMixedLong, MSyntax::kString));
	CHECK_MSTATUS(syntax.addFlag(kImageBaseLineMixed, kImageBaseLineMixedLong, MSyntax::kString));
	CHECK_MSTATUS(syntax.addFlag(kRprPluginDetails, kRprPluginDetailsLong, MSyntax::kNoArg));

	return syntax;
}

float getAvarageDifference(MString imagePath_1, MString imagePath_2) {
	if (imagePath_1.length() == 0 ||
		imagePath_2.length() == 0) {
		return -1;
	}

	MImage image1;
	image1.readFromFile(imagePath_1);

	MImage image2;
	image2.readFromFile(imagePath_2);

	unsigned int width1, height1, width2, height2;

	image1.getSize(width1, height1);
	unsigned int size1 = width1 * height1;

	image2.getSize(width2, height2);
	unsigned int size2 = width2 * height2;

	if (size1 == 0 || size2 == 0) {
		//file doesn't exist.
		return -1;
	}

	if (size1 != size2 || width1 != width2 || height1 != height2 || size1 == 0)
	{
		//size is not identical
		return -2;
	}
	else
	{
		// rgba
		unsigned char *pixels1 = image1.pixels();
		unsigned char *pixels2 = image2.pixels();
		size_t index = 0;
		int r1, r2, g1, g2, b1, b2;
		float pixelAvarage, rd, gd, bd;
		float sumAvarage = 0;
		for (size_t i = 0; i < size1; i++)
		{
			index = i * 4;
			r1 = pixels1[index];	 r2 = pixels2[index];		rd = abs(r1 - r2) / 255.0f;
			g1 = pixels1[index + 1]; g2 = pixels2[index + 1];	gd = abs(g1 - g2) / 255.0f;
			b1 = pixels1[index + 2]; b2 = pixels2[index + 2];	bd = abs(b1 - b2) / 255.0f;
			pixelAvarage = (rd + gd + bd) / 3.0f;
			sumAvarage += pixelAvarage;
		}
		sumAvarage /= (1.0f * size1);
		float percentage = sumAvarage * 100.0f;
		return percentage;
	}
	return -1;
}

MStatus FireRenderImageComparing::doIt(const MArgList & args)
{
	MStatus status;

	MArgDatabase argData(syntax(), args);
	//0: GPU vs BaseLine GPU
	//1: CPU vs BaseLine CPU
	//2: Mixed vs BaseLine Mixed
	//3: GPU vs CPU
	//4: GPU vs Mixed
	//-1 result: file not set
	//-2 result: size not identical
	MDoubleArray doubleArray;
	if (argData.isFlagSet(kRprPluginDetails) && argData.isFlagSet(kRprPluginDetailsLong))
	{
		std::string pluginVersion = PLUGIN_VERSION;

		std::stringstream s;
#ifdef RPR_VERSION_MAJOR_MINOR_REVISION
		s << "0x" << std::hex << RPR_VERSION_MAJOR_MINOR_REVISION;
#else
		s << "0x" << std::hex << RPR_API_VERSION;
#endif
		std::string rprApiVersion = s.str();

		std::string mayaVersion = std::to_string(MAYA_API_VERSION);

		MStringArray stringArray;
		stringArray.append(pluginVersion.c_str());
		stringArray.append(rprApiVersion.c_str());
		stringArray.append(mayaVersion.c_str());

		setResult(stringArray);

		return MS::kSuccess;
	}

	if (argData.isFlagSet(kImageGPU) && argData.isFlagSet(kImageBaseLineGPU))
	{
		MString imagePath_1;
		MString imagePath_2;

		argData.getFlagArgument(kImageGPU, 0, imagePath_1);
		argData.getFlagArgument(kImageBaseLineGPU, 0, imagePath_2);

		float avgDiff = getAvarageDifference(imagePath_1, imagePath_2);

		doubleArray.append(avgDiff);
	}
	else
	{
		doubleArray.append(-1);
	}

	if (argData.isFlagSet(kImageCPU) && argData.isFlagSet(kImageBaseLineCPU))
	{
		MString imagePath_1;
		MString imagePath_2;

		argData.getFlagArgument(kImageCPU, 0, imagePath_1);
		argData.getFlagArgument(kImageBaseLineCPU, 0, imagePath_2);

		float avgDiff = getAvarageDifference(imagePath_1, imagePath_2);

		doubleArray.append(avgDiff);
	}
	else
	{
		doubleArray.append(-1);
	}

	if (argData.isFlagSet(kImageMixed) && argData.isFlagSet(kImageBaseLineMixed))
	{
		MString imagePath_1;
		MString imagePath_2;

		argData.getFlagArgument(kImageMixed, 0, imagePath_1);
		argData.getFlagArgument(kImageBaseLineMixed, 0, imagePath_2);

		float avgDiff = getAvarageDifference(imagePath_1, imagePath_2);

		doubleArray.append(avgDiff);
	}
	else
	{
		doubleArray.append(-1);
	}

	if (argData.isFlagSet(kImageGPU) && argData.isFlagSet(kImageCPU))
	{
		MString imagePath_1;
		MString imagePath_2;

		argData.getFlagArgument(kImageGPU, 0, imagePath_1);
		argData.getFlagArgument(kImageCPU, 0, imagePath_2);

		float avgDiff = getAvarageDifference(imagePath_1, imagePath_2);

		doubleArray.append(avgDiff);
	}
	else
	{
		doubleArray.append(-1);
	}

	if (argData.isFlagSet(kImageGPU) && argData.isFlagSet(kImageMixed))
	{
		MString imagePath_1;
		MString imagePath_2;

		argData.getFlagArgument(kImageGPU, 0, imagePath_1);
		argData.getFlagArgument(kImageMixed, 0, imagePath_2);

		float avgDiff = getAvarageDifference(imagePath_1, imagePath_2);

		doubleArray.append(avgDiff);
	}
	else
	{
		doubleArray.append(-1);
	}

	setResult(doubleArray);

	MImage image;

	return MS::kSuccess;
}