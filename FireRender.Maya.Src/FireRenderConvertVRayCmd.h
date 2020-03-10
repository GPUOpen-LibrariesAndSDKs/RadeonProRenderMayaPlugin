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

#include <vector>
#include <map>

#include <maya/MObject.h>
#include <maya/MFnDagNode.h>
#include <maya/MPxCommand.h>
#include <maya/MSyntax.h>
#include <maya/MArgDatabase.h>
#include <maya/MFnDependencyNode.h>

#include "FireRenderUtils.h"

class FireRenderConvertVRayCmd : public MPxCommand
{
public:
	FireRenderConvertVRayCmd();
	virtual ~FireRenderConvertVRayCmd();

private:
	std::map<MString, MString, MStringComparison> m_conversionObjectMap;
	MObjectArray	m_convertedVRayMaterials;
	MStringArray	m_convertedVRayShadingGroups;

public:
	MStatus doIt(const MArgList& args);

	static void* creator();

	static MSyntax newSyntax();

private:
	MDagPathArray GetSelectedObjectsDagPaths();
	MDagPathArray GetAllSceneVRayObjects();
	static MDagPathArray FilterVRayObjects(const MDagPathArray & paths);

private:
	bool ConvertVRayObjectInDagPath(MDagPath originalVRayObjectDagPath);
	bool ConvertVRayObject(MObject object);
	bool ConvertVRayShadersOn(MDagPath path);

	void AssignMaterialToAnObject(MFnDagNode &fnDagNode, MString newName);
	void AssignMaterialToFaces(MFnDagNode &fnDagNode, const MIntArray & faceMaterialIndices, int materialIndex, MString newMaterial);

	MObject ConvertVRayShader(const MFnDependencyNode & shaderNode, MString originalName);

	void VRayLightSphereConverter(MObject originalVRayObject);
	void VRayLightRectShapeConverter(MObject originalVRayObject);
	void VRayLightIESShapeConverter(MObject originalVRayObject);
	void VRayLightDomeShapeConverter(MObject originalVRayObject);
	bool VRayLightSunShapeConverter(MObject originalVRayObject);

	MObject ConvertVRayMtlShader(const MFnDependencyNode & originalVRayObject, MString oldName);
	MObject ConvertVRayAISurfaceShader(const MFnDependencyNode & originalVRayObject);
	MObject ConvertVRayCarPaintShader(const MFnDependencyNode & originalVRayShader);
	MObject ConvertVRayFastSSS2MtlShader(const MFnDependencyNode & originalVRayShader);
	MObject ConvertVRayLightMtlShader(const MFnDependencyNode & originalVRayShader);
	MObject ConvertVRayMtlWrapperShader(const MFnDependencyNode & originalVRayObject, MString oldName);
	MObject ConvertVRayBlendMtlShader(const MFnDependencyNode & originalVRayObject, MString oldName);

	void ExecuteCommand(MString command);
	MString ExecuteCommandStringResult(MString command);
	MStringArray ExecuteCommandStringArrayResult(MString command);

	MObject CreateShader(MString type, MString oldName = MString());
	MObject tryFindAmbientLight();
	size_t TryDeleteUnusedVRayMaterials();
};

