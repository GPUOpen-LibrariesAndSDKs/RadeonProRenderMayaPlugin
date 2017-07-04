#include <maya/MPxCommand.h>
#include <maya/MSyntax.h>
#include <maya/MArgDatabase.h>
#include <maya/MObject.h>

#define kImageGPU "-iGU"
#define kImageBaseLineGPU "-bG"
#define kImageCPU "-iC"
#define kImageBaseLineCPU "-bC"
#define kImageMixed "-iM"
#define kImageBaseLineMixed "-bM"
#define kRprPluginDetails "-pD"

#define kImageGPULong "-imageGPU"
#define kImageBaseLineGPULong "-baseGPU"
#define kImageCPULong "-imageCPU"
#define kImageBaseLineCPULong "-baseCPU"
#define kImageMixedLong "-imageMixed"
#define kImageBaseLineMixedLong "-baseMixed"
#define kRprPluginDetailsLong "-rprPluginDetails"

class FireRenderImageComparing : public MPxCommand
{
public:

	static void* creator();

	static MSyntax  newSyntax();

	MStatus doIt(const MArgList& args);

};