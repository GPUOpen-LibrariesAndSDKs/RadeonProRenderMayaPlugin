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
#include "FireRenderStandardMaterial.h"
#include "FireRenderDisplacement.h"
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MDataHandle.h>
#include <maya/MFloatVector.h>
#include <maya/MPlugArray.h>
#include <maya/MDGModifier.h>
#include <maya/MDistance.h>

#include "FireMaya.h"

// Comment next line to use old UberMaterial code. Should remove USE_RPRX=0 path when UberMaterial will pass all tests.
#define USE_RPRX 1

namespace
{
	namespace Attribute
	{
		MObject version;
		MObject output;
		MObject outputAlpha;

		// Diffuse
		MObject diffuseEnable;
		MObject diffuseColor;
		MObject diffuseWeight;
		MObject diffuseRoughness;
		MObject useShaderNormal;
		MObject diffuseNormal;
		MObject backscatteringWeight;
		MObject separateBackscatterColor;
		MObject backscatteringColor;


		// Reflection
		MObject reflectionEnable;
		MObject reflectionColor;

		MObject reflectionWeight;
		MObject reflectionRoughness;
		MObject reflectionAnisotropy;
		MObject reflectionAnisotropyRotation;
		MObject reflectionMetalMaterial;
		MObject reflectionMetalness;
		MObject reflectUseShaderNormal;
		MObject reflectNormal;

		MObject reflectionIOR;
		MObject reflectionIsFresnelApproximationOn;
		MObject reflectionRoughnessX;	// used for upgrade v1 -> v2

		// Coating
		MObject clearCoatEnable;
		MObject clearCoatColor;
		MObject clearCoatIOR;

		MObject clearCoatWeight;
		MObject clearCoatRoughness;
		MObject clearCoatMetalMaterial;
		MObject clearCoatMetalness;
		MObject coatUseShaderNormal;
		MObject coatNormal;

		MObject clearCoatThickness;
		MObject clearCoatTransmissionColor;

		// Refraction
		MObject refractionEnable;
		MObject refractionColor;
		MObject refractionWeight;
		MObject refractionRoughness;
		MObject refractionIOR;
		MObject refractionAbsorptionColor;
		MObject refractionUseShaderNormal;
		MObject refractionNormal;

		MObject refractionLinkToReflection;
		MObject refractionThinSurface;
		MObject refractionAbsorptionDistance;
		MObject refractionAllowCaustics;

		// Emissive
		MObject emissiveEnable;
		MObject emissiveColor;
		MObject emissiveWeight;
		MObject emissiveIntensity;
		MObject emissiveDoubleSided;

		// Material parameters
		MObject transparencyEnable;
		MObject transparencyLevel;
		MObject transparency3Components;
		MObject useTransparency3Components;

		MObject normalMap;
		MObject normalMapEnable;

		MObject displacementEnable;
		MObject displacementMin;
		MObject displacementMax;

		// Displacement
		MObject displacementEnableAdaptiveSubdiv;
		MObject displacementASubdivFactor;
		MObject displacementSubdiv;
		MObject displacementCreaseWeight;
		MObject displacementBoundary;
		MObject displacementMap;			// warning: not used in old UberShader

		// SSS
		MObject sssEnable;
		MObject sssUseDiffuseColor;
		MObject sssWeight;

		// Volume
		MObject volumeScatter;				// scatter color
		MObject subsurfaceRadius;
		MObject volumeScatteringDirection;	//+
		MObject volumeMultipleScattering;	//+ ! single scattering

		// Sheen
		MObject sheenEnabled;
		MObject sheenWeight;
		MObject sheenTint;
		MObject sheenColor;

		// Old attributes declared for backwards compatibility
		MObject diffuseBaseNormal;
		MObject reflectionNormal;
		MObject clearCoatNormal;
	}
}

// Attributes
void FireMaya::StandardMaterial::postConstructor()
{
	ShaderNode::postConstructor();
	setMPSafe(true);
	MStatus status = setExistWithoutInConnections(true);
	CHECK_MSTATUS(status);
}

void FireMaya::StandardMaterial::OnFileLoaded()
{
#if USE_RPRX
	// Execute upgrade code
	UpgradeMaterial();
#endif
}

enum
{
	VER_INITIAL = 1,
	VER_RPRX_MATERIAL = 2,
	VER_UBER3_MATERIAL = 3,

	VER_CURRENT_PLUS_ONE,
	VER_CURRENT = VER_CURRENT_PLUS_ONE - 1
};

// Copy value of srcAttr to dstAttr
bool CopyAttribute(MFnDependencyNode& node, const MObject& srcAttr, const MObject& dstAttr, bool onlyNonEmpty = false)
{
	// Find plugs for both attributes
	MStatus status;
	MPlug src = node.findPlug(srcAttr, false, &status);
	CHECK_MSTATUS(status);
	MPlug dst = node.findPlug(dstAttr, false, &status);
	CHECK_MSTATUS(status);

	MPlugArray connections;
	src.connectedTo(connections, true, false);

	if (onlyNonEmpty)
	{
		if (connections.length() == 0)
			return false;
	}

	// Copy non-network value
	MDataHandle data = dst.asMDataHandle();
	data.copy(src.asMDataHandle());
	dst.setMDataHandle(data);

	// Copy network value
	if (connections.length())
	{
		MDGModifier modifier;
		for (unsigned int i = 0; i < connections.length(); i++)
		{
			DebugPrint("Connecting %s to %s", connections[i].name().asChar(), dst.name().asChar());
			modifier.connect(connections[i], dst);
			modifier.doIt();
		}
	}

	return true;
}

void BreakConnections(MFnDependencyNode& node, const MObject& attr)
{
	MStatus status;
	MPlug plug = node.findPlug(attr, false, &status);
	CHECK_MSTATUS(status);

	MPlugArray connections;
	plug.connectedTo(connections, true, false);

	if (connections.length())
	{
		MDGModifier modifier;
		for (unsigned int i = 0; i < connections.length(); i++)
		{
			modifier.disconnect(connections[i], plug);
			modifier.doIt();
		}
	}
}

void FireMaya::StandardMaterial::UpgradeMaterial()
{
#if USE_RPRX
	MFnDependencyNode shaderNode(thisMObject());

	MPlug plug = shaderNode.findPlug(Attribute::version, false);
	int version = plug.asInt();

	if (version < VER_CURRENT && version == VER_INITIAL)
	{
		LogPrint("UpgradeMaterial: %s from ver %d", shaderNode.name().asChar(), version);
		//		CopyAttribute(shaderNode, Attribute::diffuseColor, Attribute::reflectionColor); -- this is for test: diffuse color will be copied to reflection color

		// Old shader model: Reflection Roughness X | Y, new shader moder: Reflection Roughness
		CopyAttribute(shaderNode, Attribute::reflectionRoughnessX, Attribute::reflectionRoughness);

		// Upgrade normal maps. Old material model had 4 normal maps, new one - only 1
		bool hasNormal = true;
		if (!CopyAttribute(shaderNode, Attribute::diffuseBaseNormal, Attribute::normalMap, true))
		{
			if (!CopyAttribute(shaderNode, Attribute::reflectionNormal, Attribute::normalMap, true))
			{
				if (!CopyAttribute(shaderNode, Attribute::clearCoatNormal, Attribute::normalMap, true))
				{
					if (!CopyAttribute(shaderNode, Attribute::refractionNormal, Attribute::normalMap, true))
					{
						hasNormal = false;
					}
				}
			}
		}
		// Enable normal map if it is connected
		if (hasNormal)
		{
			shaderNode.findPlug(Attribute::normalMapEnable, false).setBool(true);
		}

		BreakConnections(shaderNode, Attribute::reflectionRoughnessX);
		BreakConnections(shaderNode, Attribute::diffuseBaseNormal);
		BreakConnections(shaderNode, Attribute::reflectionNormal);
		BreakConnections(shaderNode, Attribute::clearCoatNormal);
		BreakConnections(shaderNode, Attribute::refractionNormal);
	}
#endif
}

// We're hooking 'shouldSave' to force new materials to be saved with VER_CURRENT. Without that, materials
// which were created during current session will be saved with default value of Attribute::version, which
// is set to minimal value for correct handling of older unversioned materials.
MStatus FireMaya::StandardMaterial::shouldSave(const MPlug& plug, bool& isSaving)
{
	if (plug.attribute() == Attribute::version)
	{
		MFnDependencyNode shaderNode(thisMObject());
		MPlug plug2 = shaderNode.findPlug(Attribute::version, false);
		CHECK_MSTATUS(plug2.setInt(VER_CURRENT));
	}
	// CaLL default implementation
	return MPxNode::shouldSave(plug, isSaving);
}

// creates an instance of the node
void* FireMaya::StandardMaterial::creator()
{
	return new StandardMaterial;
}

// initializes attribute information
// call by MAYA when this plug-in was loaded.
//
MStatus FireMaya::StandardMaterial::initialize()
{
	MFnNumericAttribute nAttr;
	MFnEnumAttribute eAttr;

#define DEPRECATED_PARAM(attr) \
	CHECK_MSTATUS(attr.setCached(false)); \
	CHECK_MSTATUS(attr.setStorable(false)); \
	CHECK_MSTATUS(attr.setHidden(true));

#define SET_SOFTMINMAX(attr, min, max) \
	CHECK_MSTATUS(attr.setSoftMin(min)); \
	CHECK_MSTATUS(attr.setSoftMax(max));

#define SET_MINMAX(attr, min, max) \
	CHECK_MSTATUS(attr.setMin(min)); \
	CHECK_MSTATUS(attr.setMax(max));

	// Create version attribute. Set default value to VER_INITIAL for correct processing
	// of old un-versioned nodes.
	Attribute::version = nAttr.create("materialVersion", "mtlver", MFnNumericData::kInt, VER_INITIAL);
	CHECK_MSTATUS(nAttr.setCached(false));
	CHECK_MSTATUS(nAttr.setHidden(true));
	CHECK_MSTATUS(addAttribute(Attribute::version));

	// Diffuse
	Attribute::diffuseEnable = nAttr.create("diffuse", "dif", MFnNumericData::kBoolean, 1);
	MAKE_INPUT_CONST(nAttr);

	Attribute::diffuseColor = nAttr.createColor("diffuseColor", "dc");
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(0.644f, 0.644f, 0.644f));

	Attribute::diffuseWeight = nAttr.create("diffuseWeight", "dw", MFnNumericData::kFloat, 1.0);
	MAKE_INPUT(nAttr);
	///	CHECK_MSTATUS(nAttr.setDefault(1.0));
	SET_MINMAX(nAttr, 0.0, 1.0);

	Attribute::diffuseRoughness = nAttr.create("diffuseRoughness", "dr", MFnNumericData::kFloat, 1.0);
	MAKE_INPUT(nAttr);
	///	CHECK_MSTATUS(nAttr.setDefault(1.0));
	SET_MINMAX(nAttr, 0.0, 1.0);

	Attribute::useShaderNormal = nAttr.create("useShaderNormal", "dun", MFnNumericData::kBoolean, 1);
	MAKE_INPUT_CONST(nAttr);
	
	Attribute::diffuseNormal = nAttr.createColor("diffuseNormal", "dn");
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(1.0f, 1.0f, 1.0f));

	Attribute::backscatteringWeight = nAttr.create("backscatteringWeight", "dbw", MFnNumericData::kFloat, 0.0);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0.0);
	nAttr.setSoftMax(1.0);

	Attribute::separateBackscatterColor = nAttr.create("separateBackscatterColor", "dsb", MFnNumericData::kBoolean, 0);
	MAKE_INPUT_CONST(nAttr);

	Attribute::backscatteringColor = nAttr.createColor("backscatteringColor", "dbc");
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(0.5f, 0.5f, 0.5f));

	// Reflection
	Attribute::reflectionEnable = nAttr.create("reflections", "gr", MFnNumericData::kBoolean, 0);
	MAKE_INPUT_CONST(nAttr);

	Attribute::reflectionColor = nAttr.createColor("reflectColor", "grc");
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(1.0f, 1.0f, 1.0f));

	Attribute::reflectionWeight = nAttr.create("reflectWeight", "rw", MFnNumericData::kFloat, 1.0);
	MAKE_INPUT(nAttr);
	SET_MINMAX(nAttr, 0.0, 1.0);

	Attribute::reflectionRoughness = nAttr.create("reflectRoughness", "rr", MFnNumericData::kFloat, 0.25);
	MAKE_INPUT(nAttr);
	SET_MINMAX(nAttr, 0.0, 1.0);

	Attribute::reflectionAnisotropy = nAttr.create("reflectAnisotropy", "ra", MFnNumericData::kFloat, 0.0);
	MAKE_INPUT(nAttr);
	SET_MINMAX(nAttr, -1.0, 1.0);

	Attribute::reflectionAnisotropyRotation = nAttr.create("reflectAnisotropyRotation", "rar", MFnNumericData::kFloat, 0.0);
	MAKE_INPUT_CONST(nAttr);
	SET_MINMAX(nAttr, -1.0, 1.0);

	Attribute::reflectUseShaderNormal = nAttr.create("reflectUseShaderNormal", "run", MFnNumericData::kBoolean, 1);
	MAKE_INPUT_CONST(nAttr);

	Attribute::reflectNormal = nAttr.createColor("reflectNormal", "rn");
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(1.0f, 1.0f, 1.0f));

	Attribute::reflectionMetalMaterial = nAttr.create("reflectMetalMaterial", "rm", MFnNumericData::kBoolean, 0);
	MAKE_INPUT_CONST(nAttr);

	Attribute::reflectionMetalness = nAttr.create("reflectMetalness", "rmet", MFnNumericData::kFloat, 0);
	MAKE_INPUT(nAttr);
	SET_MINMAX(nAttr, 0.0, 1.0);

	Attribute::reflectionIOR = nAttr.create("reflectIOR", "grior", MFnNumericData::kFloat, 1.5);
	MAKE_INPUT(nAttr);
	SET_MINMAX(nAttr, 0.0, 10.0);

	Attribute::reflectionIsFresnelApproximationOn = nAttr.create("reflectIsFresnelApproximationOn", "rfao", MFnNumericData::kBoolean, 0);
	MAKE_INPUT(nAttr);

	// Coating
	Attribute::clearCoatEnable = nAttr.create("clearCoat", "cc", MFnNumericData::kBoolean, 0);
	MAKE_INPUT_CONST(nAttr);

	Attribute::clearCoatColor = nAttr.createColor("coatColor", "ccc");
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(1.0f, 1.0f, 1.0f));

	/// TODO
	Attribute::clearCoatWeight = nAttr.create("coatWeight", "ccw", MFnNumericData::kFloat, 1.0);
	MAKE_INPUT(nAttr);
	SET_MINMAX(nAttr, 0.0, 1.0);

	Attribute::clearCoatRoughness = nAttr.create("coatRoughness", "ccr", MFnNumericData::kFloat, 0.5);
	MAKE_INPUT(nAttr);
	SET_MINMAX(nAttr, 0.0, 1.0);

	Attribute::clearCoatIOR = nAttr.create("coatIor", "ccior", MFnNumericData::kFloat, 1.5);
	MAKE_INPUT(nAttr);
	SET_MINMAX(nAttr, 0.0, 10.0);

	Attribute::coatUseShaderNormal = nAttr.create("coatUseShaderNormal", "ccun", MFnNumericData::kBoolean, 1);
	MAKE_INPUT_CONST(nAttr);

	Attribute::coatNormal = nAttr.createColor("coatNormal", "ccn");
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(1.0f, 1.0f, 1.0f));

	Attribute::clearCoatThickness = nAttr.create("coatThickness", "ccth", MFnNumericData::kFloat, 1.0);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0.0);
	nAttr.setSoftMax(10.0);

	Attribute::clearCoatTransmissionColor = nAttr.createColor("coatTransmissionColor", "cctc");
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(1.0f, 1.0f, 1.0f));

	// Refraction
#if USE_RPRX
	Attribute::refractionEnable = nAttr.create("refraction", "ref", MFnNumericData::kBoolean, 0);
	MAKE_INPUT_CONST(nAttr);
#endif

	Attribute::refractionColor = nAttr.createColor("refractColor", "refc");
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(1.0f, 1.0f, 1.0f));

	Attribute::refractionWeight = nAttr.create("refractWeight", "refw", MFnNumericData::kFloat, 1.0);
	MAKE_INPUT(nAttr);
	SET_MINMAX(nAttr, 0.0, 1.0);

	Attribute::refractionRoughness = nAttr.create("refractRoughness", "refr", MFnNumericData::kFloat, 0.0);
	MAKE_INPUT(nAttr);
	SET_MINMAX(nAttr, 0.0, 1.0);

	Attribute::refractionIOR = nAttr.create("refractIor", "refior", MFnNumericData::kFloat, 1.5);
	MAKE_INPUT(nAttr);
	SET_MINMAX(nAttr, 0.0, 10.0);

	Attribute::refractionUseShaderNormal = nAttr.create("refractUseShaderNormal", "refu", MFnNumericData::kBoolean, 1);
	MAKE_INPUT_CONST(nAttr);

	Attribute::refractionNormal = nAttr.createColor("refractNormal", "refn");
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(1.0f, 1.0f, 1.0f));

	Attribute::refractionLinkToReflection = nAttr.create("refractLinkToReflect", "refl", MFnNumericData::kBoolean, 0);
	MAKE_INPUT_CONST(nAttr);

	Attribute::refractionThinSurface = nAttr.create("refractThinSurface", "refth", MFnNumericData::kBoolean, 0);
	MAKE_INPUT_CONST(nAttr);

	Attribute::refractionAbsorptionDistance = nAttr.create("refractAbsorptionDistance", "refd", MFnNumericData::kFloat, 0);
	MAKE_INPUT(nAttr);
	nAttr.setSoftMax(10.0);
	SET_MINMAX(nAttr, 0.0, 50000.0);

	Attribute::refractionAbsorptionColor = nAttr.createColor("refractAbsorbColor", "refa");
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(0.0f, 0.0f, 0.0f));

	Attribute::refractionAllowCaustics = nAttr.create("refractAllowCaustics", "reac", MFnNumericData::kBoolean, 0);
	MAKE_INPUT_CONST(nAttr);

	// Emissive
	Attribute::emissiveEnable = nAttr.create("emissive", "em", MFnNumericData::kBoolean, 0);
	MAKE_INPUT_CONST(nAttr);

	Attribute::emissiveColor = nAttr.createColor("emissiveColor", "emc");
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(1.0f, 1.0f, 1.0f));

	Attribute::emissiveWeight = nAttr.create("emissiveWeight", "emw", MFnNumericData::kFloat, 1.0);
	MAKE_INPUT(nAttr);
	SET_MINMAX(nAttr, 0.0, 1.0);

	Attribute::emissiveIntensity = nAttr.create("emissiveIntensity", "emi", MFnNumericData::kFloat, 1.0);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0.0);
	nAttr.setSoftMax(10.0);

	Attribute::emissiveDoubleSided = nAttr.create("emissiveDoubleSided", "emds", MFnNumericData::kBoolean, 0);
	MAKE_INPUT_CONST(nAttr);

	// Material parameters
	Attribute::transparencyLevel = nAttr.create("transparencyLevel", "trl", MFnNumericData::kFloat, 1.0);
	MAKE_INPUT(nAttr);
	SET_MINMAX(nAttr, 0.0, 1.0);

	Attribute::transparency3Components = nAttr.createColor("transparency3Components", "tr3c");
	MAKE_INPUT(nAttr);

	Attribute::useTransparency3Components = nAttr.create("useTransparency3Components", "ut3", MFnNumericData::kBoolean, 0 );
	MAKE_INPUT_CONST(nAttr);

	Attribute::displacementMap = nAttr.createColor("displacementMap", "disp");
	MAKE_INPUT(nAttr);

	Attribute::normalMap = nAttr.createColor("normalMap", "nm");
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(1.0f, 1.0f, 1.0f));

	Attribute::transparencyEnable = nAttr.create("transparencyEnable", "et", MFnNumericData::kBoolean, 0);
	MAKE_INPUT_CONST(nAttr);

	Attribute::displacementEnable = nAttr.create("displacementEnable", "en", MFnNumericData::kBoolean, 0);
	MAKE_INPUT_CONST(nAttr);

	Attribute::displacementMin = nAttr.create("displacementMin", "dspmn", MFnNumericData::kFloat, 0.0);
	MAKE_INPUT(nAttr);
	SET_MINMAX(nAttr, 0.0, 10000.0);

	Attribute::displacementMax = nAttr.create("displacementMax", "dspmx", MFnNumericData::kFloat, 0.01);
	MAKE_INPUT(nAttr);
	SET_MINMAX(nAttr, 0.0, 10000.0);

	Attribute::displacementSubdiv = nAttr.create("displacementSubdiv", "dsps", MFnNumericData::kByte, 4);
	MAKE_INPUT(nAttr);
	SET_MINMAX(nAttr, 0, 8);

	Attribute::displacementCreaseWeight = nAttr.create("displacementCreaseWeight", "dspcw", MFnNumericData::kFloat, 0.0);
	MAKE_INPUT(nAttr);
	SET_MINMAX(nAttr, 0.0, 100.0);

	Attribute::displacementBoundary = eAttr.create("displacementBoundary", "disb", Displacement::kDisplacement_EdgeOnly);
	eAttr.addField("Edge", Displacement::kDisplacement_EdgeOnly);
	eAttr.addField("Edge And Corner", Displacement::kDisplacement_EdgeAndCorner);
	MAKE_INPUT_CONST(eAttr);

	Attribute::displacementEnableAdaptiveSubdiv = nAttr.create("displacementEnableAdaptiveSubdiv", "daen", MFnNumericData::kBoolean, 0);
	MAKE_INPUT_CONST(nAttr);

	Attribute::displacementASubdivFactor = nAttr.create("displacementASubdivFactor", "dasf", MFnNumericData::kFloat, 1.0f);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0.001);
	nAttr.setSoftMax(10.0);

	Attribute::normalMapEnable = nAttr.create("normalMapEnable", "enm", MFnNumericData::kBoolean, 1);
	MAKE_INPUT_CONST(nAttr);

	// Subsurface layer
#if USE_RPRX
	Attribute::sssEnable = nAttr.create("sssEnable", "enss", MFnNumericData::kBoolean, 0);
	MAKE_INPUT_CONST(nAttr);

	Attribute::sssWeight = nAttr.create("sssWeight", "sssw", MFnNumericData::kFloat, 1.0);
	MAKE_INPUT(nAttr);
	SET_MINMAX(nAttr, 0.0, 1.0);

	Attribute::sssUseDiffuseColor = nAttr.create("sssUseDiffuseColor", "sssdif", MFnNumericData::kBoolean, 0);
	MAKE_INPUT_CONST(nAttr);
#endif

	Attribute::volumeScatter = nAttr.createColor("volumeScatter", "vs");
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(0.436f, 0.227f, 0.131f));

	Attribute::subsurfaceRadius = nAttr.create("subsurfaceRadius", "sssr", MFnNumericData::k3Float);
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(3.67f, 1.37f, 0.68f));
	nAttr.setMin(0.0f, 0.0f, 0.0f);
	nAttr.setMax(100000.0f, 100000.0f, 100000.0f); // maximum value for milimeters

	Attribute::volumeScatteringDirection = nAttr.create("scatteringDirection", "vsd", MFnNumericData::kFloat, 0.0);
	MAKE_INPUT(nAttr);
	SET_MINMAX(nAttr, -1.0, 1.0);

	Attribute::volumeMultipleScattering = nAttr.create("multipleScattering", "vms", MFnNumericData::kBoolean, true);
	MAKE_INPUT_CONST(nAttr);

	// output color
	Attribute::output = nAttr.createColor("outColor", "oc");
	MAKE_OUTPUT(nAttr);

	// output transparency
	Attribute::outputAlpha = nAttr.createColor("outTransparency", "ot");
	MAKE_OUTPUT(nAttr);

	// Sheen
	Attribute::sheenEnabled = nAttr.create("sheenEnabled", "she", MFnNumericData::kBoolean, 0);
	MAKE_INPUT_CONST(nAttr);

	Attribute::sheenWeight = nAttr.create("sheenWeight", "shw", MFnNumericData::kFloat, 1.0f);
	MAKE_INPUT(nAttr);
	SET_MINMAX(nAttr, 0.0f, 1.0f);

	Attribute::sheenTint = nAttr.create("sheenTint", "sht", MFnNumericData::kFloat, 0.5f);
	MAKE_INPUT(nAttr);
	SET_MINMAX(nAttr, 0.0f, 1.0f);

	Attribute::sheenColor = nAttr.createColor("sheenColor", "shc");
	MAKE_INPUT(nAttr);


	CHECK_MSTATUS(addAttribute(Attribute::output));
	CHECK_MSTATUS(addAttribute(Attribute::outputAlpha));

	// Register attribute and make it affecting output color and alpha
#define ADD_ATTRIBUTE(attr) \
	CHECK_MSTATUS(addAttribute(attr)); \
	CHECK_MSTATUS(attributeAffects(attr, Attribute::output)); \
	CHECK_MSTATUS(attributeAffects(attr, Attribute::outputAlpha));

	ADD_ATTRIBUTE(Attribute::diffuseEnable);
	ADD_ATTRIBUTE(Attribute::diffuseColor);
	ADD_ATTRIBUTE(Attribute::diffuseWeight);
	ADD_ATTRIBUTE(Attribute::diffuseRoughness);
	ADD_ATTRIBUTE(Attribute::useShaderNormal);
	ADD_ATTRIBUTE(Attribute::diffuseNormal);
	ADD_ATTRIBUTE(Attribute::backscatteringWeight);
	ADD_ATTRIBUTE(Attribute::backscatteringColor);
	ADD_ATTRIBUTE(Attribute::separateBackscatterColor);

	ADD_ATTRIBUTE(Attribute::reflectionEnable);
	ADD_ATTRIBUTE(Attribute::reflectionColor);
	ADD_ATTRIBUTE(Attribute::reflectionWeight);
	ADD_ATTRIBUTE(Attribute::reflectionRoughness);
	ADD_ATTRIBUTE(Attribute::reflectionAnisotropy);
	ADD_ATTRIBUTE(Attribute::reflectionAnisotropyRotation);
	ADD_ATTRIBUTE(Attribute::reflectUseShaderNormal);
	ADD_ATTRIBUTE(Attribute::reflectNormal);
	ADD_ATTRIBUTE(Attribute::reflectionMetalMaterial);
	ADD_ATTRIBUTE(Attribute::reflectionMetalness);
	ADD_ATTRIBUTE(Attribute::reflectionIOR);
	ADD_ATTRIBUTE(Attribute::reflectionIsFresnelApproximationOn);

	ADD_ATTRIBUTE(Attribute::clearCoatEnable);
	ADD_ATTRIBUTE(Attribute::clearCoatColor);
	ADD_ATTRIBUTE(Attribute::clearCoatIOR);
	ADD_ATTRIBUTE(Attribute::clearCoatWeight);
	ADD_ATTRIBUTE(Attribute::clearCoatRoughness);
	ADD_ATTRIBUTE(Attribute::coatUseShaderNormal);
	ADD_ATTRIBUTE(Attribute::coatNormal);
	ADD_ATTRIBUTE(Attribute::clearCoatThickness);
	ADD_ATTRIBUTE(Attribute::clearCoatTransmissionColor);

	ADD_ATTRIBUTE(Attribute::refractionEnable);
	ADD_ATTRIBUTE(Attribute::refractionColor);
	ADD_ATTRIBUTE(Attribute::refractionWeight);
	ADD_ATTRIBUTE(Attribute::refractionRoughness);
	ADD_ATTRIBUTE(Attribute::refractionIOR);
	ADD_ATTRIBUTE(Attribute::refractionUseShaderNormal);
	ADD_ATTRIBUTE(Attribute::refractionNormal);
	ADD_ATTRIBUTE(Attribute::refractionLinkToReflection);
	ADD_ATTRIBUTE(Attribute::refractionThinSurface);
	ADD_ATTRIBUTE(Attribute::refractionAbsorptionDistance);
	ADD_ATTRIBUTE(Attribute::refractionAbsorptionColor);
	ADD_ATTRIBUTE(Attribute::refractionAllowCaustics);

	ADD_ATTRIBUTE(Attribute::emissiveEnable);
	ADD_ATTRIBUTE(Attribute::emissiveColor);
	ADD_ATTRIBUTE(Attribute::emissiveWeight);
	ADD_ATTRIBUTE(Attribute::emissiveIntensity);
	ADD_ATTRIBUTE(Attribute::emissiveDoubleSided);

	ADD_ATTRIBUTE(Attribute::transparencyLevel);
	ADD_ATTRIBUTE(Attribute::transparency3Components);
	ADD_ATTRIBUTE(Attribute::useTransparency3Components);

	ADD_ATTRIBUTE(Attribute::displacementMap);

	ADD_ATTRIBUTE(Attribute::displacementBoundary);
	ADD_ATTRIBUTE(Attribute::normalMap);
	ADD_ATTRIBUTE(Attribute::normalMapEnable);
	ADD_ATTRIBUTE(Attribute::transparencyEnable);
	ADD_ATTRIBUTE(Attribute::displacementEnable);
	ADD_ATTRIBUTE(Attribute::displacementMin);
	ADD_ATTRIBUTE(Attribute::displacementMax);
	ADD_ATTRIBUTE(Attribute::displacementSubdiv);
	ADD_ATTRIBUTE(Attribute::displacementCreaseWeight);
	ADD_ATTRIBUTE(Attribute::displacementEnableAdaptiveSubdiv);
	ADD_ATTRIBUTE(Attribute::displacementASubdivFactor);

	ADD_ATTRIBUTE(Attribute::sssEnable);
	ADD_ATTRIBUTE(Attribute::sssUseDiffuseColor);
	ADD_ATTRIBUTE(Attribute::sssWeight);

	ADD_ATTRIBUTE(Attribute::volumeScatter);
	ADD_ATTRIBUTE(Attribute::subsurfaceRadius);
	ADD_ATTRIBUTE(Attribute::volumeScatteringDirection);
	ADD_ATTRIBUTE(Attribute::volumeMultipleScattering);

	ADD_ATTRIBUTE(Attribute::sheenEnabled);
	ADD_ATTRIBUTE(Attribute::sheenWeight);
	ADD_ATTRIBUTE(Attribute::sheenTint);
	ADD_ATTRIBUTE(Attribute::sheenColor);

	return MS::kSuccess;
}

MStatus FireMaya::StandardMaterial::compute(const MPlug& plug, MDataBlock& block)
{
	if ((plug == Attribute::output) || (plug.parent() == Attribute::output))
	{
		// We need to get all attributes which affect outputs in order to recalculate all dependent nodes
		// It needs to get IPR properly updating while changing attributes on the "left" nodes in dependency graph
		ForceEvaluateAllAttributes(true);

		MFloatVector& surfaceColor = block.inputValue(Attribute::diffuseColor).asFloatVector();

		// set output color attribute
		MDataHandle outColorHandle = block.outputValue(Attribute::output);
		MFloatVector& outColor = outColorHandle.asFloatVector();
		outColor = surfaceColor;
		outColorHandle.setClean();
		block.setClean(plug);
	}
	else if ((plug == Attribute::outputAlpha) || (plug.parent() == Attribute::outputAlpha))
	{
		MFloatVector tr(1.0, 1.0, 1.0);

		// set output color attribute
		MDataHandle outTransHandle = block.outputValue(Attribute::outputAlpha);
		MFloatVector& outTrans = outTransHandle.asFloatVector();
		outTrans = tr;
		block.setClean(plug);
	}
	else
		return MS::kUnknownParameter;

	return MS::kSuccess;
}

bool FireMaya::StandardMaterial::IsNormalOrBumpMap(const MObject& attrNormal, NormalMapParams& params) const
{
	assert(params.IsValid());
	if (!params.IsValid())
		return false;

	int type = params.scope.GetValue(params.shaderNode.findPlug(attrNormal, false)).GetNodeType();

	if (type == frw::ValueTypeNormalMap)
		return true;

	if (type == frw::ValueTypeBumpMap)
		return true;

	return false;
}

void FireMaya::StandardMaterial::ApplyNormalMap(NormalMapParams& params)
{
	assert(params.IsValid());
	if (!params.IsValid())
		return;

	MObject normalMapEnable = Attribute::normalMapEnable;
	MObject attrNormal = Attribute::normalMap;

	if (params.shaderNode.findPlug(params.attrUseCommonNormalMap, false).asBool())
	{
		if (params.shaderNode.findPlug(normalMapEnable, false).asBool() && IsNormalOrBumpMap(attrNormal, params))
		{
			frw::Value normalValue = params.scope.GetValue(params.shaderNode.findPlug(attrNormal, false));
			params.material.xSetValue(params.param, normalValue);
		}
	}
	else if (IsNormalOrBumpMap(params.mapPlug, params))
	{
		frw::Value normalValue = params.scope.GetValue(params.shaderNode.findPlug(params.mapPlug, false));
		params.material.xSetValue(params.param, normalValue);
	}
}

FireMaya::StandardMaterial::NormalMapParams::NormalMapParams(Scope& _scope, frw::Shader& _material, MFnDependencyNode& _shaderNode)
	: scope(_scope)
	, material(_material)
	, param(MATERIAL_INVALID_PARAM)
	, shaderNode(_shaderNode)
	, attrUseCommonNormalMap()
	, mapPlug()
{}

bool FireMaya::StandardMaterial::NormalMapParams::IsValid(void) const
{
	return !attrUseCommonNormalMap.isNull() && !mapPlug.isNull() && (param != MATERIAL_INVALID_PARAM);
}

frw::Shader FireMaya::StandardMaterial::GetShader(Scope& scope)
{
#if USE_RPRX
	frw::Shader material(scope.MaterialSystem(), scope.Context());
	MFnDependencyNode shaderNode(thisMObject());

#define GET_BOOL(_attrib_) \
	shaderNode.findPlug(Attribute::_attrib_, false).asBool()

#define GET_VALUE(_attrib_) \
	scope.GetValue(shaderNode.findPlug(Attribute::_attrib_, false))

#define GET_TYPE(_attrib_) \
	GET_VALUE(_attrib_).GetNodeType()

#define SET_RPRX_VALUE(_param_, _attrib_) \
	material.xSetValue(_param_, GET_VALUE(_attrib_));

	const IFireRenderContextInfo* ctxInfo = scope.GetIContextInfo();
	assert(ctxInfo);

	MDistance::Unit sceneUnits = MDistance::uiUnit();
	MDistance distance(1.0, sceneUnits);
	float scale_multiplier = (float) distance.asMeters();
	// Diffuse
	if (GET_BOOL(diffuseEnable))
	{
		// apply purple 1x1 image in case we missed a texture file
		frw::Value value = scope.GetValueForDiffuseColor(shaderNode.findPlug(Attribute::diffuseColor, false));
		material.xSetValue(RPR_MATERIAL_INPUT_UBER_DIFFUSE_COLOR, value);
		
		SET_RPRX_VALUE(RPR_MATERIAL_INPUT_UBER_DIFFUSE_WEIGHT, diffuseWeight);
		SET_RPRX_VALUE(RPR_MATERIAL_INPUT_UBER_DIFFUSE_ROUGHNESS, diffuseRoughness);

		NormalMapParams params(scope, material, shaderNode);
		params.attrUseCommonNormalMap = Attribute::useShaderNormal;
		params.mapPlug = Attribute::diffuseNormal;
		params.param = RPR_MATERIAL_INPUT_UBER_DIFFUSE_NORMAL;
		ApplyNormalMap(params);

		if (ctxInfo->IsUberBackscatterWeightSupported())
		{
			SET_RPRX_VALUE(RPR_MATERIAL_INPUT_UBER_BACKSCATTER_WEIGHT, backscatteringWeight);
		}

		if (GET_BOOL(separateBackscatterColor))
			SET_RPRX_VALUE(RPR_MATERIAL_INPUT_UBER_BACKSCATTER_COLOR, backscatteringColor);
	}
	else
	{
		material.xSetParameterF(RPR_MATERIAL_INPUT_UBER_DIFFUSE_WEIGHT, 0, 0, 0, 0);
	}

	// Reflection
	bool isUberReflectionDielectricSupported = ctxInfo->IsUberReflectionDielectricSupported();
	if (GET_BOOL(reflectionEnable))
	{
		SET_RPRX_VALUE(RPR_MATERIAL_INPUT_UBER_REFLECTION_COLOR, reflectionColor);
		SET_RPRX_VALUE(RPR_MATERIAL_INPUT_UBER_REFLECTION_WEIGHT, reflectionWeight);
		SET_RPRX_VALUE(RPR_MATERIAL_INPUT_UBER_REFLECTION_ROUGHNESS, reflectionRoughness);
		SET_RPRX_VALUE(RPR_MATERIAL_INPUT_UBER_REFLECTION_ANISOTROPY, reflectionAnisotropy);
		SET_RPRX_VALUE(RPR_MATERIAL_INPUT_UBER_REFLECTION_ANISOTROPY_ROTATION, reflectionAnisotropyRotation);
		// Metalness
		if (GET_BOOL(reflectionMetalMaterial))
		{
			// metallic material
			material.xSetParameterU(RPR_MATERIAL_INPUT_UBER_REFLECTION_MODE, RPR_UBER_MATERIAL_IOR_MODE_METALNESS);
			SET_RPRX_VALUE(RPR_MATERIAL_INPUT_UBER_REFLECTION_METALNESS, reflectionMetalness);
		}
		else
		{
			// PBR material
			material.xSetParameterU(RPR_MATERIAL_INPUT_UBER_REFLECTION_MODE, RPR_UBER_MATERIAL_IOR_MODE_PBR);
			SET_RPRX_VALUE(RPR_MATERIAL_INPUT_UBER_REFLECTION_IOR, reflectionIOR);
		}

		// Is Frensel Approximation On
		if (ctxInfo->IsUberShlickApproximationSupported())
		{
			SET_RPRX_VALUE(RPR_MATERIAL_INPUT_UBER_FRESNEL_SCHLICK_APPROXIMATION, reflectionIsFresnelApproximationOn);
		}

		if (ctxInfo->IsUberReflectionNormalSupported())
		{
			NormalMapParams params(scope, material, shaderNode);
			params.attrUseCommonNormalMap = Attribute::reflectUseShaderNormal;
			params.mapPlug = Attribute::reflectNormal;
			params.param = RPR_MATERIAL_INPUT_UBER_REFLECTION_NORMAL;
			ApplyNormalMap(params);
		}

		// check for the external attribute. Its needed for Demo preparation for Hybrid DX12 engine
		MPlug plug = shaderNode.findPlug("dx12_R0", false);

		if (isUberReflectionDielectricSupported)
		{
			if (!plug.isNull())
			{
				material.xSetValue(RPR_MATERIAL_INPUT_UBER_REFLECTION_DIELECTRIC_REFLECTANCE, scope.GetValue(plug));
			}
			else
			{
				material.xSetValue(RPR_MATERIAL_INPUT_UBER_REFLECTION_DIELECTRIC_REFLECTANCE, frw::Value(0.5f));
			}
		}
	}
	else
	{
		if (isUberReflectionDielectricSupported)
		{
			material.xSetValue(RPR_MATERIAL_INPUT_UBER_REFLECTION_DIELECTRIC_REFLECTANCE, frw::Value(0.0f));
		}
		material.xSetParameterF(RPR_MATERIAL_INPUT_UBER_REFLECTION_WEIGHT, 0, 0, 0, 0);
	}

	// Coating
	if (GET_BOOL(clearCoatEnable))
	{
		SET_RPRX_VALUE(RPR_MATERIAL_INPUT_UBER_COATING_COLOR, clearCoatColor);
		SET_RPRX_VALUE(RPR_MATERIAL_INPUT_UBER_COATING_WEIGHT, clearCoatWeight);
		SET_RPRX_VALUE(RPR_MATERIAL_INPUT_UBER_COATING_ROUGHNESS, clearCoatRoughness);

		// Metalness <= removed; Coating should always be PBR mode rather than metalness

		material.xSetParameterU(RPR_MATERIAL_INPUT_UBER_COATING_MODE, RPR_UBER_MATERIAL_IOR_MODE_PBR);
		SET_RPRX_VALUE(RPR_MATERIAL_INPUT_UBER_COATING_IOR, clearCoatIOR);

		NormalMapParams params(scope, material, shaderNode);
		params.attrUseCommonNormalMap = Attribute::coatUseShaderNormal;
		params.mapPlug = Attribute::coatNormal;
		params.param = RPR_MATERIAL_INPUT_UBER_COATING_NORMAL;
		ApplyNormalMap(params);

		if (ctxInfo->IsUberCoatingThicknessSupported())
		{
			SET_RPRX_VALUE(RPR_MATERIAL_INPUT_UBER_COATING_THICKNESS, clearCoatThickness);
		}
		if (ctxInfo->IsUberCoatingTransmissionColorSupported())
		{
			frw::Value clearCoatTransmissionColorInverse = frw::Value(1) - GET_VALUE(clearCoatTransmissionColor);
			material.xSetValue(RPR_MATERIAL_INPUT_UBER_COATING_TRANSMISSION_COLOR, clearCoatTransmissionColorInverse);
		}
	}
	else
	{
		material.xSetParameterF(RPR_MATERIAL_INPUT_UBER_COATING_WEIGHT, 0, 0, 0, 0);
	}

	// Refraction
	if (GET_BOOL(refractionEnable))
	{
		SET_RPRX_VALUE(RPR_MATERIAL_INPUT_UBER_REFRACTION_COLOR, refractionColor);
		SET_RPRX_VALUE(RPR_MATERIAL_INPUT_UBER_REFRACTION_WEIGHT, refractionWeight);
		SET_RPRX_VALUE(RPR_MATERIAL_INPUT_UBER_REFRACTION_ROUGHNESS, refractionRoughness);
		SET_RPRX_VALUE(RPR_MATERIAL_INPUT_UBER_REFRACTION_IOR, refractionIOR);
		if (ctxInfo->IsUberRefractionAbsorbtionColorSupported())
		{
			SET_RPRX_VALUE(RPR_MATERIAL_INPUT_UBER_REFRACTION_ABSORPTION_COLOR, refractionAbsorptionColor);
		}
		if (ctxInfo->IsUberRefractionAbsorbtionDistanceSupported())
		{
			frw::Value valueRefractionAbsorptionDistance = GET_VALUE(refractionAbsorptionDistance) * scale_multiplier;
			material.xSetValue(RPR_MATERIAL_INPUT_UBER_REFRACTION_ABSORPTION_DISTANCE, valueRefractionAbsorptionDistance);
		}

		if (ctxInfo->IsUberRefractionCausticsSupported())
		{
			bool doAllowCaustics = GET_BOOL(refractionAllowCaustics);
			material.xSetParameterU(RPR_MATERIAL_INPUT_UBER_REFRACTION_CAUSTICS, doAllowCaustics ? RPR_TRUE : RPR_FALSE);
		}
		bool bThinSurface = GET_BOOL(refractionThinSurface);
		bool bLinkedIOR = GET_BOOL(refractionLinkToReflection);
		material.xSetParameterU(RPR_MATERIAL_INPUT_UBER_REFRACTION_THIN_SURFACE, bThinSurface ? RPR_TRUE : RPR_FALSE);

		NormalMapParams params(scope, material, shaderNode);
		params.attrUseCommonNormalMap = Attribute::refractionUseShaderNormal;
		params.mapPlug = Attribute::refractionNormal;
		params.param = RPR_MATERIAL_INPUT_UBER_REFRACTION_NORMAL;
		ApplyNormalMap(params);
	}
	else
	{
		material.xSetParameterF(RPR_MATERIAL_INPUT_UBER_REFRACTION_WEIGHT, 0, 0, 0, 0);
	}

	// Emissive
	if (GET_BOOL(emissiveEnable))
	{
		frw::Value valueEmissiveWeight = scope.GetValue(shaderNode.findPlug(Attribute::emissiveWeight, false));
		material.xSetValue(RPR_MATERIAL_INPUT_UBER_EMISSION_WEIGHT, valueEmissiveWeight);

		frw::Value valueEmissiveColor = scope.GetValue(shaderNode.findPlug(Attribute::emissiveColor, false));

		frw::Value valueEmissiveIntensity = scope.GetValue(shaderNode.findPlug(Attribute::emissiveIntensity, false));

		const frw::MaterialSystem ms = scope.MaterialSystem();
		valueEmissiveColor = ms.ValueMul(valueEmissiveColor, valueEmissiveIntensity);		
		material.xSetValue(RPR_MATERIAL_INPUT_UBER_EMISSION_COLOR, valueEmissiveColor);

		bool bDoubleSided = GET_BOOL(emissiveDoubleSided);
		material.xSetParameterU(RPR_MATERIAL_INPUT_UBER_EMISSION_MODE, bDoubleSided ? RPR_UBER_MATERIAL_EMISSION_MODE_DOUBLESIDED : RPR_UBER_MATERIAL_EMISSION_MODE_SINGLESIDED);
	}
	else
	{
		material.xSetParameterF(RPR_MATERIAL_INPUT_UBER_EMISSION_WEIGHT, 0, 0, 0, 0);
	}

	// Subsurface
	if (ctxInfo->IsUberSSSWeightSupported())
	{
		if (GET_BOOL(sssEnable))
		{
			SET_RPRX_VALUE(RPR_MATERIAL_INPUT_UBER_SSS_WEIGHT, sssWeight);

			if (GET_BOOL(sssUseDiffuseColor))
			{
				SET_RPRX_VALUE(RPR_MATERIAL_INPUT_UBER_SSS_SCATTER_COLOR, diffuseColor);
			}
			else
			{
				SET_RPRX_VALUE(RPR_MATERIAL_INPUT_UBER_SSS_SCATTER_COLOR, volumeScatter);
			}

			// SET_RPRX_VALUE(RPR_MATERIAL_INPUT_UBER_SSS_WEIGHT, sssWeight);

			frw::Value valueSubsurfaceRadius = GET_VALUE(subsurfaceRadius) * scale_multiplier;
			material.xSetValue(RPR_MATERIAL_INPUT_UBER_SSS_SCATTER_DISTANCE, valueSubsurfaceRadius);
			SET_RPRX_VALUE(RPR_MATERIAL_INPUT_UBER_SSS_SCATTER_DIRECTION, volumeScatteringDirection);
			material.xSetParameterU(RPR_MATERIAL_INPUT_UBER_SSS_MULTISCATTER, GET_BOOL(volumeMultipleScattering) ? RPR_TRUE : RPR_FALSE);
		}
		else
		{
			material.xSetParameterF(RPR_MATERIAL_INPUT_UBER_SSS_WEIGHT, 0, 0, 0, 0);
		}
	}

	// Sheen
	if (ctxInfo->IsUberSheenWeightSupported())
	{
		if (GET_BOOL(sheenEnabled))
		{
			SET_RPRX_VALUE(RPR_MATERIAL_INPUT_UBER_SHEEN_WEIGHT, sheenWeight);
			SET_RPRX_VALUE(RPR_MATERIAL_INPUT_UBER_SHEEN, sheenColor);
			SET_RPRX_VALUE(RPR_MATERIAL_INPUT_UBER_SHEEN_TINT, sheenTint);
		}
		else
		{
			material.xSetParameterF(RPR_MATERIAL_INPUT_UBER_SHEEN_WEIGHT, 0, 0, 0, 0);
		}
	}

	// Material attributes
	if (GET_BOOL(transparencyEnable))
	{
		if (GET_BOOL(useTransparency3Components))
		{
			frw::Value value = GET_VALUE(transparency3Components);

			material.xSetValue(RPR_MATERIAL_INPUT_UBER_TRANSPARENCY, value.SelectX());
		}
		else
		{
			SET_RPRX_VALUE(RPR_MATERIAL_INPUT_UBER_TRANSPARENCY, transparencyLevel);
		}
	}

	if (GET_BOOL(normalMapEnable))
	{
		frw::Value value = GET_VALUE(normalMap);
		int type = value.GetNodeType();
		if (type == frw::ValueTypeNormalMap || type == frw::ValueTypeBumpMap)
		{

		}
		else if (type >= 0)
		{
			ErrorPrint("%s NormalMap: invalid node type %d\n", shaderNode.name().asChar(), value.GetNodeType());
		}
	}
#endif
	return material;
}

frw::Shader FireMaya::StandardMaterial::GetVolumeShader(Scope& scope)
{
#if !USE_RPRX
	auto ms = scope.MaterialSystem();
	MFnDependencyNode shaderNode(thisMObject());

	if (scope.GetValue(shaderNode.findPlug(Attribute::volumeEnable)).GetBool())
	{
		frw::Shader material = frw::Shader(ms, frw::ShaderTypeVolume);
		auto scatterColor = scope.GetValue(shaderNode.findPlug(Attribute::volumeScatter));
		auto transmissionColor = scope.GetValue(shaderNode.findPlug(Attribute::volumeTransmission));
		auto emissionColor = scope.GetValue(shaderNode.findPlug(Attribute::volumeEmission));
		auto k = shaderNode.findPlug(Attribute::volumeDensity).asFloat();
		auto scatteringDirection = shaderNode.findPlug(Attribute::volumeScatteringDirection).asFloat();
		auto multiScatter = shaderNode.findPlug(Attribute::volumeMultipleScattering).asBool();

		// scattering
		material.SetValue("sigmas", scatterColor * k);

		// absorption
		material.SetValue("sigmaa", (1 - transmissionColor) * k);

		// emission
		material.SetValue("emission", emissionColor * k);

		// phase and multi on/off
		material.SetValue("g", scatteringDirection);
		material.SetValue("multiscatter", multiScatter ? 1.f : 0.f);
		return material;
	}
#endif
	return frw::Shader();
}

MObject FireMaya::StandardMaterial::GetDisplacementNode()
{
	MFnDependencyNode shaderNode(thisMObject());

	if (!GET_BOOL(displacementEnable))
		return MObject::kNullObj;

	MPlug plug = shaderNode.findPlug(Attribute::displacementMap, false);
	if (plug.isNull())
		return MObject::kNullObj;

	MPlugArray shaderConnections;
	plug.connectedTo(shaderConnections, true, false);
	if (shaderConnections.length() == 0)
		return MObject::kNullObj;
	MObject node = shaderConnections[0].node();
	return node;
}
