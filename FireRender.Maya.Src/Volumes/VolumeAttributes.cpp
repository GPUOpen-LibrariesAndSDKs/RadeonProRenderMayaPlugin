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
#include "VolumeAttributes.h"
#include "FireRenderUtils.h"
#include <maya/MPxNode.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnStringData.h>
#include <maya/MDGModifier.h>
#include <maya/MFnStringArrayData.h>
#include <maya/MArrayDataBuilder.h>
#include <maya/MAnimControl.h>
#include <maya/MTime.h>
#include <maya/MSelectionList.h>

#pragma warning(push)
#pragma warning(disable : 4244)
#pragma warning(disable : 4800)
#include <RadeonProRenderLibs/rprLibs/pluginUtils.hpp>
#pragma warning(pop) 

#include <array>
#include <fstream>
#include <regex>


// general
MObject RPRVolumeAttributes::vdbFile;
MObject RPRVolumeAttributes::namingSchema;
MObject RPRVolumeAttributes::loadedGrids;

// channels
// - albedo
MObject RPRVolumeAttributes::albedoEnabled;
MObject RPRVolumeAttributes::volumeDimensionsAlbedo;
MObject RPRVolumeAttributes::albedoSelectedGrid;
MObject RPRVolumeAttributes::albedoGradType;
MObject RPRVolumeAttributes::albedoRamp;
// - emission
MObject RPRVolumeAttributes::emissionEnabled;
MObject RPRVolumeAttributes::volumeDimensionsEmission;
MObject RPRVolumeAttributes::emissionSelectedGrid;
MObject RPRVolumeAttributes::emissionGradType;
MObject RPRVolumeAttributes::emissionValue;
MObject RPRVolumeAttributes::emissionIntensity;
MObject RPRVolumeAttributes::emissionRamp;
MObject RPRVolumeAttributes::emissionKelvin;
MObject RPRVolumeAttributes::emissionInputType;
// - density
MObject RPRVolumeAttributes::densityEnabled;
MObject RPRVolumeAttributes::volumeDimensionsDensity;
MObject RPRVolumeAttributes::volumeVoxelSizeDensity;
MObject RPRVolumeAttributes::densitySelectedGrid;
MObject RPRVolumeAttributes::densityGradType;
MObject RPRVolumeAttributes::densityRamp;
MObject RPRVolumeAttributes::densityMultiplier;

namespace
{
	const float temperatureToColorMapMaxTemperature = 10000.0f;
	const float temperatureToColor[] =
	{
		0.000000f, 0.000000f, 0.000000f,
		0.043055f, 0.001184f, 0.000000f,
		0.172220f, 0.004734f, 0.000000f,
		0.387495f, 0.010652f, 0.000000f,
		0.688881f, 0.018937f, 0.000000f,
		1.076376f, 0.029590f, 0.000000f,
		1.549982f, 0.042609f, 0.000000f,
		2.109697f, 0.057996f, 0.000000f,
		2.755523f, 0.075749f, 0.000000f,
		3.487458f, 0.095870f, 0.000000f,
		4.305504f, 0.118358f, 0.000000f,
		4.093179f, 0.181527f, 0.000000f,
		3.825340f, 0.261242f, 0.000000f,
		3.555754f, 0.341438f, 0.000000f,
		3.317587f, 0.412200f, 0.000000f,
		3.128523f, 0.468227f, 0.000000f,
		2.980078f, 0.512269f, 0.000837f,
		2.851015f, 0.550663f, 0.000555f,
		2.734544f, 0.585126f, 0.002127f,
		2.625639f, 0.616902f, 0.008050f,
		2.520566f, 0.646888f, 0.020404f,
		2.417980f, 0.675474f, 0.039308f,
		2.319993f, 0.702280f, 0.062315f,
		2.228321f, 0.726976f, 0.087614f,
		2.144101f, 0.749375f, 0.113732f,
		2.068048f, 0.769382f, 0.139490f,
		1.999053f, 0.787293f, 0.165230f,
		1.935324f, 0.803566f, 0.191690f,
		1.876358f, 0.818386f, 0.218510f,
		1.821721f, 0.831915f, 0.245382f,
		1.771028f, 0.844294f, 0.272032f,
		1.723882f, 0.855615f, 0.298710f,
		1.679905f, 0.865969f, 0.325642f,
		1.638817f, 0.875464f, 0.352566f,
		1.600369f, 0.884200f, 0.379247f,
		1.564341f, 0.892263f, 0.405471f,
		1.530534f, 0.899689f, 0.431446f,
		1.498762f, 0.906515f, 0.457389f,
		1.468852f, 0.912809f, 0.483121f,
		1.440653f, 0.918631f, 0.508478f,
		1.414028f, 0.924039f, 0.533308f,
		1.388875f, 0.929047f, 0.557769f,
		1.365094f, 0.933666f, 0.582038f,
		1.342575f, 0.937941f, 0.605999f,
		1.321218f, 0.941913f, 0.629540f,
		1.300933f, 0.945620f, 0.652556f,
		1.281665f, 0.949065f, 0.675165f,
		1.263360f, 0.952251f, 0.697502f,
		1.245944f, 0.955208f, 0.719493f,
		1.229348f, 0.957964f, 0.741064f,
		1.213509f, 0.960544f, 0.762144f,
		1.198397f, 0.962949f, 0.782820f,
		1.183984f, 0.965177f, 0.803191f,
		1.170216f, 0.967249f, 0.823211f,
		1.157044f, 0.969183f, 0.842833f,
		1.144423f, 0.970999f, 0.862011f,
		1.132336f, 0.972695f, 0.880806f,
		1.120770f, 0.974267f, 0.899292f,
		1.109684f, 0.975730f, 0.917440f,
		1.099042f, 0.977098f, 0.935221f,
		1.088811f, 0.978385f, 0.952606f,
		1.078865f, 0.979599f, 0.969861f,
		1.069246f, 0.980733f, 0.986951f,
		1.060096f, 0.981779f, 1.003535f,
		1.051465f, 0.982740f, 1.019433f,
		1.043333f, 0.983624f, 1.034613f,
		1.035526f, 0.984448f, 1.049445f,
		1.027849f, 0.985226f, 1.064346f,
		1.020264f, 0.985968f, 1.079330f,
		1.012814f, 0.986676f, 1.094254f,
		1.005628f, 0.987343f, 1.108806f,
		0.998809f, 0.987955f, 1.122821f,
		0.992293f, 0.988513f, 1.136480f,
		0.986022f, 0.989028f, 1.149848f,
		0.979940f, 0.989509f, 1.162988f,
		0.973987f, 0.989969f, 1.175962f,
		0.968216f, 0.990400f, 1.188687f,
		0.962691f, 0.990791f, 1.201078f,
		0.957371f, 0.991150f, 1.213189f,
		0.952206f, 0.991485f, 1.225082f,
		0.947148f, 0.991803f, 1.236823f,
		0.942231f, 0.992101f, 1.248356f,
		0.937506f, 0.992370f, 1.259601f,
		0.932943f, 0.992616f, 1.270603f,
		0.928506f, 0.992844f, 1.281409f,
		0.924161f, 0.993059f, 1.292072f,
		0.919932f, 0.993259f, 1.302546f,
		0.915856f, 0.993438f, 1.312772f,
		0.911909f, 0.993600f, 1.322785f,
		0.908067f, 0.993749f, 1.332622f,
		0.904302f, 0.993889f, 1.342326f,
		0.900634f, 0.994017f, 1.351860f,
		0.897089f, 0.994130f, 1.361179f,
		0.893649f, 0.994231f, 1.370311f,
		0.890296f, 0.994321f, 1.379286f,
		0.887009f, 0.994405f, 1.388138f,
		0.883604f, 0.994481f, 1.397407f,
		0.880092f, 0.994549f, 1.407082f,
		0.876759f, 0.994606f, 1.416328f,
		0.873905f, 0.994653f, 1.424269f,
		0.871841f, 0.994688f, 1.429996f
	};
}

MStatus postConstructor_initialise_ramp_curve_dbg(
	MObject parentNode, 
	MObject rampObj, 
	int index, 
	float position, 
	float value, 
	int interpolation)
{
	MStatus status;

	MPlug rampPlug(parentNode, rampObj);

	MPlug elementPlug = rampPlug.elementByLogicalIndex(index, &status);

	MPlug positionPlug = elementPlug.child(0, &status);
	status = positionPlug.setFloat(position);

	MPlug valuePlug = elementPlug.child(1);
	status = valuePlug.setFloat(value);

	MPlug interpPlug = elementPlug.child(2);
	interpPlug.setInt(interpolation);

	return MS::kSuccess;
}

MStatus postConstructor_initialise_color_curve_dbg(
	MObject& parentNode, 
	MObject& rampObj, 
	int index, 
	float position, 
	MColor value, 
	int interpolation)
{
	MStatus status;

	MPlug rampPlug(parentNode, rampObj);

	MPlug elementPlug = rampPlug.elementByLogicalIndex(index, &status);

	MPlug positionPlug = elementPlug.child(0, &status);
	status = positionPlug.setFloat(position);

	MPlug valuePlug = elementPlug.child(1);
	status = valuePlug.child(0).setFloat(value.r);
	status = valuePlug.child(1).setFloat(value.g);
	status = valuePlug.child(2).setFloat(value.b);

	MPlug interpPlug = elementPlug.child(2);
	interpPlug.setInt(interpolation);

	return MS::kSuccess;
}

void RPRVolumeAttributes::Initialize()
{
	MStatus status;
	MFnNumericAttribute nAttr;
	MFnEnumAttribute eAttr;
	MRampAttribute rAttr;
	MFnTypedAttribute tAttr;

	// General
	MFnStringData fnStringData;
	MObject defaultStringData = fnStringData.create("");
	vdbFile = tAttr.create("VdbFilePath", "file", MFnData::kString, defaultStringData, &status);
	CHECK_MSTATUS(status);
	tAttr.setConnectable(false);
	tAttr.setUsedAsFilename(true);
	status = addAttribute(vdbFile);
	CHECK_MSTATUS(status);

	// - naming schema (for sequence)
	namingSchema = nAttr.create("namingSchema", "nmsh", MFnNumericData::kInt, 0);
	CHECK_MSTATUS(status);
	tAttr.setHidden(true);
	setAttribProps(tAttr, namingSchema);

	MFnStringData stringData; // use textScrollList for UI
	MStatus status2;
	loadedGrids = tAttr.create("loadedGrids", "loag", MFnData::kString, stringData.create(&status2), &status); //https://download.autodesk.com/us/maya/2011help/API/cgfx_shader_node_8cpp-example.html#_a24
	tAttr.setArray(true);
	tAttr.setHidden(true);
	tAttr.setUsesArrayDataBuilder(true);
	CHECK_MSTATUS(tAttr.setKeyable(true));
	CHECK_MSTATUS(tAttr.setStorable(false));
	CHECK_MSTATUS(tAttr.setReadable(true));
	CHECK_MSTATUS(tAttr.setWritable(true));
	CHECK_MSTATUS(MPxNode::addAttribute(loadedGrids));

	// Albedo
	albedoEnabled = nAttr.create("albedoEnabled", "ealb", MFnNumericData::kBoolean, 0);
	setAttribProps(nAttr, albedoEnabled);

	volumeDimensionsAlbedo = nAttr.create("volumeDimensionsAlbedo", "adms", MFnNumericData::k3Short);
	setAttribProps(nAttr, volumeDimensionsAlbedo);
	CHECK_MSTATUS(nAttr.setDefault(4, 4, 4));
	nAttr.setMin(1, 1, 1);
	nAttr.setMax(10000, 10000, 10000);

	albedoSelectedGrid = tAttr.create("albedoSelectedGrid", "alsg", MFnData::kString, MFnStringData().create("Not used"), &status);
	tAttr.setHidden(true);
	setAttribProps(tAttr, albedoSelectedGrid);

	albedoRamp = rAttr.createColorRamp("albedoRamp", "valb", &status);
	CHECK_MSTATUS(status);
	status = MPxNode::addAttribute(albedoRamp);
	CHECK_MSTATUS(status);

	// Emission
	emissionEnabled = nAttr.create("emissionEnabled", "eems", MFnNumericData::kBoolean, 0);
	setAttribProps(nAttr, emissionEnabled);

	volumeDimensionsEmission = nAttr.create("volumeDimensionsEmission", "edms", MFnNumericData::k3Short);
	setAttribProps(nAttr, volumeDimensionsEmission);
	CHECK_MSTATUS(nAttr.setDefault(4, 4, 4));
	nAttr.setMin(1, 1, 1);
	nAttr.setMax(10000, 10000, 10000);

	emissionSelectedGrid = tAttr.create("emissionSelectedGrid", "elsg", MFnData::kString, MFnStringData().create("Not used"), &status);
	tAttr.setHidden(true);
	setAttribProps(tAttr, emissionSelectedGrid);

	emissionRamp = rAttr.createColorRamp("emissionRamp", "vems", &status);
	CHECK_MSTATUS(status);
	status = MPxNode::addAttribute(emissionRamp);
	CHECK_MSTATUS(status);

	emissionIntensity = nAttr.create("emissionIntensity", "iems", MFnNumericData::kFloat, 1.0f);
	setAttribProps(nAttr, emissionIntensity);
	nAttr.setMin(0.0f);
	nAttr.setSoftMax(1000.0f);

	emissionKelvin = nAttr.create("emissionKelvin", "eklv", MFnNumericData::kFloat, 5000.0f);
	setAttribProps(nAttr, emissionKelvin);
	nAttr.setMin(0.0f);
	nAttr.setSoftMax(30000.0f);

	emissionInputType = eAttr.create("emissionInputType", "emst", 0);
	eAttr.addField("By Value", kByValue);
	eAttr.addField("By Kelvin", kByKelvin);
	setAttribProps(eAttr, emissionInputType);

	// Density
	densityEnabled = nAttr.create("densityEnabled", "edns", MFnNumericData::kBoolean, 1);
	setAttribProps(nAttr, densityEnabled);

	volumeDimensionsDensity = nAttr.create("volumeDimensionsDensity", "ddms", MFnNumericData::k3Short);
	setAttribProps(nAttr, volumeDimensionsDensity);
	CHECK_MSTATUS(nAttr.setDefault(4, 4, 4));
	nAttr.setMin(1, 1, 1);
	nAttr.setMax(10000, 10000, 10000);

	volumeVoxelSizeDensity = nAttr.create("volumeVoxelSizeDensity", "dvxs", MFnNumericData::k3Double);
	setAttribProps(nAttr, volumeVoxelSizeDensity);
	CHECK_MSTATUS(nAttr.setDefault(0.25, 0.25, 0.25));

	densitySelectedGrid = tAttr.create("densitySelectedGrid", "dnsg", MFnData::kString, MFnStringData().create("Not used"), &status);
	tAttr.setHidden(true);
	
	setAttribProps(tAttr, densitySelectedGrid);

	densityMultiplier = nAttr.create("densityMultiplier", "kdns", MFnNumericData::kFloat, 800.0f);
	setAttribProps(nAttr, densityMultiplier);
	nAttr.setMin(0.0f);
	nAttr.setSoftMax(10000.0f);

	densityRamp = rAttr.createCurveRamp("densityRamp", "vdns", &status);
	CHECK_MSTATUS(status);
	status = MPxNode::addAttribute(densityRamp);
	CHECK_MSTATUS(status);
}

MDataHandle RPRVolumeAttributes::GetVolumeGridDimentions(const MFnDependencyNode& node)
{
	MPlug plug = node.findPlug(RPRVolumeAttributes::volumeDimensionsDensity);

	assert(!plug.isNull());
	if (!plug.isNull())
	{
		return plug.asMDataHandle();
	}

	return MDataHandle();
}

MDataHandle RPRVolumeAttributes::GetVolumeVoxelSize(const MFnDependencyNode& node)
{
	MPlug plug = node.findPlug(RPRVolumeAttributes::volumeVoxelSizeDensity);

	assert(!plug.isNull());
	if (!plug.isNull())
	{
		return plug.asMDataHandle();
	}

	return MDataHandle();
}

bool ProcessSchema(int schemaId, int frame, std::string& filePath)
{
	const std::string& fileExtension ("vdb");

#ifdef WIN32
	const static std::map<int, std::tuple<std::regex, std::regex, std::string>> mayaNamePattern =
	{
		{ 0, {std::regex(R"(^(?:[\w]\:)(\/[a-zA-Z_\-\s0-9\.\)\(]+)+(\.)([0-9])+\.(vdb)$)"), std::regex(R"((\.)([0-9])+\.(vdb)$)"),	"name.#.ext"	}},
		{ 1, {std::regex(R"(^(?:[\w]\:)(\/[a-zA-Z_\-\s0-9\.\)\(]+)+(\.)(vdb)\.([0-9])+$)"), std::regex(R"((\.)(vdb)\.([0-9])+$)"),	"name.ext.#"	}},
		{ 2, {std::regex(R"(^(?:[\w]\:)(\/[a-zA-Z_\-\s0-9\.\)\(]+)+\.([0-9])+$)"),			std::regex(R"(\.([0-9])+$)"),			"name.#"		}},
		{ 3, {std::regex(R"(^(?:[\w]\:)(\/[a-zA-Z_\-\s0-9\.\)\(]+)+([0-9])+\.(vdb)$)"),		std::regex(R"(([0-9])+\.(vdb)$)"),		"name#.ext"		}},
		{ 4, {std::regex(R"(^(?:[\w]\:)(\/[a-zA-Z_\-\s0-9\.\)\(]+)+_([0-9])+\.(vdb)$)"), std::regex(R"(_([0-9])+\.(vdb)$)"),		"name_#.ext"	}}
	};
#else // macOS
	const static std::map<int, std::tuple<std::regex, std::regex, std::string>> mayaNamePattern =
	{
		{ 0, {std::regex(R"(^(\/[a-zA-Z_\-\s0-9\.]+)+(\.)([0-9])+\.(vdb)$)"),	std::regex(R"((\.)([0-9])+\.(vdb)$)"),	"name.#.ext"	}},
		{ 1, {std::regex(R"(^(\/[a-zA-Z_\-\s0-9\.]+)+(\.)(vdb)\.([0-9])+$)"),	std::regex(R"((\.)(vdb)\.([0-9])+$)"),	"name.ext.#"	}},
		{ 2, {std::regex(R"(^(\/[a-zA-Z_\-\s0-9\.]+)+\.([0-9])+$)"),			std::regex(R"(\.([0-9])+$)"),			"name.#"		}},
		{ 3, {std::regex(R"(^(\/[a-zA-Z_\-\s0-9\.]+)+([0-9])+\.(vdb)$)"),		std::regex(R"(([0-9])+\.(vdb)$)"),		"name#.ext"		}},
		{ 4, {std::regex(R"(^(\/[a-zA-Z_\-\s0-9\.]+)+_([0-9])+\.(vdb)$)"),		std::regex(R"(_([0-9])+\.(vdb)$)"),		"name_#.ext"	}}
	};
#endif

	// get regex record corresponding to ui option
	auto it = mayaNamePattern.find(schemaId);
	if (it == mayaNamePattern.end())
		return false;

	// ui option matches with input file name / path => proceed
	const std::regex& fullPathCheck = std::get<0>(it->second);
	bool matches = std::regex_match(filePath.begin(), filePath.end(), fullPathCheck);
	if (!matches)
		return false;

	// extract file name (without frame and extention)
	std::smatch match;
	const std::regex& getFilename = std::get<1>(it->second); //getFilename(R"(([a-zA-Z_]+)\_([0-9]+)\.(vdb)$)");
	bool found = std::regex_search(filePath, match, getFilename);
	assert(found);
	std::string tail = match[0];
	std::string filename = filePath.substr(0, filePath.find(tail));

	// form output string
	std::regex name_regex("name");
	std::regex frame_regex("#");
	std::regex extension_regex("ext");
	const std::string pattern = std::get<2>(it->second);

	filePath = std::regex_replace(pattern, extension_regex, fileExtension);
	filePath = std::regex_replace(filePath, frame_regex, std::to_string(frame));
	filePath = std::regex_replace(filePath, name_regex, filename);

	return true;
}

std::string RPRVolumeAttributes::GetVDBFilePath(const MFnDependencyNode& node)
{
	MStatus status;

	// get file name string
	MPlug vdbFilePlug = node.findPlug(RPRVolumeAttributes::vdbFile, &status);
	CHECK_MSTATUS(status);

	assert(!vdbFilePlug.isNull());
	if (vdbFilePlug.isNull())
	{
		return "";
	}

	std::string out;
	out = vdbFilePlug.asString().asChar();
	// - NIY => process env vars

	// check if file exists
	std::ifstream f(out);
	bool fileExists = f.good();
	if (!fileExists)
		return "";

	// get current animation frame
	MTime currentTime = MAnimControl::currentTime();
	double timeValue = currentTime.value();
	MTime::Unit timeUnit = currentTime.uiUnit();
	int currentAnimFrame = (int)timeValue;

	// check if filename is part of sequence
	MPlug vdbSchemaPlug = node.findPlug(RPRVolumeAttributes::namingSchema);
	assert(!vdbSchemaPlug.isNull());
	bool isSequence = ProcessSchema(vdbSchemaPlug.asInt(), currentAnimFrame, out);

	return out;
}

bool RPRVolumeAttributes::GetAlbedoEnabled(const MFnDependencyNode& node)
{
	MPlug plug = node.findPlug(RPRVolumeAttributes::albedoEnabled);

	assert(!plug.isNull());
	if (!plug.isNull())
	{
		return plug.asBool();
	}

	return false;
}

VolumeGradient RPRVolumeAttributes::GetAlbedoGradientType(const MFnDependencyNode& node)
{
	MPlug plug = node.findPlug(RPRVolumeAttributes::albedoGradType);

	assert(!plug.isNull());

	if (!plug.isNull())
	{
		return (VolumeGradient)plug.asInt();
	}

	return kConstant;
}

MPlug RPRVolumeAttributes::GetAlbedoRamp(const MFnDependencyNode& node)
{
	MPlug plug = node.findPlug(RPRVolumeAttributes::albedoRamp);

	assert(!plug.isNull());

	return plug;
}

bool RPRVolumeAttributes::GetEmissionEnabled(const MFnDependencyNode& node)
{
	MPlug plug = node.findPlug(RPRVolumeAttributes::emissionEnabled);

	assert(!plug.isNull());
	if (!plug.isNull())
	{
		return plug.asBool();
	}

	return false;
}

VolumeGradient RPRVolumeAttributes::GetEmissionGradientType(const MFnDependencyNode& node)
{
	MPlug plug = node.findPlug(RPRVolumeAttributes::emissionGradType);

	assert(!plug.isNull());

	if (!plug.isNull())
	{
		return (VolumeGradient)plug.asInt();
	}

	return kConstant;
}

MPlug RPRVolumeAttributes::GetEmissionValueRamp(const MFnDependencyNode& node)
{
	MPlug plug = node.findPlug(RPRVolumeAttributes::emissionRamp);

	assert(!plug.isNull());

	return plug;
}

float RPRVolumeAttributes::GetEmissionIntensity(const MFnDependencyNode& node)
{
	MPlug plug = node.findPlug(RPRVolumeAttributes::emissionIntensity);

	assert(!plug.isNull());
	if (!plug.isNull())
	{
		return plug.asFloat();
	}

	return 1.0f;
}

float RPRVolumeAttributes::GetEmissionKelvin(const MFnDependencyNode& node)
{
	MPlug plug = node.findPlug(RPRVolumeAttributes::emissionKelvin);

	assert(!plug.isNull());
	if (!plug.isNull())
	{
		return plug.asFloat();
	}

	return 1.0f;
}

RPRVolumeAttributes::EmissionInputType RPRVolumeAttributes::GetEmissionInputType(const MFnDependencyNode& node)
{
	MPlug plug = node.findPlug(RPRVolumeAttributes::emissionInputType);

	EmissionInputType inputType = kByValue;

	if (!plug.isNull())
	{
		int n = 0;
		if (MStatus::kSuccess == plug.getValue(n))
			inputType = static_cast<EmissionInputType>(n);
	}

	return inputType;
}

MPlug RPRVolumeAttributes::GetEmissionIntensityRamp(const MFnDependencyNode& node)
{
	MPlug plug = node.findPlug(RPRVolumeAttributes::emissionRamp);

	assert(!plug.isNull());

	return plug;
}

bool RPRVolumeAttributes::GetDensityEnabled(const MFnDependencyNode& node)
{
	MPlug plug = node.findPlug(RPRVolumeAttributes::densityEnabled);

	assert(!plug.isNull());
	if (!plug.isNull())
	{
		return plug.asBool();
	}

	return false;
}

VolumeGradient RPRVolumeAttributes::GetDensityGradientType(const MFnDependencyNode& node)
{
	MPlug plug = node.findPlug(RPRVolumeAttributes::densityGradType);

	assert(!plug.isNull());

	if (!plug.isNull())
	{
		return (VolumeGradient)plug.asInt();
	}

	return kConstant;
}

MPlug RPRVolumeAttributes::GetDensityRamp(const MFnDependencyNode& node)
{
	MPlug plug = node.findPlug(RPRVolumeAttributes::densityRamp);

	assert(!plug.isNull());

	return plug;
}

float RPRVolumeAttributes::GetDensityMultiplier(const MFnDependencyNode& node)
{
	MPlug plug = node.findPlug(RPRVolumeAttributes::densityMultiplier);

	assert(!plug.isNull());
	if (!plug.isNull())
	{
		return plug.asFloat();
	}

	return 1.0f;
}

MString RPRVolumeAttributes::GetSelectedAlbedoGridName(const MFnDependencyNode& node, std::string filePath, bool& failed)
{
	failed = false;
	MPlug plug = node.findPlug(RPRVolumeAttributes::albedoSelectedGrid);
	MStatus status;

	if (plug.isNull(&status))
		return MString();

	MString plugName = plug.name(&status);
	MDataHandle data = plug.asMDataHandle(&status);
	MString& value = data.asString();

	// ensure valid grid is selected
	VDBGridParams gridParams;
	auto res = ReadVolumeDataFromFile(filePath, gridParams);
	if (!std::get<bool>(res))
		failed = true;

	std::string stdStrValue = value.asChar();
	bool found = false;
	for (auto it = gridParams.begin(); it != gridParams.end(); ++it)
	{
		const std::string& gridName = it->first;
		if (stdStrValue == gridName)
		{
			found = true;
		}
	}

	if (!found)
	{
		failed = true;
	}
	return MString(value);
}

MString RPRVolumeAttributes::GetSelectedDensityGridName(const MFnDependencyNode& node, std::string filePath, bool& failed)
{
	failed = false;
	MPlug plug = node.findPlug(RPRVolumeAttributes::densitySelectedGrid);
	MStatus status;

	if (plug.isNull(&status))
		return MString();

	MString plugName = plug.name(&status);
	MDataHandle data = plug.asMDataHandle(&status);
	MString& value = data.asString();

	// ensure valid grid is selected
	VDBGridParams gridParams;
	auto res = ReadVolumeDataFromFile(filePath, gridParams);
	if (!std::get<bool>(res))
		failed = true;

	std::string stdStrValue = value.asChar();
	bool found = false;
	for (auto it = gridParams.begin(); it != gridParams.end(); ++it)
	{
		const std::string& gridName = it->first;
		if (stdStrValue == gridName)
		{
			found = true;
		}
	}

	if (!found)
	{
		failed = true;
	}
	return MString(value);
}

MString RPRVolumeAttributes::GetSelectedEmissionGridName(const MFnDependencyNode& node, std::string filePath, bool& failed)
{
	failed = false;
	MPlug plug = node.findPlug(RPRVolumeAttributes::emissionSelectedGrid); 
	MStatus status;

	if (plug.isNull(&status))
		return MString();

	MString plugName = plug.name(&status);
	MDataHandle data = plug.asMDataHandle(&status);
	MString& value = data.asString();

	// ensure valid grid is selected
	VDBGridParams gridParams;
	auto res = ReadVolumeDataFromFile(filePath, gridParams);
		if (!std::get<bool>(res))
			failed = true;
	
	std::string stdStrValue = value.asChar();
	bool found = false;
	for (auto it = gridParams.begin(); it != gridParams.end(); ++it)
	{
		const std::string& gridName = it->first;
		if (stdStrValue == gridName)
		{
			found = true;
		}
	}

	if (!found)
	{
		failed = true;
	}
	return MString(value);
}

void SetVolumeUIGridSize(VDBGridSize& dimValues, MPlug& plugToSet)
{
	assert(!plugToSet.isNull());
	if (plugToSet.isNull())
		return;

	MStatus status;
	bool isCompund = plugToSet.isCompound(&status);
	if (!isCompund)
		return;

	// set x dimension
	MPlug childPlug = plugToSet.child(0, &status);
	if (status != MStatus::kSuccess)
		return;
	status = childPlug.setShort(dimValues.gridSizeX);

	// set y dimension
	childPlug = plugToSet.child(1, &status);
	if (status != MStatus::kSuccess)
		return;
	status = childPlug.setShort(dimValues.gridSizeY);

	// set z dimension
	childPlug = plugToSet.child(2, &status);
	if (status != MStatus::kSuccess)
		return;
	status = childPlug.setShort(dimValues.gridSizeZ);
}

void SetVolumeUIVoxelSize(VDBGridSize& dimValues, MPlug& plugToSet)
{
	assert(!plugToSet.isNull());
	if (plugToSet.isNull())
		return;

	MStatus status;
	bool isCompund = plugToSet.isCompound(&status);
	if (!isCompund)
		return;

	// set x dimension
	MPlug childPlug = plugToSet.child(0, &status);
	if (status != MStatus::kSuccess)
		return;
	status = childPlug.setDouble(dimValues.voxelSizeX);

	// set y dimension
	childPlug = plugToSet.child(1, &status);
	if (status != MStatus::kSuccess)
		return;
	status = childPlug.setDouble(dimValues.voxelSizeY);

	// set z dimension
	childPlug = plugToSet.child(2, &status);
	if (status != MStatus::kSuccess)
		return;
	status = childPlug.setDouble(dimValues.voxelSizeZ);
}

// modify input grid to be used as density and calculate corresponing lookup table
void ProcessDensityGrid(
	std::vector<float>& floatGridOnValueIndices,
	std::vector<float>& valuesLookUpTable,
	float minVal,
	float maxVal,
	float densityMultiplier)
{
	float valueScale = (maxVal <= minVal) ? 1.0f : (1.0f / (maxVal - minVal));

	float offset = 0.0f;
	if (minVal*valueScale < 0.0f) // density less than zero is not a valid case for RPR but it is possible in VDB grid
	{
		offset = -minVal * valueScale;
	}

	for (float& gridValue : floatGridOnValueIndices)
	{
		gridValue = gridValue * valueScale + offset;
	}
}

// modify input grid to be used as albedo and calculate corresponing lookup table
void ProcessTemperatureGrid(
	std::vector<float>& floatGridOnValueIndices,
	float minVal,
	float maxVal,
	float temperatureColorMul = 1.0f)
{
	const float temperatureOffset = (minVal < 0) ? -minVal : 0.0f;

	for (float& gridValue : floatGridOnValueIndices)
	{
		gridValue += temperatureOffset;
	}
}

void GetMaxGridSize(const std::string& filename, const MFnDependencyNode& node, VDBGridParams& maxGridParams)
{
	maxGridParams.clear();

	// iterate over animation frames to find max bbox size
	MTime startTime = MAnimControl::minTime();
	unsigned int startFrame = static_cast<unsigned int>(startTime.value());
	MTime endTime = MAnimControl::maxTime();
	unsigned int endFrame = static_cast<unsigned int>(endTime.value());

	MPlug vdbSchemaPlug = node.findPlug(RPRVolumeAttributes::namingSchema);
	assert(!vdbSchemaPlug.isNull());
	VDBGridParams gridParams;
	for (unsigned int tmpFrame = startFrame; tmpFrame < endFrame; ++tmpFrame)
	{
		std::string tmpFilePath = filename;

		// try find anim file
		bool success = ProcessSchema(vdbSchemaPlug.asInt(), tmpFrame, tmpFilePath);
		if (!success)
			continue;

		std::ifstream f(tmpFilePath);
		bool fileExists = f.good();
		if (!fileExists)
			continue;

		// read file and set grids list with grids from file
		auto res = ReadVolumeDataFromFile(tmpFilePath, gridParams);
		if (!std::get<bool>(res))
			continue; 

		for (auto it = gridParams.begin(); it != gridParams.end(); ++it)
		{
			const std::string& gridName = it->first;
			auto& maxGrid = maxGridParams[gridName];

			if (it->second.upperBound[0] > maxGrid.gridSizeX)
				maxGrid.gridSizeX = it->second.upperBound[0];

			if (it->second.upperBound[1] > maxGrid.gridSizeY)
				maxGrid.gridSizeY = it->second.upperBound[1];

			if (it->second.upperBound[2] > maxGrid.gridSizeZ)
				maxGrid.gridSizeZ = it->second.upperBound[2];
		}
	}

	// save voxel sizes
	for (auto it = gridParams.begin(); it != gridParams.end(); ++it)
	{
		const std::string& gridName = it->first;
		auto& maxGrid = maxGridParams[gridName];

		maxGrid.voxelSizeX = it->second.voxelSizeX;
		maxGrid.voxelSizeY = it->second.voxelSizeY;
		maxGrid.voxelSizeZ = it->second.voxelSizeZ;
	}
}

void RPRVolumeAttributes::SetupVolumeFromFile(MObject& node, VDBGridParams& gridParams, VDBGridParams& maxGridParams, bool shouldSetup /*= false*/)
{
	// get .vdb file from UI form
	std::string filename = GetVDBFilePath(node);
	if (filename.empty())
		return;

	// setup volume data from file
	if (shouldSetup)
	{
		// for animation
		GetMaxGridSize(filename, node, maxGridParams);
	}

	// read file and set grids list with grids from file
	ReadVolumeDataFromFile(filename, gridParams);

	// write grid names to array attribute
	// - get array builder
	MStatus status;
	MPlug wPlug(node, RPRVolumeAttributes::loadedGrids);
	MDataHandle wHandle = wPlug.asMDataHandle(&status);
	MArrayDataHandle arrayHandle(wHandle, &status);
	status = arrayHandle.jumpToArrayElement(0);
	CHECK_MSTATUS(status);
	MArrayDataBuilder arrayBuilder = arrayHandle.builder(&status);
	CHECK_MSTATUS(status);

	// - clear array attribute from old data
	MIntArray indices;
	wPlug.getExistingArrayAttributeIndices(indices, &status);

	for (unsigned int idx = 0; idx < indices.length(); ++idx) // turns out removeElement takes not the element in the array index but "internal" index
	{
		MStatus success = arrayBuilder.removeElement(indices[idx]);
		CHECK_MSTATUS(success);
	}

	// - write fresh data that was read from grids into array attribute
	auto& tmpGridParams = (maxGridParams.size() > 0) ? maxGridParams : gridParams;
	for (auto it = tmpGridParams.begin(); it != tmpGridParams.end(); ++it)
	{
		MDataHandle handle = arrayBuilder.addLast(&status);
		MString val = MString(it->first.c_str());
		handle.set(val);		 
	}

	status = arrayHandle.set(arrayBuilder);
	wPlug.setValue(wHandle);
	wPlug.destructHandle(wHandle);

	// remember current selection
	MSelectionList currentSelection;
	MGlobal::getActiveSelectionList(currentSelection);
	// update UI
	MFnDagNode dagNode(node);
	MString partialPathName = dagNode.partialPathName();
	MString selectNode = "select -r " + partialPathName + ";\n"; // we have to select to ensure that ui gets created
	MString source = "source \"AERPRVolumeTemplate.mel\";\n";
	MString command = selectNode + source + "catchQuiet(`FillGridList(\"" + partialPathName + "\")`);\n";
	MStatus res = MGlobal::executeCommandOnIdle(command);
	CHECK_MSTATUS(res);

	// setup new grid parameters
	MFnDependencyNode depNode(node);
	MPlug densityPlug = depNode.findPlug(RPRVolumeAttributes::densitySelectedGrid);
	SetupGridSizeFromFile(node, densityPlug, tmpGridParams);
	// set selection to previous selection
	MGlobal::setActiveSelectionList(currentSelection);
}

void RPRVolumeAttributes::SetupGridSizeFromFile(MObject& node, MPlug& plug, VDBGridParams& gridParams)
{
	std::string filename = GetVDBFilePath(node);
	MFnDependencyNode depNode(node);
	if (plug == RPRVolumeAttributes::densitySelectedGrid)
	{
		// set selected grid dementions in ui
		bool failed = false;
		std::string selectedGridName = GetSelectedDensityGridName(depNode, filename, failed).asChar();
		if (!failed)
		{
			MPlug dimensionsPlug = depNode.findPlug(RPRVolumeAttributes::volumeDimensionsDensity);
			MPlug voxelSizePlug = depNode.findPlug(RPRVolumeAttributes::volumeVoxelSizeDensity);
			SetVolumeUIGridSize(gridParams[selectedGridName], dimensionsPlug);
			SetVolumeUIVoxelSize(gridParams[selectedGridName], voxelSizePlug);
		} else {
			MGlobal::displayWarning("invalid density grid value");
		}
	}
	else if (plug == RPRVolumeAttributes::albedoSelectedGrid)
	{
		// set selected grid dementions in ui
		bool failed = false;
		std::string selectedGridName = GetSelectedAlbedoGridName(depNode, filename, failed).asChar();
		if (!failed)
		{
			MPlug plug = depNode.findPlug(RPRVolumeAttributes::volumeDimensionsAlbedo);
			SetVolumeUIGridSize(gridParams[selectedGridName], plug);
		} else {
			MGlobal::displayWarning("invalid albedo grid value");
		}
	}
	else if (plug == RPRVolumeAttributes::emissionSelectedGrid)
	{
		// set selected grid dementions in ui
		bool failed = false;
		std::string selectedGridName = GetSelectedEmissionGridName(depNode, filename, failed).asChar();
		if (!failed)
		{
			MPlug plug = depNode.findPlug(RPRVolumeAttributes::volumeDimensionsEmission);
			SetVolumeUIGridSize(gridParams[selectedGridName], plug);
		} else {
			MGlobal::displayWarning("invalid emission grid value");
		}
	}
}

template <typename T>
void CopyLookupValue(std::vector<float>& lookupTableOut, const std::vector<T>& remapedRampValue);

template <>
void CopyLookupValue(std::vector<float>& lookupTableOut, const std::vector<float>& remapedRampValue)
{
	lookupTableOut.resize(remapedRampValue.size() * 3);
	for (size_t idx = 0; idx < remapedRampValue.size(); ++idx)
	{
		lookupTableOut[idx * 3]		= remapedRampValue[idx];
		lookupTableOut[idx * 3 + 1] = remapedRampValue[idx];
		lookupTableOut[idx * 3 + 2] = remapedRampValue[idx];
	}
}

template <>
void CopyLookupValue(std::vector<float>& lookupTableOut, const std::vector<MColor>& remapedRampValue)
{
	lookupTableOut.resize(remapedRampValue.size() * 3);
	for (size_t idx = 0; idx < remapedRampValue.size(); ++idx)
	{
		lookupTableOut[idx * 3]		= remapedRampValue[idx].r;
		lookupTableOut[idx * 3 + 1] = remapedRampValue[idx].g;
		lookupTableOut[idx * 3 + 2] = remapedRampValue[idx].b;
	}
}

template <typename MayaArrayT, typename valTypeT>
void SetupLookupTableFromRamp(VDBGrid<float>& dataGrid, MPlug& rampPlug)
{
	using MayaElementT = decltype(
		std::declval<MayaArrayT&>()[std::declval<unsigned int>()]
	);
	static_assert(std::is_same<MayaElementT, valTypeT&>::value, "array type mismatch");

	dataGrid.valuesLookUpTable.resize(100);
	std::vector<RampCtrlPoint<valTypeT>> ctrlPoints;
	GetRampValues<MayaArrayT, valTypeT>(rampPlug, ctrlPoints);
	std::vector<valTypeT> remapedRampValue(100, 0.0f);
	RemapRampControlPoints(remapedRampValue.size(), remapedRampValue, ctrlPoints);
	CopyLookupValue(dataGrid.valuesLookUpTable, remapedRampValue);
}

void CopyGridSizeValues(VDBGrid<float>& destination, const VDBGridSize& source)
{
	destination.size.gridSizeX = source.gridSizeX;
	destination.size.gridSizeY = source.gridSizeY;
	destination.size.gridSizeZ = source.gridSizeZ;
}

void RPRVolumeAttributes::FillVolumeData(VDBVolumeData& data, const MObject& node)
{
	MFnDependencyNode depNode(node);

	std::string filename = GetVDBFilePath(depNode);
	if (filename.empty())
		return;

	VDBGridParams maxGridParams;
	GetMaxGridSize(filename, node, maxGridParams);
	bool treatAsAnimation = maxGridParams.size() > 0;

	// process vdb file
	// initialize openvdb; it is necessary to call it before beginning working with vdb
	openvdb::initialize();

	// create a VDB file object.
	openvdb::io::File file(filename);

	try
	{
		// open the file; this reads the file header, but not any grids.
		file.open();
	}
	catch (openvdb::IoError ex)
	{
		// display error message in Maya
		std::string err = ex.what();
		MGlobal::displayError(MString(err.c_str()));

		return;
	}

	try
	{
		// read density
		if (GetDensityEnabled(depNode))
		{
			bool failed = false;
			std::string densityGridName = GetSelectedDensityGridName(depNode, filename, failed).asChar();
			if (!failed)
			{
				ReadFileGridToVDBGrid(data.densityGrid, file, densityGridName);

				if (treatAsAnimation)
				{
					CopyGridSizeValues(data.densityGrid, maxGridParams[densityGridName]);
				}

				// - setup look up table values
				ProcessDensityGrid(data.densityGrid.gridOnValueIndices, data.densityGrid.valuesLookUpTable, data.densityGrid.minValue, data.densityGrid.maxValue, GetDensityMultiplier(depNode));
				MPlug densityRampPlug = RPRVolumeAttributes::GetDensityRamp(node);
				SetupLookupTableFromRamp<MFloatArray, float>(data.densityGrid, densityRampPlug);
			} else {
				MGlobal::displayWarning("invalid density grid value");
			}
		}

		// read albedo
		if (GetAlbedoEnabled(depNode))
		{
			bool failed = false;
			std::string albedoGridName = GetSelectedAlbedoGridName(depNode, filename, failed).asChar();
			if (!failed)
			{
				ReadFileGridToVDBGrid(data.albedoGrid, file, albedoGridName);

				if (treatAsAnimation)
				{
					CopyGridSizeValues(data.albedoGrid, maxGridParams[albedoGridName]);
				}

				// - setup look up table values
				ProcessTemperatureGrid(data.albedoGrid.gridOnValueIndices, data.albedoGrid.minValue, data.albedoGrid.maxValue);
				MPlug albedoRampPlug = RPRVolumeAttributes::GetAlbedoRamp(node);
				SetupLookupTableFromRamp<MColorArray, MColor>(data.albedoGrid, albedoRampPlug);
			} else {
				MGlobal::displayWarning("invalid albedo grid value");
			}
		}

		// read emission
		if (GetEmissionEnabled(depNode))
		{
			bool failed = false;
			std::string emissionGridName = GetSelectedEmissionGridName(depNode, filename, failed).asChar();
			if (!failed)
			{
				ReadFileGridToVDBGrid(data.emissionGrid, file, emissionGridName);
	
				if (treatAsAnimation)
				{
					CopyGridSizeValues(data.emissionGrid, maxGridParams[emissionGridName]);
				}
	
				// - setup look up table values
				ProcessTemperatureGrid(data.emissionGrid.gridOnValueIndices, data.emissionGrid.minValue, data.emissionGrid.maxValue);
				MPlug emissionRampPlug = RPRVolumeAttributes::GetEmissionValueRamp(node);
				SetupLookupTableFromRamp<MColorArray, MColor>(data.emissionGrid, emissionRampPlug);
			} else {
				MGlobal::displayWarning("invalid emission grid value");
			}
		}

		// close the file.
		file.close();
	}
	catch (openvdb::Exception& ex)
	{
		// display error message in Maya
		std::string err = ex.what();
		MGlobal::displayError(MString(err.c_str()));

		return;
	}
}

template <typename T>
bool RPRVolumeAttributes::GetSingleGridData(VDBGrid<T>& outGridData, const MObject& node, const std::string& gridName)
{
	MFnDependencyNode depNode(node);

	std::string filename = GetVDBFilePath(depNode);
	if (filename.empty())
		return false;

	// process vdb file
	// initialize openvdb; it is necessary to call it before beginning working with vdb
	openvdb::initialize();

	// create a VDB file object.
	openvdb::io::File file(filename);

	try
	{
		// open the file; this reads the file header, but not any grids.
		file.open();
	}
	catch (openvdb::IoError ex)
	{
		// display error message in Maya
		std::string err = ex.what();
		MGlobal::displayError(MString(err.c_str()));

		return false;
	}

	try
	{
		auto res = ReadFileGridToVDBGrid(outGridData, file, gridName);
		if (!std::get<bool>(res))
		{
			std::string& errMsg = std::get<std::string>(res);
			MGlobal::displayError(MString(errMsg.c_str()));
			return false;
		}
	}
	catch (openvdb::Exception& ex)
	{
		// display error message in Maya
		std::string err = ex.what();
		MGlobal::displayError(MString(err.c_str()));

		return false;
	}

	return true;
}

void RPRVolumeAttributes::FillVolumeData(VolumeData& data, const MObject& node, FireMaya::Scope* scope)
{
	short3& volumeDims = RPRVolumeAttributes::GetVolumeGridDimentions(node).asShort3();
	data.gridSizeX = volumeDims[0];
	data.gridSizeY = volumeDims[1];
	data.gridSizeZ = volumeDims[2];

	float density_multiplier = 1.0f;
	float emission_intensity = 1.0f;
	float emission_kelvin = 1.0f;
	EmissionInputType emission_type = kByValue;

	data.voxels.clear();
	data.voxels.resize(volumeDims[0] * volumeDims[1] * volumeDims[2]);

	std::vector<RampCtrlPoint<MColor>> albedoCtrlPoints;
	std::vector<RampCtrlPoint<MColor>> emisionCtrlPoints;
	std::vector<RampCtrlPoint<float>> emisionIntensityCtrlPoints;
	std::vector<RampCtrlPoint<float>> denstiyCtrlPoints;
	
	bool isAlbedoEnabled = RPRVolumeAttributes::GetAlbedoEnabled(node);
	if (isAlbedoEnabled)
	{
		MPlug albedoRampPlug = RPRVolumeAttributes::GetAlbedoRamp(node);
		GetRampValues<MColorArray, MColor>(albedoRampPlug, albedoCtrlPoints);
	}

	bool isEmissionEnabled = RPRVolumeAttributes::GetEmissionEnabled(node);
	if (isEmissionEnabled)
	{
		MPlug emissionRampPlug = RPRVolumeAttributes::GetEmissionValueRamp(node);
		GetRampValues<MColorArray, MColor>(emissionRampPlug, emisionCtrlPoints);
		emission_intensity = RPRVolumeAttributes::GetEmissionIntensity(node);
		emission_kelvin = RPRVolumeAttributes::GetEmissionKelvin(node);
		emission_type = RPRVolumeAttributes::GetEmissionInputType(node);
		MPlug emissionIntensityRampPlug = RPRVolumeAttributes::GetEmissionIntensityRamp(node);
		GetRampValues<MFloatArray, float>(emissionIntensityRampPlug, emisionIntensityCtrlPoints);
	}

	bool isDensityEnabled = RPRVolumeAttributes::GetDensityEnabled(node);
	if (isDensityEnabled)
	{
		MPlug densityRampPlug = RPRVolumeAttributes::GetDensityRamp(node);
		GetRampValues<MFloatArray, float>(densityRampPlug, denstiyCtrlPoints);
		density_multiplier = RPRVolumeAttributes::GetDensityMultiplier(node);
	}

	VolumeGradient albedoGradientType = RPRVolumeAttributes::GetAlbedoGradientType(node);
	VolumeGradient emissionGradientType = RPRVolumeAttributes::GetEmissionGradientType(node);
	VolumeGradient densiyGradientType = RPRVolumeAttributes::GetDensityGradientType(node);

	size_t voxel_idx = 0;
	for (size_t z_idx = 0; z_idx < data.gridSizeZ; ++z_idx)
		for (size_t y_idx = 0; y_idx < data.gridSizeY; ++y_idx)
			for (size_t x_idx = 0; x_idx < data.gridSizeX; ++x_idx)
			{
				VoxelParams voxelParams;
				voxelParams.x = (unsigned int) x_idx;
				voxelParams.y = (unsigned int) y_idx;
				voxelParams.z = (unsigned int) z_idx;
				voxelParams.Xres = (unsigned int) data.gridSizeX;
				voxelParams.Yres = (unsigned int) data.gridSizeY;
				voxelParams.Zres = (unsigned int) data.gridSizeZ;

				// fill voxels with data
				if (isAlbedoEnabled)
				{
					MColor voxelAlbedoColor = GetVoxelValue<MColor>(voxelParams, albedoCtrlPoints, albedoGradientType);

					data.voxels[voxel_idx].aR = voxelAlbedoColor.r;
					data.voxels[voxel_idx].aG = voxelAlbedoColor.g;
					data.voxels[voxel_idx].aB = voxelAlbedoColor.b;
				}

				if (isEmissionEnabled)
				{
					if (emission_type == kByValue)
					{
						float emission_ramp_intensity = GetVoxelValue<float>(voxelParams, emisionIntensityCtrlPoints, emissionGradientType);
						float emission_multiplier = emission_intensity * emission_ramp_intensity;

						MColor voxelEmissionColor = GetVoxelValue<MColor>(voxelParams, emisionCtrlPoints, emissionGradientType);

						data.voxels[voxel_idx].eR = voxelEmissionColor.r * emission_multiplier;
						data.voxels[voxel_idx].eG = voxelEmissionColor.g * emission_multiplier;
						data.voxels[voxel_idx].eB = voxelEmissionColor.b * emission_multiplier;
					}
				}

				if (isDensityEnabled)
				{
					float voxelDensityValue = GetVoxelValue<float>(voxelParams, denstiyCtrlPoints, densiyGradientType);

					data.voxels[voxel_idx].density = voxelDensityValue * density_multiplier;
				}

				voxel_idx++;
			}


}

float GetDistanceBetweenPoints(
	float x, float y, float z,
	std::array<float, 3> point)
{
	return sqrt((point[0] - x)*(point[0] - x) + (point[1] - y)*(point[1] - y) + (point[2] - z)*(point[2] - z));
}

float GetDistParamNormalized(
	const VoxelParams& voxelParams,
	VolumeGradient gradientType
)
{
	float dist2vx_normalized; // this is parameter that is used for Ramp input

	float fXres = 1.0f * voxelParams.Xres;
	float fYres = 1.0f * voxelParams.Yres;
	float fZres = 1.0f * voxelParams.Zres;
	float fx = 1.0f * voxelParams.x;
	float fy = 1.0f * voxelParams.y;
	float fz = 1.0f * voxelParams.z;

	switch (gradientType)
	{
		case VolumeGradient::kConstant:
		{
			dist2vx_normalized = 1.0f;
			break;
		}

		case VolumeGradient::kXGradient:
		{
			// get distance between YZ plane and point (fx, fy, fz)
			/*d = | A*Mx + B*My + C*Mz + D | /	SQRT(A^2 + B^2 + C^2)*/
			dist2vx_normalized = 1 - (fx / fXres);

			break;
		}

		case VolumeGradient::kYGradient:
		{
			// get relative distance between XZ plane and point (fx, fy, fz)
			/*d = | A*Mx + B*My + C*Mz + D | /	SQRT(A^2 + B^2 + C^2)*/
			dist2vx_normalized = 1 - (fy / fYres);

			break;
		}

		case VolumeGradient::kZGradient:
		{
			// get relative distance between XY plane and point (fx, fy, fz)
			/*d = | A*Mx + B*My + C*Mz + D | /	SQRT(A^2 + B^2 + C^2)*/
			dist2vx_normalized = 1 - (fz / fZres);

			break;
		}

		case VolumeGradient::kNegXGradient:
		{
			// get distance between YZ plane and point (fx, fy, fz)
			/*d = | A*Mx + B*My + C*Mz + D | /	SQRT(A^2 + B^2 + C^2)*/
			dist2vx_normalized = fx / fXres;

			break;
		}

		case VolumeGradient::kNegYGradient:
		{
			// get relative distance between XZ plane and point (fx, fy, fz)
			/*d = | A*Mx + B*My + C*Mz + D | /	SQRT(A^2 + B^2 + C^2)*/
			dist2vx_normalized = fy / fYres;

			break;
		}

		case VolumeGradient::kNegZGradient:
		{
			// get relative distance between XY plane and point (fx, fy, fz)
			/*d = | A*Mx + B*My + C*Mz + D | /	SQRT(A^2 + B^2 + C^2)*/
			dist2vx_normalized = fz / fZres;

			break;
		}

		case VolumeGradient::kCenterGradient: // 0.0 is border, 1.0 is center
		{
			// get relative distance from current voxel to center
			float dist2vx = GetDistanceBetweenPoints(fXres / 2, fYres / 2, fZres / 2, std::array<float, 3> {fx, fy, fz});
			/*std::tie(hasIntersections, dist2center) = GetDistanceToCenter(fx, fy, fz, fXres, fYres, fZres);
			if (!hasIntersections)
				return 100*1.0f; */
			float dist2center = GetDistanceBetweenPoints(fXres / 2, fYres / 2, fZres / 2, std::array<float, 3> {0.0f, 0.0f, 0.0f});
			dist2vx_normalized = 1 - (dist2vx / dist2center);
			break;
		}

	default:
		dist2vx_normalized = 0.0f; // atm only center gradient is supported
	}

	return dist2vx_normalized;
}
