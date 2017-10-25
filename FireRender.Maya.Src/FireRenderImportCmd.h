#pragma once
//
// Copyright (C) AMD
//
// File: FireRenderImportCmd.h
//
// FireRender import command
//
// Created by Alan Stanzione.
//

#include <maya/MPxCommand.h>
#include <maya/MSyntax.h>
#include <maya/MArgDatabase.h>

#define kMaterialFlag "-m"
#define kMaterialFlagLong "-material"
#define kFilePathFlag "-f"
#define kFilePathFlagLong "-file"

class FireRenderImportCmd : public MPxCommand
{
public:

	FireRenderImportCmd();

	virtual ~FireRenderImportCmd();

	MStatus doIt(const MArgList& args);

	static void* creator();

	static MSyntax  newSyntax();
};
