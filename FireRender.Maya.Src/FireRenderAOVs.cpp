#include "frWrap.h"

#include "FireRenderAOVs.h"
#include "FireRenderGlobals.h"
#include "FireRenderContext.h"
#include "FireRenderImageUtil.h"
#include "FireMaya.h"

#include <maya/MStatus.h>
#include <maya/MFnEnumAttribute.h>
#include <wchar.h>
using namespace OIIO;


// Life Cycle
// -----------------------------------------------------------------------------
FireRenderAOVs::FireRenderAOVs() :
	m_renderViewAOVId(RPR_AOV_COLOR)
{
	// Initialize and add AOV types.
	m_aovs.push_back(FireRenderAOV(RPR_AOV_COLOR, "aovColor", "Color", "color",
				{ { "R", "G", "B", "A" },{ TypeDesc::FLOAT, TypeDesc::VEC4, TypeDesc::COLOR } }));

	m_aovs.push_back(FireRenderAOV(RPR_AOV_OPACITY, "aovOpacity", "Opacity", "opacity",
				{ { "R", "G", "B" },{ TypeDesc::FLOAT, TypeDesc::VEC3, TypeDesc::COLOR } }));

	m_aovs.push_back(FireRenderAOV(RPR_AOV_WORLD_COORDINATE, "aovWorldCoordinate", "World Coordinate", "world_coordinate",
				{ { "X", "Y", "Z" },{ TypeDesc::FLOAT, TypeDesc::VEC3, TypeDesc::POINT } }));

	m_aovs.push_back(FireRenderAOV(RPR_AOV_UV, "aovUV", "UV", "uv",
				{ { "U", "V", "W" },{ TypeDesc::FLOAT, TypeDesc::VEC3, TypeDesc::POINT } }));

	m_aovs.push_back(FireRenderAOV(RPR_AOV_MATERIAL_IDX, "aovMaterialIndex", "Material Index", "material_index",
				{ { "R", "G", "B" },{ TypeDesc::FLOAT, TypeDesc::VEC3, TypeDesc::COLOR } }));

	m_aovs.push_back(FireRenderAOV(RPR_AOV_GEOMETRIC_NORMAL, "aovGeometricNormal", "Geometric Normal", "geometric_normal",
				{ { "X", "Y", "Z" },{ TypeDesc::FLOAT, TypeDesc::VEC3, TypeDesc::NORMAL } }));

	m_aovs.push_back(FireRenderAOV(RPR_AOV_SHADING_NORMAL, "aovShadingNormal", "Shading Normal", "shading_normal",
				{ { "X", "Y", "Z" },{ TypeDesc::FLOAT, TypeDesc::VEC3, TypeDesc::NORMAL } }));

	m_aovs.push_back(FireRenderAOV(RPR_AOV_DEPTH, "aovDepth", "Depth", "depth",
				{ { "Z" },{ TypeDesc::FLOAT } }));

	m_aovs.push_back(FireRenderAOV(RPR_AOV_OBJECT_ID, "aovObjectId", "Object ID", "object_id",
				{ { "R", "G", "B" },{ TypeDesc::FLOAT, TypeDesc::VEC3, TypeDesc::COLOR } }));

	m_aovs.push_back(FireRenderAOV(RPR_AOV_OBJECT_GROUP_ID, "aovObjectGroupId", "Object Group ID", "object_group_id",
				{ { "R", "G", "B" },{ TypeDesc::FLOAT, TypeDesc::VEC3, TypeDesc::COLOR } }));

	m_aovs.push_back(FireRenderAOV(RPR_AOV_SHADOW_CATCHER, "aovShadowCatcher", "Shadow", "shadow",
				{ { "R", "G", "B" },{ TypeDesc::FLOAT, TypeDesc::VEC3, TypeDesc::COLOR } }));

	m_aovs.push_back(FireRenderAOV(RPR_AOV_BACKGROUND, "aovBackground", "Background", "background",
	{ { "R", "G", "B" },{ TypeDesc::FLOAT, TypeDesc::VEC3, TypeDesc::COLOR } }));

}

// -----------------------------------------------------------------------------
FireRenderAOVs::~FireRenderAOVs()
{
	freePixels();
}


// Public Methods
// -----------------------------------------------------------------------------
FireRenderAOV& FireRenderAOVs::getAOV(unsigned int id)
{
	return m_aovs[id];
}

// -----------------------------------------------------------------------------
FireRenderAOV& FireRenderAOVs::getRenderViewAOV()
{
	return getAOV(m_renderViewAOVId);
}

// -----------------------------------------------------------------------------
unsigned int FireRenderAOVs::getActiveAOVCount()
{
	unsigned int count = 0;

	for (auto& aov : m_aovs)
		if (aov.active)
			count++;

	return count;
}

// -----------------------------------------------------------------------------
void FireRenderAOVs::registerAttributes()
{
	MStatus status;
	MFnNumericAttribute nAttr;
	MFnEnumAttribute eAttr;

	MObject displayedInRenderView = eAttr.create("aovDisplayedInRenderView", "aovdiv", 0, &status);

	for (auto& aov : m_aovs)
	{
		eAttr.addField(aov.name, aov.id);

		MObject aovAttribute = nAttr.create(aov.attribute, aov.attribute, MFnNumericData::kBoolean, 0, &status);
		MAKE_INPUT(nAttr);
		status = FireRenderGlobals::addAttribute(aovAttribute);
		CHECK_MSTATUS(status);
	}

	MAKE_INPUT(eAttr);
	CHECK_MSTATUS(FireRenderGlobals::addAttribute(displayedInRenderView));
}

// -----------------------------------------------------------------------------
void FireRenderAOVs::readFromGlobals(const MFnDependencyNode& globals)
{
	// Read render view AOV.
	MPlug plug = globals.findPlug("aovDisplayedInRenderView");
	if (!plug.isNull())
		m_renderViewAOVId = plug.asShort();

	// Read AOV active states.
	for (auto& aov : m_aovs)
	{
		// Colour and render view AOVs are always active.
		if (aov.id == RPR_AOV_COLOR || aov.id == RPR_AOV_OPACITY || aov.id == m_renderViewAOVId)
		{
			aov.active = true;
			continue;
		}

		// Activate user selected AOVs.
		MPlug uplug = globals.findPlug(aov.attribute);
		if (!uplug.isNull())
			aov.active = uplug.asBool();
	}
}

// -----------------------------------------------------------------------------
void FireRenderAOVs::applyToContext(FireRenderContext& context)
{
	for (auto& aov : m_aovs)
	{
		if (aov.active)
			context.enableAOV(aov.id);
	}
}

// -----------------------------------------------------------------------------
void FireRenderAOVs::setRegion(const RenderRegion& region,
	unsigned int frameWidth, unsigned int frameHeight)
{
	m_region = region;

	for (auto& aov : m_aovs)
		aov.setRegion(region, frameWidth, frameHeight);
}

// -----------------------------------------------------------------------------
void FireRenderAOVs::allocatePixels()
{
	for (auto& aov : m_aovs)
		aov.allocatePixels();
}

// -----------------------------------------------------------------------------
void FireRenderAOVs::freePixels()
{
	for (auto& aov : m_aovs)
		aov.freePixels();
}

void FireRenderAOVs::setRenderStamp(const MString& renderStamp)
{
	for (auto& aov : m_aovs)
		aov.setRenderStamp(renderStamp);
}

// -----------------------------------------------------------------------------
void FireRenderAOVs::readFrameBuffers(FireRenderContext& context, bool flip)
{
	for (auto& aov : m_aovs)
		aov.readFrameBuffer(context, flip);
}

// -----------------------------------------------------------------------------
void FireRenderAOVs::writeToFile(const MString& filePath, unsigned int imageFormat)
{
	// Check if only the color AOV is active.
	bool colorOnly = getActiveAOVCount() == 1;

	// For EXR, save all AOVs to a single multichannel file.
	MString extension = FireRenderImageUtil::getImageFormatExtension(imageFormat);
	if (extension == "exr" )
	{
		FireRenderImageUtil::saveMultichannelAOVs(filePath,
			m_region.getWidth(), m_region.getHeight(),imageFormat, *this);
	}

	// Otherwise, write active AOVs to individual files.
	else
	{
		for (auto& aov : m_aovs)
			aov.writeToFile(filePath, colorOnly, imageFormat);
	}
}

int FireRenderAOVs::getNumberOfAOVs() {
	return (int)m_aovs.size();
}