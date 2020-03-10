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

#define kPanelFlag "-p"
#define kPanelFlagLong "-panel"

#define kClearFlag "-cl"
#define kClearFlagLong "-clear"

#define kActiveCacheFlag "-c"
#define kActiveCacheFlagLong "-cache"

#define kViewportModeFlag "-vm"
#define kViewportModeFlagLong "-viewportMode"

#define kViewportAOVFlag "-va"
#define kViewportAOVFlagLong "-viewportAOV"

#define kRefreshFlag "-rf"
#define kRefreshFlagLong "-refresh"

class FireRenderViewportCmd : public MPxCommand
{
public:

	FireRenderViewportCmd();

	virtual ~FireRenderViewportCmd();

	MStatus doIt(const MArgList& args);

	static void* creator();

	static MSyntax  newSyntax();

	static int convertViewPortMode(MString value);

};
