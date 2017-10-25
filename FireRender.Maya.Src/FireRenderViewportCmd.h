#pragma once
// Copyright (C) AMD
//
// File: FireRenderViewportCmd.h
//
// Maya cmd used to interact with the fire render viewport
//
// Created by Alan Stanzione.
//

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
