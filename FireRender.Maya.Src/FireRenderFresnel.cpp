#include "FireRenderFresnel.h"
#include <maya/MFnNumericAttribute.h>

#include "FireMaya.h"


namespace
{
	namespace Attribute
	{
		MObject ior;
		MObject normal;
		MObject inVec;
		MObject	output;
	}
}

void* FireMaya::Fresnel::creator()
{
	return new Fresnel;
}

MStatus FireMaya::Fresnel::initialize()
{
	MFnNumericAttribute nAttr;
	Attribute::ior = nAttr.create("ior", "ior", MFnNumericData::kFloat, 1.5);
	MAKE_INPUT(nAttr);
	nAttr.setSoftMin(1.0);
	nAttr.setSoftMax(5.0);

	Attribute::normal = nAttr.create("normalMap", "nm", MFnNumericData::k3Float, 0.0);
	MAKE_INPUT(nAttr);

	Attribute::inVec = nAttr.create("inVec", "inVec", MFnNumericData::k3Float, 0.0);
	MAKE_INPUT(nAttr);

	Attribute::output = nAttr.create("out", "o", MFnNumericData::kFloat, 0.0);
	MAKE_OUTPUT(nAttr);

	CHECK_MSTATUS(addAttribute(Attribute::ior));
	CHECK_MSTATUS(addAttribute(Attribute::normal));
	CHECK_MSTATUS(addAttribute(Attribute::inVec));
	CHECK_MSTATUS(addAttribute(Attribute::output));

	CHECK_MSTATUS(attributeAffects(Attribute::ior, Attribute::output));
	CHECK_MSTATUS(attributeAffects(Attribute::normal, Attribute::output));
	CHECK_MSTATUS(attributeAffects(Attribute::inVec, Attribute::output));

	return MS::kSuccess;
}

frw::Value FireMaya::Fresnel::GetValue(const Scope& scope) const
{
	MFnDependencyNode shaderNode(thisMObject());

	frw::ValueNode valueNode(scope.MaterialSystem(), frw::ValueTypeFresnel);
	valueNode.SetValue("ior", scope.GetValue(shaderNode.findPlug(Attribute::ior, false)));
	valueNode.SetValue("normal", scope.GetConnectedValue(shaderNode.findPlug(Attribute::normal, false)));
	valueNode.SetValue("invec", scope.GetConnectedValue(shaderNode.findPlug(Attribute::inVec, false)));

	return valueNode;
}


//////////////////////////////////////////////////////////////////////////////
// VIEWPORT 2.0

#include <maya/MFragmentManager.h>
#include <maya/MShaderManager.h>


FireMaya::Fresnel::Override::Override(const MObject& obj) : MPxShadingNodeOverride(obj)
{
	m_shader = obj;
	m_ior = 1.5f;
}

FireMaya::Fresnel::Override::~Override()
{

}

MHWRender::MPxShadingNodeOverride* FireMaya::Fresnel::Override::creator(const MObject& obj)
{
	return new Override(obj);
}

MHWRender::DrawAPI FireMaya::Fresnel::Override::supportedDrawAPIs() const
{
	return MHWRender::DrawAPI::kAllDevices;
}

bool FireMaya::Fresnel::Override::allowConnections() const
{
	return true;
}

MString FireMaya::Fresnel::Override::fragmentName() const
{
	return "mayaSamplerInfo";
}

void FireMaya::Fresnel::Override::getCustomMappings(MHWRender::MAttributeParameterMappingList& mappings)
{
}

bool FireMaya::Fresnel::Override::valueChangeRequiresFragmentRebuild(const MPlug* plug) const
{
	return false;
}

void FireMaya::Fresnel::Override::updateDG()
{
	MFnDependencyNode shaderNode(m_shader);
	MPlug rplug = shaderNode.findPlug("ior", false);
	if (!rplug.isNull())
		rplug.getValue(m_ior);
}

void FireMaya::Fresnel::Override::updateShader(MHWRender::MShaderInstance& shader, const MHWRender::MAttributeParameterMappingList& mappings)
{
}


MString FireMaya::Fresnel::Override::outputForConnection(const MPlug& sourcePlug, const MPlug& destinationPlug)
{
	return "facingRatio";
}


const char* FireMaya::Fresnel::Override::className()
{
	return "RPRFresnelNodeOverride";
}
