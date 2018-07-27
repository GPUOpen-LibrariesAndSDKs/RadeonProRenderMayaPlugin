#include "FireRenderAO.h"
#include "frWrap.h"
#include "FireRenderUtils.h"

#include <maya/MFnEnumAttribute.h>
#include <maya/MColor.h>

namespace FireMaya
{

MObject FireRenderAO::m_radiusAttr;
MObject FireRenderAO::m_sideAttr;
MObject FireRenderAO::m_occludedColorAttr;
MObject FireRenderAO::m_unoccludedColorAttr;
MObject FireRenderAO::m_outputAttr;

MStatus FireRenderAO::initialize()
{
    MFnNumericAttribute nAttr;
    MFnEnumAttribute eAttr;

    m_radiusAttr = nAttr.create("radius", "r", MFnNumericData::kFloat, 0.1);
    MAKE_INPUT(nAttr);
    nAttr.setMin(0.0);
    nAttr.setSoftMax(10.0);

    m_sideAttr = eAttr.create("side", "s", AOMapSideTypeFront);
    eAttr.addField("Front", AOMapSideTypeFront);
    eAttr.addField("Back", AOMapSideTypeBack);
    MAKE_INPUT_CONST(eAttr);

    m_unoccludedColorAttr = nAttr.createColor("unoccludedColor", "ucl");
    nAttr.setDefault(1.0, 1.0, 1.0);
    MAKE_INPUT(nAttr);

    m_occludedColorAttr = nAttr.createColor("occludedColor", "ocl");
    nAttr.setDefault(0.0, 0.0, 0.0);
    MAKE_INPUT(nAttr);

    m_outputAttr = nAttr.createColor("output", "o");
    MAKE_OUTPUT(nAttr);

    CHECK_MSTATUS(addAttribute(m_sideAttr));
    CHECK_MSTATUS(addAttribute(m_radiusAttr));
    CHECK_MSTATUS(addAttribute(m_occludedColorAttr));
    CHECK_MSTATUS(addAttribute(m_unoccludedColorAttr));
    CHECK_MSTATUS(addAttribute(m_outputAttr));

    CHECK_MSTATUS(attributeAffects(m_sideAttr, m_outputAttr));
    CHECK_MSTATUS(attributeAffects(m_radiusAttr, m_outputAttr));
    CHECK_MSTATUS(attributeAffects(m_occludedColorAttr, m_outputAttr));
    CHECK_MSTATUS(attributeAffects(m_unoccludedColorAttr, m_outputAttr));

    return MStatus::kSuccess;
}

void* FireRenderAO::creator()
{
    return new FireRenderAO;
}

frw::Value FireRenderAO::GetValue(Scope& scope)
{
    MFnDependencyNode shaderNode(thisMObject());

    float radius = 0.0f;
    AOMapSideType sideType = AOMapSideTypeFront;

    MPlug plug = shaderNode.findPlug(m_radiusAttr);

    if (!plug.isNull())
    {
        radius = plug.asFloat();
    }

    frw::Value radiusValue(radius, 0.0f, 0.0f, 0.0f);

    plug = shaderNode.findPlug(m_sideAttr);

    if (!plug.isNull())
    {
        sideType = static_cast<AOMapSideType>(plug.asInt());
    }

    frw::Value sideValue(1.0f, 0.0f, 0.0f, 0.0f); // Front type for default

    if (sideType == AOMapSideTypeBack)
    {
        sideValue = frw::Value(-1.0f, 0.0f, 0.0f, 0.0f);
    }

    frw::Value occColor = scope.GetValue(shaderNode.findPlug(m_occludedColorAttr));
    frw::Value unoccColor = scope.GetValue(shaderNode.findPlug(m_unoccludedColorAttr));

    return scope.MaterialSystem().ValueBlend(occColor, unoccColor, frw::AOMapNode(scope.MaterialSystem(), radiusValue, sideValue));
}

} // FireMaya
