#include "SubsurfaceMaterial.h"

#include <maya/MFnNumericAttribute.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MDataHandle.h>
#include <maya/MFloatVector.h>

#include "FireMaya.h"

namespace
{
	namespace Attribute
	{
		MObject surfaceColor;
		MObject surfaceIntensity;

		MObject subsurfaceColor;
		MObject scatterColor;
		MObject scatterAmount;
		MObject density;
		MObject emissionColor;

		MObject scatteringDirection;
		MObject multiScattering;

		MObject	output;

		namespace ShortName
		{
			const auto surfaceColor = "surfc";
			const auto surfaceIntensity = "surfi";

			const auto subsurfaceColor = "c";
			const auto scatterColor = "scatc";
			const auto scatterAmount = "scata";
			const auto emissionColor = "ec";

			const auto density = "d";

			const auto scatteringDirection = "dir";
			const auto multiScattering = "multi";

			const auto output = "oc";
		}
	}
}

// Attributes

void FireMaya::SubsurfaceMaterial::postConstructor()
{
	setMPSafe(true);
}


// creates an instance of the node
void* FireMaya::SubsurfaceMaterial::creator()
{
	return new SubsurfaceMaterial;
}

// initializes attribute information
// call by MAYA when this plug-in was loaded.
//
MStatus FireMaya::SubsurfaceMaterial::initialize()
{
	MFnNumericAttribute nAttr;
	MFnEnumAttribute eAttr;

	// output color
	Attribute::output = nAttr.createColor("outColor", Attribute::ShortName::output);
	MAKE_OUTPUT(nAttr);
	CHECK_MSTATUS(addAttribute(Attribute::output));

	Attribute::surfaceColor = nAttr.createColor("surfaceColor", Attribute::ShortName::surfaceColor);
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(1.0f, 1.0f, 1.0f));
	CHECK_MSTATUS(addAttribute(Attribute::surfaceColor));
	CHECK_MSTATUS(attributeAffects(Attribute::surfaceColor, Attribute::output));

	Attribute::surfaceIntensity = nAttr.create("surfaceIntensity", Attribute::ShortName::surfaceIntensity, MFnNumericData::kFloat, 0.5);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0.0);
	nAttr.setMax(1.f);
	CHECK_MSTATUS(addAttribute(Attribute::surfaceIntensity));
	CHECK_MSTATUS(attributeAffects(Attribute::surfaceIntensity, Attribute::output));

	Attribute::subsurfaceColor = nAttr.createColor("subsurfaceColor", Attribute::ShortName::subsurfaceColor);
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(1.0f, 1.0f, 1.0f));
	CHECK_MSTATUS(addAttribute(Attribute::subsurfaceColor));
	CHECK_MSTATUS(attributeAffects(Attribute::subsurfaceColor, Attribute::output));

	Attribute::density = nAttr.create("density", Attribute::ShortName::density, MFnNumericData::kFloat, 1.);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0.0);
	nAttr.setSoftMax(100.f);
	CHECK_MSTATUS(addAttribute(Attribute::density));
	CHECK_MSTATUS(attributeAffects(Attribute::density, Attribute::output));


	Attribute::scatterColor = nAttr.createColor("scatterColor", Attribute::ShortName::scatterColor);
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(1.0f, 1.0f, 1.0f));
	CHECK_MSTATUS(addAttribute(Attribute::scatterColor));
	CHECK_MSTATUS(attributeAffects(Attribute::scatterColor, Attribute::output));

	Attribute::scatterAmount = nAttr.create("scatterAmount", Attribute::ShortName::scatterAmount, MFnNumericData::kFloat, 0.5);
	MAKE_INPUT(nAttr);
	nAttr.setMin(0.0);
	nAttr.setSoftMax(1.f);
	CHECK_MSTATUS(addAttribute(Attribute::scatterAmount));
	CHECK_MSTATUS(attributeAffects(Attribute::scatterAmount, Attribute::output));

	Attribute::emissionColor = nAttr.createColor("emissionColor", Attribute::ShortName::emissionColor);
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(0.0f, 0.0f, 0.0f));
	CHECK_MSTATUS(addAttribute(Attribute::emissionColor));
	CHECK_MSTATUS(attributeAffects(Attribute::emissionColor, Attribute::output));


	Attribute::scatteringDirection = nAttr.create("scatteringDirection", Attribute::ShortName::scatteringDirection, MFnNumericData::kFloat, 0.0);
	MAKE_INPUT(nAttr);
	nAttr.setMin(-1.0f);
	nAttr.setMax(1.0f);
	CHECK_MSTATUS(addAttribute(Attribute::scatteringDirection));
	CHECK_MSTATUS(attributeAffects(Attribute::scatteringDirection, Attribute::output));

	Attribute::multiScattering = nAttr.create("multiscatter", Attribute::ShortName::multiScattering, MFnNumericData::kBoolean, true);
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(addAttribute(Attribute::multiScattering));
	CHECK_MSTATUS(attributeAffects(Attribute::multiScattering, Attribute::output));

	return MS::kSuccess;
}

MStatus FireMaya::SubsurfaceMaterial::compute(const MPlug& plug, MDataBlock& block)
{
	if ((plug == Attribute::output) || (plug.parent() == Attribute::output))
	{
		MFloatVector& surfaceColor = block.inputValue(Attribute::subsurfaceColor).asFloatVector();

		// set color attribute
		MDataHandle outColorHandle = block.outputValue(Attribute::output);
		MFloatVector& outColor = outColorHandle.asFloatVector();
		outColor = surfaceColor;
		outColorHandle.setClean();
		block.setClean(plug);
	}
	else
		return MS::kUnknownParameter;

	return MS::kSuccess;
}

frw::Shader FireMaya::SubsurfaceMaterial::GetShader(Scope& scope)
{
	auto ms = scope.MaterialSystem();
	MFnDependencyNode shaderNode(thisMObject());

	auto k = shaderNode.findPlug(Attribute::surfaceIntensity).asFloat();
	if (k <= 0)
		return frw::TransparentShader(ms);

	auto surfacePlug = shaderNode.findPlug(Attribute::surfaceColor);
	auto shader = scope.GetShader(surfacePlug);

	if (!shader)
	{
		auto color = scope.GetValue(surfacePlug);
		frw::DiffuseShader diffuse(ms);
		diffuse.SetColor(color);
		shader = diffuse;
	}

	if (k < 1)	// blend it with nothing
	{
		auto transparent = frw::TransparentShader(ms);

		shader = ms.ShaderBlend(transparent, shader, k);
	}

	return shader;
}

frw::Shader FireMaya::SubsurfaceMaterial::GetVolumeShader(Scope& scope)
{
	auto ms = scope.MaterialSystem();
	MFnDependencyNode shaderNode(thisMObject());

	frw::Shader material = frw::Shader(ms, frw::ShaderTypeVolume);

	auto scatterColor = scope.GetValue(shaderNode.findPlug(Attribute::scatterColor));
	auto scatterAmount = scope.GetValue(shaderNode.findPlug(Attribute::scatterAmount));
	auto subsurfaceColor = scope.GetValue(shaderNode.findPlug(Attribute::subsurfaceColor));
	auto emissionColor = scope.GetValue(shaderNode.findPlug(Attribute::emissionColor));
	auto k = shaderNode.findPlug(Attribute::density).asFloat();
	auto scatteringDirection = shaderNode.findPlug(Attribute::scatteringDirection).asFloat();
	auto multiScatter = shaderNode.findPlug(Attribute::multiScattering).asBool();

	// auto k = radius ? 1.0 / radius : 1000;	// avoid DBZ
	// scattering
	material.SetValue("sigmas", scatterAmount * k * scatterColor);

	// absorption
	material.SetValue("sigmaa", (1 - subsurfaceColor) * k);

	// emission
	material.SetValue("emission", emissionColor * k);

	// phase and multi on/off
	material.SetValue("g", scatteringDirection);
	material.SetValue("multiscatter", multiScatter ? 1.f : 0.f);

	return material;
}
