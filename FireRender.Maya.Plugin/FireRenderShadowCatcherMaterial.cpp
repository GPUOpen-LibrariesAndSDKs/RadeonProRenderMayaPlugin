#include "FireRenderShadowCatcherMaterial.h"

#include <cassert>

#include <maya/MGlobal.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnNumericData.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFloatVector.h>
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>

#include "FireMaya.h"

namespace
{
	namespace Attribute
	{
		MObject bgIsEnv;
		MObject bgWeight;
		MObject	bgColor;
		MObject refOverride;
		MObject refWeight;
		MObject refColor;
		MObject shadowColor;
		MObject shadowWeight;
		MObject shadowTransparency;
		MObject useNormalMap;
		MObject normalMap;
		MObject useDispMap;
		MObject dispMap;
		MObject output;
		MObject disableSwatch;
		MObject swatchIterations;
	}
}

void* FireMaya::ShadowCatcherMaterial::creator()
{
	return new ShadowCatcherMaterial();
}

MStatus FireMaya::ShadowCatcherMaterial::initialize()
{
	MFnNumericAttribute nAttr;
	MFnTypedAttribute tAttr;

	editorTemplate - l "Background is Environment" - addControl "bgIsEnv";
	editorTemplate - l "Background weight" - addControl "bgWeight";
	editorTemplate - l "Background color" - addControl "bgColor";

	editorTemplate - l "Reflective override" - addControl "refOverride";
	editorTemplate - l "Reflective weight" - addControl "refWeight";
	editorTemplate - l "Reflective color" - addControl "refColor";

	editorTemplate - l "Shadow color" - addControl "shadowColor";
	editorTemplate - l "Shadow weight" - addControl "shadowWeight";
	editorTemplate - l "Shadow transparency" - addControl "shadowTransp";

	editorTemplate - l "Use normal map" - addControl "useNormalMap";
	editorTemplate - l "Normal map" - addControl "normalMap";
	editorTemplate - l "Use displacement map" - addControl "useDispMap";
	editorTemplate - l "Displacement map" - addControl "dispMap";


	Attribute::bgIsEnv = nAttr.create("bgIsEnv", "bgie", MFnNumericData::kBoolean, 0);
	MAKE_INPUT(nAttr);

	Attribute::bgColor = nAttr.createColor("bgColor", "bgc");
	MAKE_INPUT(nAttr);
	nAttr.setDefault(0.0, 0.0, 0.0);

	Attribute::output = nAttr.createColor("outColor", "oc");
	MAKE_OUTPUT(nAttr);

	Attribute::disableSwatch = nAttr.create("disableSwatch", "ds", MFnNumericData::kBoolean, 0);
	MAKE_INPUT(nAttr);

	Attribute::swatchIterations = nAttr.create("swatchIterations", "swi", MFnNumericData::kInt, 4);
	MAKE_INPUT(nAttr);
	nAttr.setMin(1);
	nAttr.setMax(64);

	CHECK_MSTATUS(addAttribute(Attribute::bgIsEnv));
	CHECK_MSTATUS(addAttribute(Attribute::bgColor));
	CHECK_MSTATUS(addAttribute(Attribute::output));

	CHECK_MSTATUS(attributeAffects(Attribute::bgIsEnv, Attribute::output));
	CHECK_MSTATUS(attributeAffects(Attribute::bgColor, Attribute::output));

	CHECK_MSTATUS(addAttribute(Attribute::disableSwatch));
	CHECK_MSTATUS(addAttribute(Attribute::swatchIterations));

	return MS::kSuccess;
}

MStatus FireMaya::ShadowCatcherMaterial::compute(const MPlug& plug, MDataBlock& block)
{
	if ((plug == Attribute::output) || (plug.parent() == Attribute::output))
	{
/*		MFloatVector& topColor = block.inputValue(Attribute::inputA).asFloatVector();
		MFloatVector& baseColor = block.inputValue(Attribute::inputB).asFloatVector();

		// set ouput color attribute
		MDataHandle outColorHandle = block.outputValue(Attribute::output);
		MFloatVector& outColor = outColorHandle.asFloatVector();
		outColor = topColor + baseColor;
		outColorHandle.setClean();
		block.setClean(plug);*/
	}
	else
		return MS::kUnknownParameter;

	return MS::kSuccess;
}

frw::Shader FireMaya::ShadowCatcherMaterial::GetShader(Scope& scope)
{
	MFnDependencyNode shaderNode(thisMObject());

/*	auto a = scope.GetShader(shaderNode.findPlug(Attribute::inputA));
	auto b = scope.GetShader(shaderNode.findPlug(Attribute::inputB));*/

	return frw::Shader(scope.MaterialSystem(), scope.Context(), RPRX_MATERIAL_UBER);
	//return scope.MaterialSystem().;
}