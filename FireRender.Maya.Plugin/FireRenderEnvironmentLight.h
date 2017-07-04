#pragma once

#include "ShadersManager.h"
#include <maya/MTypeId.h>
#include <maya/MGlobal.h>
#include <maya/MPxLocatorNode.h>

class FireRenderEnvironmentLight :
	public MPxLocatorNode
{
public:
	FireRenderEnvironmentLight();
	virtual ~FireRenderEnvironmentLight();

public:
	static  void * creator();
	static  MStatus initialize();

public:
	static MObject	aColor;
	static MObject	aIntensity;
	static MObject	aDisplay;
	static MObject	aPortal;
public:
	static  MTypeId id;
	static  MString drawDbClassification;
	static  MString drawRegistrantId;

};

