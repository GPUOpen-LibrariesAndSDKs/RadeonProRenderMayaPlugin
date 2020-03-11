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

#include <maya/MPxCommand.h>
#include <maya/MSyntax.h>
#include <maya/MArgDatabase.h>

#define kMaterialFlag "-m"
#define kMaterialFlagLong "-material"
#define kSelectionFlag "-s"
#define kSelectionFlagLong "-selection"
#define kAllFlag "-sc"
#define kAllFlagLong "-scene"
#define kFilePathFlag "-f"
#define kFilePathFlagLong "-file"
#define kFramesFlag "-fr"
#define kFramesFlagLong "-frames"
#define kCompressionFlag "-cs"
#define kCompressionFlagLong "-compress"
#define kPadding "-p"
#define kPaddingLong "-padding"

class FireRenderExportCmd : public MPxCommand
{
public:

	FireRenderExportCmd();

	virtual ~FireRenderExportCmd();

	MStatus doIt(const MArgList& args);

	static void* creator();

	static MSyntax  newSyntax();
};
