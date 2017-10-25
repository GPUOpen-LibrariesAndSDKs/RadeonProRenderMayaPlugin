#include <functional>
#include <algorithm>

#include "maya/MGlobal.h"
#include "maya/MDagPath.h"
#include "maya/MFnDependencyNode.h"
#include "maya/MSelectionList.h"
#include "maya/MItDependencyNodes.h"
#include "maya/MDagModifier.h"
#include "maya/MFnMesh.h"
#include "maya/MFnMeshData.h"
#include "maya/MFnTransform.h"
#include "maya/MPointArray.h"
#include "maya/MFloatArray.h"
#include "maya/MIntArray.h"
#include "maya/MItDependencyGraph.h"
#include "maya/MFnAmbientLight.h"

#include "FireRenderConvertVRayCmd.h"

#include "FireRenderMaterial.h"
#include "FireRenderObjects.h"
#include "FireRenderContext.h"
#include "FireRenderUtils.h"
#include "FireRenderMath.h"
#include "VRay.h"
#include "Translators.h"

using namespace std;
using namespace FireMaya;
using namespace FireMaya::VRay;

vector<MObject> findShadingEngine(MObject &mesh)
{
	vector<MObject> ret;
	MStatus result;
	MItDependencyGraph itdep(mesh, MFn::kShadingEngine, MItDependencyGraph::kDownstream, MItDependencyGraph::kDepthFirst, MItDependencyGraph::kNodeLevel, &result);
	for (; !itdep.isDone(); itdep.next())
		ret.push_back(itdep.currentItem());

	return ret;
}

vector<MObject> findAllMaterialsAndShadingGroups(MObject &mesh, MStringArray & shadingEngineNames)
{
	vector<MObject> ret;

	auto shadingEngines = findShadingEngine(mesh);
	for (auto shadingEngine : shadingEngines)
	{
		MStatus result;
		MFnDependencyNode fnDepShadingEngine(shadingEngine);
		shadingEngineNames.append(fnDepShadingEngine.name());

		MPlug plgsurface = fnDepShadingEngine.findPlug("surfaceShader");

		MPlugArray shaderConnections;
		plgsurface.connectedTo(shaderConnections, true, false);

		for (MPlug plug : shaderConnections)
			ret.push_back(plug.node());
	}

	return ret;
}

vector<MObject> findVRayMaterials(MObject &mesh)
{
	MStringArray shadingGroups;
	vector<MObject> allShaders = findAllMaterialsAndShadingGroups(mesh, shadingGroups);
	vector<MObject> ret;

	for (auto ocur : allShaders)
	{
		if (ocur.hasFn(MFn::kPluginDependNode))
			ret.push_back(ocur);
	}

	return ret;
}

FireRenderConvertVRayCmd::FireRenderConvertVRayCmd()
{
}

FireRenderConvertVRayCmd::~FireRenderConvertVRayCmd()
{
}

void * FireRenderConvertVRayCmd::creator()
{
	return new FireRenderConvertVRayCmd();
}

MSyntax FireRenderConvertVRayCmd::newSyntax()
{
	return MSyntax();
}

MDagPathArray FireRenderConvertVRayCmd::GetSelectedObjectsDagPaths()
{
	MDagPathArray ret;

	MSelectionList sList;
	MGlobal::getActiveSelectionList(sList);

	for (unsigned int i = 0; i < sList.length(); i++)
	{
		MDagPath path;

		sList.getDagPath(i, path);
		path.extendToShape();

		DebugPrint("Selection: %s", path.fullPathName().asUTF8());

		if (path.isValid())
			ret.append(path);
	}

	return ret;
}

static bool isVRayObject(MDagPath dagPath)
{
	if (dagPath.isValid())
	{
		auto object = dagPath.node();

		if (object.hasFn(MFn::kPluginLocatorNode) || object.hasFn(MFn::kPluginTransformNode))
		{
			MFnDagNode dagNode(object);
			if (FireMaya::VRay::isVRayObject(dagNode))
				return true;
		}
	}

	return false;
}

MDagPathArray
FireRenderConvertVRayCmd::FilterVRayObjects(const MDagPathArray & arr)
{
	MDagPathArray ret;

	for (auto path : arr)
	{
		if (path.isValid())
		{
			if (isVRayObject(path))
			{
				ret.append(path);
			}
			else
			{
				auto obj = path.node();

				if (!obj.isNull())
				{
					auto materials = findVRayMaterials(obj);

					for (auto material : materials)
					{
						if (FireMaya::VRay::isTexture(material))
						{
							ret.append(path);
							break; // need only one to detect that object has VRay texture
						}
					}
				}
			}
		}
	}

	return ret;
}

MDagPathArray
FireRenderConvertVRayCmd::GetAllSceneVRayObjects()
{
	MDagPathArray ret;

	// 1st: get all the lights:
	{
		MItDependencyNodes it(MFn::kPluginLocatorNode);

		while (!it.isDone())
		{
			MObject node = it.item();
			if (!node.isNull() && isVRayObject(node))
			{
				MDagPathArray objectPaths;

				MDagPath::getAllPathsTo(node, objectPaths);

				for (auto p : objectPaths)
					ret.append(p);
			}

			it.next();
		}
	}

	// 2nd: Find meshes with VRay textures:
	{
		MItDependencyNodes it(MFn::kMesh);

		while (!it.isDone())
		{
			MObject node = it.item();
			if (!node.isNull())
			{
				auto materials = findVRayMaterials(node);

				for (auto material : materials)
				{
					if (FireMaya::VRay::isTexture(material))
					{
						MDagPathArray objectPaths;

						MDagPath::getAllPathsTo(node, objectPaths);

						for (auto p : objectPaths)
							ret.append(p);

						break;
					}
				}
			}

			it.next();
		}
	}


	return ret;
}

void FireRenderConvertVRayCmd::VRayLightIESShapeConverter(MObject originalVRayObject)
{
	MStatus status;

	auto data = FireMaya::VRay::readVRayIESLightData(originalVRayObject);

	MFnDagNode dagNodeOriginal(originalVRayObject);
	MString cmd =
		R"(createNode "transform" -n "t$name";
createNode RPRIES -n "$name" -p "t$name";
select -r "t$name";
setAttr -type "string" "$name.iesFile" "$filePath";
setAttr "$name.color" -type double3 $color.r $color.g $color.b;
setAttr "$name.display" $display;
setAttr "$name.intensity" $intensity;
setAttr "t$name.translateX" $translate.x;
setAttr "t$name.translateY" $translate.y;
setAttr "t$name.translateZ" $translate.z;
setAttr "t$name.rotateX" $rotate.x;
setAttr "t$name.rotateY" $rotate.y;
setAttr "t$name.rotateZ" $rotate.z;
setAttr "t$name.scaleX" $scale.x;
setAttr "t$name.scaleY" $scale.y;
setAttr "t$name.scaleZ" $scale.z;

select -r "$originalName";
doDelete;
)";

	auto name = "RPR"_ms + dagNodeOriginal.name();
	cmd.substitute("$name", name);
	cmd.substitute("$originalName", dagNodeOriginal.name());
	auto color = data.color * data.filterColor;
	cmd.substitute("$color.r", toMString(color.r));
	cmd.substitute("$color.g", toMString(color.g));
	cmd.substitute("$color.b", toMString(color.b));
	cmd.substitute("$filePath", data.filePath);
	cmd.substitute("$display", toMString(data.visible));
	cmd.substitute("$intensity", toMString(data.intensity));
	cmd.substitute("$translate.x", toMString(data.matrix(3, 0)));
	cmd.substitute("$translate.y", toMString(data.matrix(3, 1)));
	cmd.substitute("$translate.z", toMString(data.matrix(3, 2)));
	double rotation[3]{ 0 };
	MTransformationMatrix tm(data.matrix);
	MTransformationMatrix::RotationOrder ro;
	tm.getRotation(rotation, ro);
	cmd.substitute("$rotate.x", toMString(limit_degrees180pm(rad2deg(rotation[0]))));
	cmd.substitute("$rotate.x", toMString(rad2deg(rotation[0])));
	cmd.substitute("$rotate.y", toMString(rad2deg(rotation[1])));
	cmd.substitute("$rotate.z", toMString(rad2deg(rotation[2])));
	double scale[3]{ 0 };
	tm.getScale(scale, MSpace::kWorld);
	cmd.substitute("$scale.x", toMString(scale[0] * 2));
	cmd.substitute("$scale.y", toMString(scale[1] * 2));
	cmd.substitute("$scale.z", toMString(scale[2]));

	ExecuteCommand(cmd);
}

void FireRenderConvertVRayCmd::VRayLightSphereConverter(MObject originalVRayObject)
{
	auto data = FireMaya::VRay::readVRayLightSphereData(originalVRayObject);

	MFnDagNode dagNodeOriginal(originalVRayObject);
	MString cmd = "sphere -r "_ms + data.radius + " -n RPR"_ms + dagNodeOriginal.name();
	MStringArray arrNames = ExecuteCommandStringArrayResult(cmd);
	if (arrNames.length() < 1)
		throw std::runtime_error("Unexpected number of nodes selected after MEL sphere command!"s);
	auto name = arrNames[0];

	cmd = R"(shadingNode -asShader "RPRMaterial";)";
	auto materialName = ExecuteCommandStringResult(cmd);
	auto materialShaderGroupName = materialName + "SG";
	cmd =
		R"(sets -renderable true -noSurfaceShader true -empty -name("$materialShaderGroupName");
connectAttr -f("$materialName.outColor") ("$materialShaderGroupName.surfaceShader");
setAttr("$materialName.type") 6;
select -r "$name";
hyperShade -assign "$materialName";
setAttr "$materialName.color" -type double3 $color.r $color.g $color.b;
setAttr "$materialName.wattsPerSqm" $wattsPerSqm;
setAttr "$name.translateX" $translate.x;
setAttr "$name.translateY" $translate.y;
setAttr "$name.translateZ" $translate.z;
setAttr "$name.primaryVisibility" $display;
setAttr "$nameShape.castsShadows" 0;
setAttr "$nameShape.receiveShadows" 0;

select -r $originalName;
doDelete;
)";
	cmd.substitute("$name", name);
	cmd.substitute("$originalName", dagNodeOriginal.name());
	cmd.substitute("$materialName", materialName);
	cmd.substitute("$materialShaderGroupName", materialShaderGroupName);
	cmd.substitute("$color.r", toMString(data.color.r));
	cmd.substitute("$color.g", toMString(data.color.g));
	cmd.substitute("$color.b", toMString(data.color.b));
	cmd.substitute("$wattsPerSqm", toMString(translateVrayLightIntensity(data.intensity, data.units, 4 * PI_F * powf(data.radius, 2.f))));
	cmd.substitute("$translate.x", toMString(data.matrix(3, 0)));
	cmd.substitute("$translate.y", toMString(data.matrix(3, 1)));
	cmd.substitute("$translate.z", toMString(data.matrix(3, 2)));
	cmd.substitute("$display", toMString(!data.invisible));

	ExecuteCommand(cmd);
}

void FireRenderConvertVRayCmd::VRayLightRectShapeConverter(MObject originalVRayObject)
{
	auto data = FireMaya::VRay::readVRayLightRectData(originalVRayObject);

	MFnDagNode dagNodeOriginal(originalVRayObject);
	MString cmd = "polyPlane -w "_ms + data.uSize + " -h "_ms + data.vSize + " -sx 1 -sy 1 -ax 0 1 0 -cuv 2 -ch 1 -n \"RPR"_ms + dagNodeOriginal.name() + "\";"_ms;
	MStringArray arrNames = ExecuteCommandStringArrayResult(cmd);
	if (arrNames.length() < 1)
		throw std::runtime_error("Unexpected number of nodes selected after MEL polyPlane command!"s);
	auto name = arrNames[0];

	cmd = R"(shadingNode -asShader "RPRMaterial";)";
	auto materialName = MGlobal::executeCommandStringResult(cmd, false, true);
	auto materialShaderGroupName = materialName + "SG";
	cmd =
		R"(sets -renderable true -noSurfaceShader true -empty -name("$materialShaderGroupName");
connectAttr -f("$materialName.outColor") ("$materialShaderGroupName.surfaceShader");
setAttr("$materialName.type") 6;
select -r "$name";
hyperShade -assign "$materialName";
setAttr "$materialName.color" -type double3 $color.r $color.g $color.b;
setAttr "$materialName.wattsPerSqm" $wattsPerSqm;
setAttr "$name.translateX" $translate.x;
setAttr "$name.translateY" $translate.y;
setAttr "$name.translateZ" $translate.z;
setAttr "$name.rotateX" $rotate.x;
setAttr "$name.rotateY" $rotate.y;
setAttr "$name.rotateZ" $rotate.z;
setAttr "$name.scaleX" $scale.x;
setAttr "$name.scaleY" $scale.y;
setAttr "$name.scaleZ" $scale.z;
setAttr "$name.primaryVisibility" $display;
setAttr "$nameShape.castsShadows" 0;
setAttr "$nameShape.receiveShadows" 0;

select -r $originalName;
doDelete;
)";
	cmd.substitute("$name", name);
	cmd.substitute("$originalName", dagNodeOriginal.name());
	cmd.substitute("$materialName", materialName);
	cmd.substitute("$materialShaderGroupName", materialShaderGroupName);
	cmd.substitute("$color.r", toMString(data.color.r));
	cmd.substitute("$color.g", toMString(data.color.g));
	cmd.substitute("$color.b", toMString(data.color.b));
	cmd.substitute("$wattsPerSqm", toMString(translateVrayLightIntensity(data.intensity, data.units, 2 * data.uSize * data.vSize)));
	cmd.substitute("$translate.x", toMString(data.matrix(3, 0)));
	cmd.substitute("$translate.y", toMString(data.matrix(3, 1)));
	cmd.substitute("$translate.z", toMString(data.matrix(3, 2)));
	double rotation[3]{ 0 };
	MTransformationMatrix tm(data.matrix);
	MTransformationMatrix::RotationOrder ro;
	tm.getRotation(rotation, ro);
	cmd.substitute("$rotate.x", toMString(limit_degrees180pm(rad2deg(rotation[0]) - 90.0f)));
	cmd.substitute("$rotate.y", toMString(rad2deg(rotation[1])));
	cmd.substitute("$rotate.z", toMString(rad2deg(rotation[2])));
	double scale[3]{ 0 };
	tm.getScale(scale, MSpace::kWorld);
	cmd.substitute("$scale.x", toMString(scale[0] * 2));
	cmd.substitute("$scale.y", toMString(scale[2] * 2));
	cmd.substitute("$scale.z", toMString(scale[1] * 2));
	cmd.substitute("$display", toMString(!data.invisible));

	ExecuteCommand(cmd);
}

void FireRenderConvertVRayCmd::VRayLightDomeShapeConverter(MObject originalVRayObject)
{
	auto data = FireMaya::VRay::readVRayLightDomeShapeData(originalVRayObject);

	MFnDagNode dagNodeOriginal(originalVRayObject);

	if (data.useDomeTexture && data.texutre.length())
	{
		ExecuteCommand("createIBLNodeRPR");
		auto name = MGlobal::executeCommandStringResult("getIBLNodeRPR");

		MString cmd = R"(select -r "$name";
setAttr -type "string" "$name.filePath" "$texture";
setAttr "$name.intensity" $intensity;
setAttr "$name.display" $display;

select -r $originalName;
doDelete;
)";
		cmd.substitute("$name", name);
		cmd.substitute("$originalName", dagNodeOriginal.name());
		cmd.substitute("$texture", data.texutre);
		cmd.substitute("$intensity", toMString(data.intensity));
		cmd.substitute("$display", toMString(data.visible));

		ExecuteCommand(cmd);
	}
	else
	{
		ExecuteCommand("createEnvLightNodeRPR");
		auto name = MGlobal::executeCommandStringResult("getEnvLightNodeRPR");

		MString cmd = R"(select -r "$name";
setAttr "$name.color" -type double3 $color.r $color.g $color.b;
setAttr "$name.intensity" $intensity;
setAttr "$name.display" $display;

select -r $originalName;
doDelete;
)";
		cmd.substitute("$name", name);
		cmd.substitute("$originalName", dagNodeOriginal.name());
		cmd.substitute("$color.r", toMString(data.color.r));
		cmd.substitute("$color.g", toMString(data.color.g));
		cmd.substitute("$color.b", toMString(data.color.b));
		cmd.substitute("$intensity", toMString(data.intensity));
		cmd.substitute("$display", toMString(data.visible));

		ExecuteCommand(cmd);
	}
}

bool FireRenderConvertVRayCmd::VRayLightSunShapeConverter(MObject originalVRayObject)
{
	auto data = FireMaya::VRay::readVRayLightSunShapeData(originalVRayObject);

	if (!data)
		return false;

	if (data->manualPosition)
		data = FireMaya::VRay::translateManualVRaySunPoisiton(data);

	MString cmd = R"(directionalLight -rgb $filterColor.r $filterColor.g $filterColor.b -intensity $intensity -rotation $rotate.x $rotate.y $rotate.z -position $translate.x $translate.y $translate.z -name "$name";

select -r $geoSunName $sunShapeName $sunTargetName;
doDelete;
)";
	cmd.substitute("$name", "RPR"_ms + data->nameOfVRaySunShape);
	cmd.substitute("$geoSunName", data->nameOfVRayGeoSun);
	cmd.substitute("$sunShapeName", data->nameOfVRaySunShape);
	cmd.substitute("$sunTargetName", data->nameOfVRaySunTarget);
	cmd.substitute("$turbidity", toMString(data->turbidity));
	cmd.substitute("$intensity", toMString(data->intensity * 10));
	cmd.substitute("$filterColor.r", toMString(data->filterColor.r));
	cmd.substitute("$filterColor.g", toMString(data->filterColor.g));
	cmd.substitute("$filterColor.b", toMString(data->filterColor.b));
	cmd.substitute("$sizeMultiplier", toMString(data->sizeMultiplier));
	cmd.substitute("$rotate.x", toMString(limit_degrees180pm(data->altitudeDeg + 180)));
	cmd.substitute("$rotate.y", toMString(-data->azimuthDeg));
	cmd.substitute("$rotate.z", toMString(0));
	cmd.substitute("$translate.x", toMString(data->position.x));
	cmd.substitute("$translate.y", toMString(data->position.y));
	cmd.substitute("$translate.z", toMString(data->position.z));

	ExecuteCommand(cmd);

	return true;
}

bool FireRenderConvertVRayCmd::ConvertVRayObjectInDagPath(MDagPath originalVRayObjectDagPath)
{
	auto node = originalVRayObjectDagPath.node();

	if (node.isNull())
		return false;

	if (VRay::isVRayObject(node))
		return ConvertVRayObject(node);
	else
		return ConvertVRayShadersOn(originalVRayObjectDagPath);
}

bool FireRenderConvertVRayCmd::ConvertVRayObject(MObject originalVRayObject)
{
	MFnDagNode node(originalVRayObject);
	auto typeStr = node.typeName();

	if (typeStr == TypeStr::VRayLightSphereShape)
		VRayLightSphereConverter(originalVRayObject);
	else if (typeStr == TypeStr::VRayLightRectShape)
		VRayLightRectShapeConverter(originalVRayObject);
	else if (typeStr == TypeStr::VRayLightIESShape)
		VRayLightIESShapeConverter(originalVRayObject);
	else if (typeStr == TypeStr::VRayLightDomeShape)
		VRayLightDomeShapeConverter(originalVRayObject);
	else if (typeStr == TypeStr::VRayGeoSun || typeStr == TypeStr::VRaySunShape || typeStr == TypeStr::VRaySunTarget)
		return VRayLightSunShapeConverter(originalVRayObject);
	else
		throw std::logic_error(("Unsupported VRay light object: "_ms + typeStr).asUTF8());

	return true;

}

bool isOneOfMaterialTargets(MString name)
{
	const char* const findTargets[]
	{
		"outColor",
		"surfaceShader",
		"shadingGroup",
	};

	auto sName = name.asUTF8();

	for (auto target : findTargets)
	{
		if (strstr(sName, target))
			return true;
	}

	return false;
}

bool materialHasConnections(const MFnDependencyNode & shaderNode)
{
	MStatus status;
	MPlugArray connections;
	status = shaderNode.getConnections(connections);

	for (auto c : connections)
	{
		auto name = c.name(&status);

		if (strstr(name.asUTF8(), ".dagSetMembers["))
			return true;

		if (c.isSource())
		{
			if (isOneOfMaterialTargets(name))
			{
				MPlugArray destinations;
				c.connectedTo(destinations, true, false, &status);

				for (auto d : destinations)
				{
					MFnDependencyNode targetNode(d.node());
					if (materialHasConnections(targetNode))
						return true;
				}
			}
		}
	}

	return false;
}

bool FireRenderConvertVRayCmd::ConvertVRayShadersOn(MDagPath path)
{
	MStatus status;

	int converted = 0;
	bool ret = false;

	auto obj = path.node();

	if (obj.hasFn(MFn::kDagNode))
	{
		MFnDagNode fnDagNode(obj);

		MFnMesh fnMesh(obj, &status);

		MIntArray faceMaterialIndices;
		auto materialCount = GetFaceMaterials(fnMesh, faceMaterialIndices);

		MStringArray shadingGroups;
		auto oldMaterials = findAllMaterialsAndShadingGroups(obj, shadingGroups);

		if (oldMaterials.empty())
			return false;

		bool materialAssignedToAnObject = materialCount == 1;
		bool materialConverted = false;

		for (int materialIndex = 0; materialIndex < oldMaterials.size(); materialIndex++)
		{
			auto oldMaterial = oldMaterials[materialIndex];
			MFnDependencyNode shaderNode(oldMaterial);

			if (VRay::isTexture(shaderNode))
			{
				MObject newShader;

				MString newName;

				auto oldName = shaderNode.name();
				auto it = m_conversionObjectMap.find(oldName);
				if (it != m_conversionObjectMap.end())
				{
					newName = it->second;
				}
				else
				{
					newShader = ConvertVRayShader(shaderNode, oldName);

					if (!newShader.isNull())
					{
						newName = MFnDependencyNode(newShader).name();

						m_conversionObjectMap[oldName] = newName;
					}
				}

				if (newName.length())
				{
					if (materialAssignedToAnObject)
						AssignMaterialToAnObject(fnDagNode, newName);
					else
						AssignMaterialToFaces(fnDagNode, faceMaterialIndices, materialIndex, newName);

					m_convertedVRayMaterials.append(oldMaterial);
					materialConverted = true;
				}

				if (converted++)
					ret &= !newShader.isNull();
				else
					ret = !newShader.isNull();
			}
		}

		if (materialConverted)
		{
			for (auto name : shadingGroups)
				m_convertedVRayShadingGroups.append(name);
		}
	}

	return ret;
}

void FireRenderConvertVRayCmd::AssignMaterialToAnObject(MFnDagNode &fnDagNode, MString material)
{
	ExecuteCommand("select -r "_ms + fnDagNode.fullPathName());
	ExecuteCommand("hyperShade -assign "_ms + material);
}

void FireRenderConvertVRayCmd::AssignMaterialToFaces(MFnDagNode &fnDagNode, const MIntArray & faceMaterialIndices, int materialIndex, MString newMaterial)
{
	auto path = fnDagNode.partialPathName();

	vector<pair<int, int>> pairs;
	int start = -1, end = -1;
	for (int i = 0; i < static_cast<int>(faceMaterialIndices.length()); i++)
	{
		if (faceMaterialIndices[i] == materialIndex)
		{
			if (start == -1)
			{
				start = end = i;
			}
			else
			{
				if (end == (i - 1))
				{
					end = i;
				}
				else
				{
					pairs.push_back(make_pair(start, end));
					start = end = -1;
				}
			}
		}
	}
	if (start != -1)
		pairs.push_back(make_pair(start, end));

	for (auto pair : pairs)
	{
		ExecuteCommand("select -r "_ms + path + +".f[" + pair.first + ":" + pair.second + "]"_ms);
		ExecuteCommand("hyperShade -assign "_ms + newMaterial);
	}
}

MObject FireRenderConvertVRayCmd::ConvertVRayShader(const MFnDependencyNode & shaderNode, MString originalName)
{
	MStatus status;

	MTypeId typeId = shaderNode.typeId();
	auto typeIdUI = typeId.id();
	auto id = MayaSurfaceId(shaderNode.typeId().id());
	auto typeName = shaderNode.typeName();

	switch (id)
	{
	case VRay::VRayAlSurface:
		return ConvertVRayAISurfaceShader(shaderNode);
	case VRay::VRayMtl:
		return ConvertVRayMtlShader(shaderNode, originalName);
	case VRay::VRayCarPaintMtl:
		return ConvertVRayCarPaintShader(shaderNode);
	case VRay::VRayFastSSS2:
		return ConvertVRayFastSSS2MtlShader(shaderNode);
	case VRay::VRayLightMtl:
		return ConvertVRayLightMtlShader(shaderNode);
	case VRay::VRayMtlWrapper:
		return ConvertVRayMtlWrapperShader(shaderNode, originalName);
	case VRay::VRayBlendMtl:
		return ConvertVRayBlendMtlShader(shaderNode, originalName);
	case VRay::VRayBumpMtl:
	case VRay::VRayFlakesMtl:
	case VRay::VRayMeshMaterial:
	case VRay::VRayMtl2Sided:
	case VRay::VRayMtlHair3:
	case VRay::VRayMtlOSL:
	default:
		DebugPrint("VRay->RPR Shader: %s [0x%x]", typeName.asUTF8(), id);
		break;
	}

	return MObject();
}

MObject FireRenderConvertVRayCmd::tryFindAmbientLight()
{
	for (MItDependencyNodes it(MFn::kAmbientLight); !it.isDone(); it.next())
	{
		auto object = it.item();
		if (!object.isNull() && object.hasFn(MFn::kAmbientLight))
			return object;
	}

	return MObject();
}

MStatus FireRenderConvertVRayCmd::doIt(const MArgList & args)
{
	MStatus status;

	auto selectedObjects = GetSelectedObjectsDagPaths();

	auto numberOfSelectedObjects = selectedObjects.length();
	if (numberOfSelectedObjects == 0)
		selectedObjects = GetAllSceneVRayObjects();
	else
		selectedObjects = FilterVRayObjects(selectedObjects);

	int converted = 0, failed = 0;

	m_conversionObjectMap.clear();
	m_convertedVRayMaterials.clear();
	m_convertedVRayShadingGroups.clear();

	if (numberOfSelectedObjects == 0)
	{
		MFnDependencyNode vraySettings(findDependNode("vraySettings"));
		auto vrayGIOn = findPlugTryGetValue(vraySettings, "giOn", true);
		auto vrayOverrideEnvironment = findPlugTryGetValue(vraySettings, "cam_overrideEnvtex", false);
		MColor vrayGIColor(1, 1, 1);
		{
			auto plug = vraySettings.findPlug("cam_envtexGi");
			if (!plug.isNull())
			{
				vrayGIColor.r = plug.child(0).asFloat();
				vrayGIColor.g = plug.child(1).asFloat();
				vrayGIColor.b = plug.child(2).asFloat();
			}
		}

		if (vrayGIOn && vrayOverrideEnvironment)
		{
			auto ambientLight = tryFindAmbientLight();
			// check for ambient light
			if (ambientLight.isNull())
			{
				MFnAmbientLight light;
				ambientLight = light.create(true);
				light.setColor(vrayGIColor);
				light.setIntensity(1.f);
			}
		}
	}

	for (auto vrayObject : selectedObjects)
	{
		try
		{
			if (ConvertVRayObjectInDagPath(vrayObject))
				converted++;
		}
		catch (const std::exception & ex)
		{
			DebugPrint(ex.what());
			this->displayError("Failed to convert one of the objects because of: "_ms + ex.what());

			failed++;
		}
		catch (...)
		{
			this->displayError("Failed to convert one of the objects."_ms);

			failed++;
		}
	}

	auto vrayShadersDeleted = TryDeleteUnusedVRayMaterials();

	MString message;

	if (converted == 0)
		if (failed == 0)
			this->displayInfo(message = "No VRay objects have been found for conversion in the selection or scene.");
		else
			this->displayError(message = "VRay to Radeon Pro Render conversion has failed");
	else
		if (failed == 0)
			this->displayInfo(message = ""_ms + converted + " VRay object(s) have been successfully converted.");
		else
			this->displayWarning(message = ""_ms + converted + " VRay object(s) have been successfully converted, " + failed + " VRay object(s) have failed conversion.");

	this->setResult(message);

	m_conversionObjectMap.clear();

	return status;
}

MObjectArray unique(MObjectArray array)
{
	MObjectArray ret;

	for (unsigned int i = 0; i < array.length(); i++)
	{
		bool found = false;

		for (unsigned int j = 0; j < ret.length(); j++)
			if (array[i] == ret[j])
			{
				found = true;
				break;
			}

		if (!found)
			ret.append(array[i]);
	}

	return ret;
}

size_t FireRenderConvertVRayCmd::TryDeleteUnusedVRayMaterials()
{
	size_t deleted = 0;
	MStatus status;

	auto uniqueConvertedMaterials = unique(m_convertedVRayMaterials);

	std::vector<MString> namesToDelete;
	for (auto vrayMaterial : uniqueConvertedMaterials)
	{
		MFnDependencyNode vrayDepNode(vrayMaterial);

		if (!materialHasConnections(vrayDepNode))
			namesToDelete.push_back(vrayDepNode.name());
	}

	std::sort(namesToDelete.begin(), namesToDelete.end(), MStringComparison());

	for (auto name : namesToDelete)
	{
		try
		{
			ExecuteCommand("delete "_ms + name);
			deleted++;
		}
		catch (std::exception & ex)
		{
			DebugPrint(("Failed to delete VRay material: "s + name.asUTF8() + " because of: "s + ex.what()).c_str());
		}
	}

	return deleted;
}

void FireRenderConvertVRayCmd::ExecuteCommand(MString command)
{
	DebugPrint("ExecuteCommand: %s", command.asUTF8());

	MStatus status = MGlobal::executeCommand(command, false, true);

	if (status.error())
		throw runtime_error(status.errorString().asUTF8());
}

MString FireRenderConvertVRayCmd::ExecuteCommandStringResult(MString command)
{
	DebugPrint("ExecuteCommandStringResult: %s", command.asUTF8());

	MStatus status;
	MString ret = MGlobal::executeCommandStringResult(command, false, true, &status);

	if (status.error())
		throw logic_error(status.errorString().asUTF8());

	DebugPrint("ExecuteCommandStringResult -> %s", ret.asUTF8());

	return ret;
}

MStringArray FireRenderConvertVRayCmd::ExecuteCommandStringArrayResult(MString command)
{
	DebugPrint("ExecuteCommandStringArrayResult: %s", command.asUTF8());

	MStatus status;
	MStringArray ret;
	status = MGlobal::executeCommand(command, ret, false, true);

	if (status.error())
		throw runtime_error(status.errorString().asUTF8());

	DebugPrint("ExecuteCommandStringArrayResult -> Count: %d", ret.length());
	for (auto r : ret)
		DebugPrint("%s", r.asUTF8());

	return ret;
}

MObject FireRenderConvertVRayCmd::CreateShader(MString type, MString oldName)
{
	auto command = "shadingNode -asShader "_ms + type;
	if (oldName.length())
		command += " -name "_ms + oldName + "_RPR"_ms;

	auto shaderName = this->ExecuteCommandStringResult(command);

	MSelectionList sList;
	sList.add(shaderName);
	MObject shaderNode;
	sList.getDependNode(0, shaderNode);
	if (shaderNode.isNull())
	{
		MGlobal::displayError("Unable to create standard shader");

		throw logic_error(("Unable to create "_ms + type + " shader"_ms).asUTF8());
	}

	return shaderNode;
}

class MPlugValueHelper
{
	const MFnDependencyNode & m_source;
	MFnDependencyNode m_destination;

public:
	MPlugValueHelper(const MFnDependencyNode & source, MObject destination) :
		m_source(source),
		m_destination(destination)
	{
	}

public:
	MDataHandle CopyPlugValue(const MString sourcePlugName, const MString destinationPlugName)
	{
		MStatus status;
		bool connection = false;

		auto sourcePlug = m_source.findPlug(sourcePlugName);
		if (!sourcePlug.isNull())
		{
			MPlugArray connections;
			sourcePlug.connectedTo(connections, true, false);
			if (connections.length() > 0)
			{
				if (!connections[0].node().hasFn(MFn::kAnimCurve))
				{
					sourcePlug = connections[0];
					connection = true;
				}
			}
		}

		if (sourcePlug.isNull())
			throw logic_error(("Source plug can't be found: "_ms + sourcePlugName).asUTF8());


		if (!connection)
		{
			MDataHandle val;
			status = sourcePlug.getValue(val);
			if (status.error())
				throw logic_error(("Failed to read: "_ms + sourcePlugName + ": "_ms + status.errorString()).asUTF8());

			SetPlugValue(destinationPlugName, val);

			return val;
		}
		else
		{
			auto destPlug = m_destination.findPlug(destinationPlugName);
			if (destPlug.isNull())
				throw logic_error(("Destination plug can't be found: "_ms + destinationPlugName).asUTF8());

			MDagModifier modifier;
			modifier.connect(sourcePlug, destPlug);
			status = modifier.doIt();
			if (status.error())
				throw logic_error(("Failed to connect: "_ms + destinationPlugName + ": "_ms + status.errorString()).asUTF8());

			return MDataHandle();
		}
	}

	MObject GetPlugDestination(const MString plugName)
	{
		MStatus status;
		bool connection = false;

		MPlug sourcePlug = m_source.findPlug(plugName);
		if (!sourcePlug.isNull())
		{
			MPlugArray connections;
			sourcePlug.connectedTo(connections, true, false);
			if (connections.length() > 0)
			{
				if (!connections[0].node().hasFn(MFn::kAnimCurve))
				{
					sourcePlug = connections[0];
					connection = true;
				}
			}
		}

		if (sourcePlug.isNull())
			throw logic_error(("Source plug can't be found: "_ms + plugName).asUTF8());

		if (!connection)
			return MObject();

		auto ret = sourcePlug.node(&status);

		if (status.error())
			throw logic_error(status.errorString().asUTF8());

		return ret;
	}

	MDataHandle GetPlugValue(const MString sourcePlugName)
	{
		auto sourcePlug = m_source.findPlug(sourcePlugName);
		if (sourcePlug.isNull())
			throw logic_error(("Source plug can't be found: "_ms + sourcePlugName).asUTF8());

		MDataHandle data;
		MStatus status = sourcePlug.getValue(data);
		if (status.error())
			throw logic_error(("Failed to read: "_ms + sourcePlugName + ": "_ms + status.errorString()).asUTF8());

		return data;
	}

	void SetPlugValue(const MString destinationPlugName, std::function<MStatus(MPlug &)> setter)
	{
		auto destPlug = m_destination.findPlug(destinationPlugName);
		if (destPlug.isNull())
			throw logic_error(("Destination plug can't be found: "_ms + destinationPlugName).asUTF8());

		MStatus status = setter(destPlug);
		if (status.error())
			throw logic_error(("Failed to write: "_ms + destinationPlugName + ": "_ms + status.errorString()).asUTF8());
	}

	void SetPlugValue(const MString destinationPlugName, MDataHandle data)
	{
		SetPlugValue(destinationPlugName, [&data](MPlug& plug) { return plug.setMDataHandle(data); });
	}

	void SetPlugValue(const MString destinationPlugName, bool value)
	{
		SetPlugValue(destinationPlugName, [value](MPlug& plug) { return plug.setBool(value); });
	}

	void SetPlugValue(const MString destinationPlugName, float value)
	{
		SetPlugValue(destinationPlugName, [value](MPlug& plug) { return plug.setFloat(value); });
	}

	void SetPlugValue(const MString destinationPlugName, int value)
	{
		SetPlugValue(destinationPlugName, [value](MPlug& plug) { return plug.setInt(value); });
	}
};

void DebugDump(const MFnDependencyNode & node)
{
	for (auto i = node.attributeCount(); i; i--)
	{
		auto attr = node.attribute(i - 1);

		auto type = attr.apiTypeStr();

		auto plug = node.findPlug(attr);

		auto name = plug.name();
		auto info = plug.info();
		auto pname = plug.partialName(false, false, false, true, false, false);
		DebugPrint("%s(%s) [%s]: %s", name.asUTF8(), pname.asUTF8(), type, info.asUTF8());
	}
}

MObject FireRenderConvertVRayCmd::ConvertVRayMtlShader(const MFnDependencyNode & originalVRayShader, MString oldName)
{
	MStatus status;

	auto shaderNode = CreateShader("RPRUberMaterial", oldName);
	MFnDependencyNode convertedShader(shaderNode);
	if (!shaderNode.isNull())
	{
		MPlugValueHelper helper(originalVRayShader, shaderNode);

		helper.CopyPlugValue("dc", "diffuseColor");

		helper.SetPlugValue("reflections", true);
		helper.CopyPlugValue("rlc", "reflectColor");
		helper.CopyPlugValue("rlca", "reflectIOR");
		helper.CopyPlugValue("ra", "reflectRoughnessX");

		helper.SetPlugValue("clearCoat", true);
		helper.CopyPlugValue("rlc", "coatColor");
		helper.CopyPlugValue("fior", "coatIOR");
		helper.CopyPlugValue("rrc", "refractColor");

		auto rrcr = helper.GetPlugValue("rrcr").asFloat();
		helper.SetPlugValue("refraction", rrcr >= std::numeric_limits<float>::epsilon());
		helper.CopyPlugValue("rrcr", "refractWeight");
		auto rrg = helper.GetPlugValue("rrg").asFloat();
		helper.SetPlugValue("refractRoughness", 1.f - rrg);
		helper.CopyPlugValue("rior", "refractIOR");

		helper.CopyPlugValue("an", "reflectAnisotropy");
		helper.CopyPlugValue("anr", "reflectAnisotropyRotation");

		auto ssson = helper.GetPlugValue("ssson").asBool();
		helper.SetPlugValue("sssEnable", ssson);
		helper.CopyPlugValue("tlc", "sssColor");
		helper.CopyPlugValue("scdir", "scatteringDirection");
		helper.CopyPlugValue("sclvl", "sssWeight");

		auto opacity = helper.GetPlugValue("omr").asFloat();
		helper.SetPlugValue("transparencyLevel", 1.0f - opacity);	// opacity map to transparency color?
	}

	return shaderNode;
}

MObject FireRenderConvertVRayCmd::ConvertVRayAISurfaceShader(const MFnDependencyNode & originalVRayShader)
{
	MStatus status;

	auto shaderNode = CreateShader("RPRUberMaterial");
	MFnDependencyNode convertedShader(shaderNode);
	if (!shaderNode.isNull())
	{
		MPlugValueHelper helper(originalVRayShader, shaderNode);

		helper.CopyPlugValue("diffuse", "diffuseColor");

		helper.CopyPlugValue("diffuseBumpMap", "diffuseNormal");

		helper.SetPlugValue("reflections", true);
		helper.CopyPlugValue("reflect1", "reflectColor");
		helper.CopyPlugValue("reflect1IOR", "reflectIOR");
		helper.CopyPlugValue("reflect1Roughness", "reflectRoughnessX");
		helper.CopyPlugValue("reflect1BumpMap", "reflectNormal");

		auto opacity = helper.GetPlugValue("opacity").asFloat();
		helper.SetPlugValue("transparencyLevel", 1.0f - opacity);	// opacity map to transparency color?
	}


	return shaderNode;
}

MObject FireRenderConvertVRayCmd::ConvertVRayCarPaintShader(const MFnDependencyNode & originalVRayShader)
{
	MStatus status;

	auto shaderNode = CreateShader("RPRUberMaterial");
	MFnDependencyNode convertedShader(shaderNode);
	if (!shaderNode.isNull())
	{
		MPlugValueHelper helper(originalVRayShader, shaderNode);

		helper.CopyPlugValue("bcol", "diffuseColor");

		helper.CopyPlugValue("bbm", "diffuseNormal");

		helper.SetPlugValue("reflections", true);
		helper.CopyPlugValue("bcol", "reflectColor");

		auto reflection = helper.GetPlugValue("brfl").asFloat();
		auto ior = (2.0f / (sqrt(reflection) + 1.0f)) - 1.0f;
		helper.SetPlugValue("reflectIOR", ior);

		helper.SetPlugValue("clearCoat", true);
		helper.CopyPlugValue("ctcol", "coatColor");
	}


	return shaderNode;
}

MObject FireRenderConvertVRayCmd::ConvertVRayFastSSS2MtlShader(const MFnDependencyNode & originalVRayShader)
{
	MStatus status;

	auto shaderNode = CreateShader("RPRSubsurfaceMaterial");
	MFnDependencyNode convertedShader(shaderNode);
	if (!shaderNode.isNull())
	{
		MPlugValueHelper helper(originalVRayShader, shaderNode);

		helper.CopyPlugValue("df", "surfaceColor");
		helper.CopyPlugValue("dfa", "surfaceIntensity");

		helper.CopyPlugValue("ssc", "subsurfaceColor");

		helper.CopyPlugValue("scrc", "scatterColor");
		helper.CopyPlugValue("scrm", "scatterAmount");

		helper.CopyPlugValue("phf", "scatteringDirection");
	}

	return shaderNode;
}

MObject FireRenderConvertVRayCmd::ConvertVRayLightMtlShader(const MFnDependencyNode & originalVRayShader)
{
	MStatus status;

	auto shaderNode = CreateShader("RPRMaterial");
	MFnDependencyNode convertedShader(shaderNode);
	if (!shaderNode.isNull())
	{
		MPlugValueHelper helper(originalVRayShader, shaderNode);
		helper.SetPlugValue("type", (int)FireMaya::Material::Type::kEmissive);

		helper.CopyPlugValue("cl", "color");
	}

	return shaderNode;
}

MObject FireRenderConvertVRayCmd::ConvertVRayMtlWrapperShader(const MFnDependencyNode & originalVRayShader, MString oldName)
{
	MStatus status;

	MObject shaderNode;

	MPlugValueHelper helper(originalVRayShader, shaderNode);

	auto destination = helper.GetPlugDestination("bm");

	MFnDependencyNode childShader(destination);

	shaderNode = ConvertVRayShader(childShader, oldName);
	// It does not look like we can do anything about converting other attributes
	return shaderNode;
}

MObject FireRenderConvertVRayCmd::ConvertVRayBlendMtlShader(const MFnDependencyNode & originalVRayShader, MString oldName)
{
	MStatus status;

	DebugDump(originalVRayShader);

	auto shaderNode = CreateShader("RPRBlendMaterial");

	// It does not look like we can do anything about converting other attributes
	return shaderNode;
}
