#pragma once
//
// Copyright (C) AMD
//
// File: FireRenderExportCmd.h
//
// FireRender export command
//
// Command used to export the scene, objects or materials
//
// Created by Alan Stanzione.
//

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

class FireRenderExportCmd : public MPxCommand
{
public:

	FireRenderExportCmd();

	virtual ~FireRenderExportCmd();

	MStatus doIt(const MArgList& args);

	static void* creator();

	static MSyntax  newSyntax();
};
