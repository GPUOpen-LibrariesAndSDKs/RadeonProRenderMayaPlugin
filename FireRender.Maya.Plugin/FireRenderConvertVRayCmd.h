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

