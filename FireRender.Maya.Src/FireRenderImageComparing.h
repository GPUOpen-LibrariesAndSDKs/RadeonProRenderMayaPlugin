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