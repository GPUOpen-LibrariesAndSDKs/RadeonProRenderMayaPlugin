#include "frWrap.h"

#include "FireRenderAOVs.h"
#include "FireRenderGlobals.h"
#include "FireRenderContext.h"
#include "FireRenderImageUtil.h"
#include "FireRenderAOVDepth.h"
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
	AddAOV(RPR_AOV_COLOR, "aovColor", "Color", "color", 
		{ { "R", "G", "B", "A" },{ TypeDesc::FLOAT, TypeDesc::VEC4, TypeDesc::COLOR } });

	AddAOV(RPR_AOV_OPACITY, "aovOpacity", "Opacity", "opacity", 
		{ { "R", "G", "B" },{ TypeDesc::FLOAT, TypeDesc::VEC3, TypeDesc::COLOR } });

	AddAOV(RPR_AOV_WORLD_COORDINATE, "aovWorldCoordinate", "World Coordinate", "world_coordinate",
		{ { "X", "Y", "Z" },{ TypeDesc::FLOAT, TypeDesc::VEC3, TypeDesc::POINT } });

	AddAOV(RPR_AOV_UV, "aovUV", "UV", "uv", { { "U", "V", "W" },
		{ TypeDesc::FLOAT, TypeDesc::VEC3, TypeDesc::POINT } });

	AddAOV(RPR_AOV_MATERIAL_IDX, "aovMaterialIndex", "Material Index", "material_index",
		{ { "R", "G", "B" },{ TypeDesc::FLOAT, TypeDesc::VEC3, TypeDesc::COLOR } });

	AddAOV(RPR_AOV_GEOMETRIC_NORMAL, "aovGeometricNormal", "Geometric Normal", "geometric_normal",
		{ { "X", "Y", "Z" },{ TypeDesc::FLOAT, TypeDesc::VEC3, TypeDesc::NORMAL } });

	AddAOV(RPR_AOV_SHADING_NORMAL, "aovShadingNormal", "Shading Normal", "shading_normal",
		{ { "X", "Y", "Z" },{ TypeDesc::FLOAT, TypeDesc::VEC3, TypeDesc::NORMAL } });

	AddAOV(RPR_AOV_DEPTH, "aovDepth", "Depth", "depth", 
		{ { "Z" },{ TypeDesc::FLOAT } });

	AddAOV(RPR_AOV_OBJECT_ID, "aovObjectId", "Object ID", "object_id",
		{ { "R", "G", "B" },{ TypeDesc::FLOAT, TypeDesc::VEC3, TypeDesc::COLOR } });

	AddAOV(RPR_AOV_OBJECT_GROUP_ID, "aovObjectGroupId", "Object Group ID", "object_group_id",
		{ { "R", "G", "B" },{ TypeDesc::FLOAT, TypeDesc::VEC3, TypeDesc::COLOR } });

	AddAOV(RPR_AOV_SHADOW_CATCHER, "aovShadowCatcher", "Shadow", "shadow",
		{ { "R", "G", "B" },{ TypeDesc::FLOAT, TypeDesc::VEC3, TypeDesc::COLOR } });

	AddAOV(RPR_AOV_BACKGROUND, "aovBackground", "Background", "background",
		{ { "R", "G", "B" },{ TypeDesc::FLOAT, TypeDesc::VEC3, TypeDesc::COLOR } });

	AddAOV(RPR_AOV_EMISSION, "aovEmission", "Emission", "emission",
		{ { "R", "G", "B" },{ TypeDesc::FLOAT, TypeDesc::VEC3, TypeDesc::COLOR } });

	AddAOV(RPR_AOV_VELOCITY, "aovVelocity", "Velocity", "velocity",
		{ { "X", "Y", "Z" },{ TypeDesc::FLOAT, TypeDesc::VEC3, TypeDesc::NORMAL } });

	AddAOV(RPR_AOV_DIRECT_ILLUMINATION, "aovDirectIllumination", "Direct Illumination", "direct_illumination",
		{ { "R", "G", "B" },{ TypeDesc::FLOAT, TypeDesc::VEC3, TypeDesc::COLOR } });

	AddAOV(RPR_AOV_INDIRECT_ILLUMINATION, "aovIndirectIllumination", "Indirect Illumination", "indirect_illumination",
		{ { "R", "G", "B" },{ TypeDesc::FLOAT, TypeDesc::VEC3, TypeDesc::COLOR } });

	AddAOV(RPR_AOV_AO, "aovAO", "Ambient Occlusion", "ambient_occlusion",
		{ { "R", "G", "B" },{ TypeDesc::FLOAT, TypeDesc::VEC3, TypeDesc::COLOR } });

	AddAOV(RPR_AOV_DIRECT_DIFFUSE, "aovDiffuseDirect", "Diffuse Direct", "diffuse_direct",
		{ { "R", "G", "B" },{ TypeDesc::FLOAT, TypeDesc::VEC3, TypeDesc::COLOR } });

	AddAOV(RPR_AOV_INDIRECT_DIFFUSE, "aovDiffuseIndirect", "Diffuse Indirect", "diffuse_indirect",
		{ { "R", "G", "B" },{ TypeDesc::FLOAT, TypeDesc::VEC3, TypeDesc::COLOR } });

	AddAOV(RPR_AOV_DIRECT_REFLECT, "aovReflectionDirect", "Reflection Direct", "reflection_direct",
		{ { "R", "G", "B" },{ TypeDesc::FLOAT, TypeDesc::VEC3, TypeDesc::COLOR } });

	AddAOV(RPR_AOV_INDIRECT_REFLECT, "aovReflectionIndirect", "Reflection Indirect", "reflection_indirect",
		{ { "R", "G", "B" },{ TypeDesc::FLOAT, TypeDesc::VEC3, TypeDesc::COLOR } });

	AddAOV(RPR_AOV_REFRACT, "aovRefraction", "Refraction", "refraction",
		{ { "R", "G", "B" },{ TypeDesc::FLOAT, TypeDesc::VEC3, TypeDesc::COLOR } });

	AddAOV(RPR_AOV_VOLUME, "aovVolume", "Volume", "volume",
		{ { "R", "G", "B" },{ TypeDesc::FLOAT, TypeDesc::VEC3, TypeDesc::COLOR } });
}

// -----------------------------------------------------------------------------
FireRenderAOVs::~FireRenderAOVs()
{
	freePixels();
}

void FireRenderAOVs::AddAOV(unsigned int id, 
							const MString& attribute, 
							const MString& name,
							const MString& folder, 
							AOVDescription description)
{
	FireRenderAOV* aov = new FireRenderAOV(id, attribute, name, folder, description);

	m_aovs[id] = std::shared_ptr<FireRenderAOV>(aov);
}

// Public Methods
// -----------------------------------------------------------------------------
FireRenderAOV& FireRenderAOVs::getAOV(unsigned int id)
{
	assert(m_aovs.find(id) != m_aovs.end());

	return *m_aovs[id];
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
	{
		if (aov.second->active)
			count++;
	}

	return count;
}

// -----------------------------------------------------------------------------
void FireRenderAOVs::registerAttributes()
{
	MStatus status;
	MFnNumericAttribute nAttr;
	MFnEnumAttribute eAttr;

	MObject displayedInRenderView = eAttr.create("aovDisplayedInRenderView", "aovdiv", 0, &status);

	for (auto& aovIter : m_aovs)
	{
		std::shared_ptr<FireRenderAOV> aov = aovIter.second;
		eAttr.addField(aov->name, aov->id);

		MObject aovAttribute = nAttr.create(aov->attribute, aov->attribute, MFnNumericData::kBoolean, 0, &status);
		MAKE_INPUT(nAttr);
		status = FireRenderGlobals::addAttribute(aovAttribute);
		CHECK_MSTATUS(status);
	}

	MAKE_INPUT(eAttr);
	CHECK_MSTATUS(FireRenderGlobals::addAttribute(displayedInRenderView));

	// Add Depth tuning Parameters
	MObject depthAutoNormalise = nAttr.create("aovDepthAutoNormalise", "aovDAN", MFnNumericData::kBoolean, 1, &status);
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(FireRenderGlobals::addAttribute(depthAutoNormalise));

	MObject depthAutoInvertOutput = nAttr.create("aovDepthInvertOutput", "aovDIO", MFnNumericData::kBoolean, 1, &status);
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(FireRenderGlobals::addAttribute(depthAutoInvertOutput));

	MObject depthNormaliseMin = nAttr.create("aovDepthNormaliseMin", "aovDNMi", MFnNumericData::kFloat, 1, &status);
	nAttr.setMin(0.0f);
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(FireRenderGlobals::addAttribute(depthNormaliseMin));

	MObject depthNormaliseMax = nAttr.create("aovDepthNormaliseMax", "aovDNMa", MFnNumericData::kFloat, 1, &status);
	nAttr.setMin(0.0f);
	MAKE_INPUT(nAttr);
	CHECK_MSTATUS(FireRenderGlobals::addAttribute(depthNormaliseMax));
}

// -----------------------------------------------------------------------------
void FireRenderAOVs::readFromGlobals(const MFnDependencyNode& globals)
{
	// Read render view AOV.
	MPlug plug = globals.findPlug("aovDisplayedInRenderView");
	if (!plug.isNull())
		m_renderViewAOVId = plug.asShort();

	// Read AOV active states.
	for (auto& aovIter : m_aovs)
	{
		std::shared_ptr<FireRenderAOV> aov = aovIter.second;

		// Colour and render view AOVs are always active.
		if (aov->id == RPR_AOV_COLOR || aov->id == RPR_AOV_OPACITY || aov->id == m_renderViewAOVId)
		{
			aov->active = true;
		}
		else
		{
			// Activate user selected AOVs.
			MPlug uplug = globals.findPlug(aov->attribute);
			if (!uplug.isNull())
				aov->active = uplug.asBool();
		}

		if (aov->active)
		{
			aov->ReadFromGlobals(globals);
		}
	}
}

// -----------------------------------------------------------------------------
void FireRenderAOVs::applyToContext(FireRenderContext& context)
{
	for (auto& aov : m_aovs)
	{
		if (aov.second->active)
			context.enableAOV(aov.second->id);
	}
}

// -----------------------------------------------------------------------------
void FireRenderAOVs::setRegion(const RenderRegion& region,
	unsigned int frameWidth, unsigned int frameHeight)
{
	m_region = region;

	for (auto& aov : m_aovs)
		aov.second->setRegion(region, frameWidth, frameHeight);
}

// -----------------------------------------------------------------------------
void FireRenderAOVs::allocatePixels()
{
	for (auto& aov : m_aovs)
		aov.second->allocatePixels();
}

// -----------------------------------------------------------------------------
void FireRenderAOVs::freePixels()
{
	for (auto& aov : m_aovs)
		aov.second->freePixels();
}

void FireRenderAOVs::setRenderStamp(const MString& renderStamp)
{
	for (auto& aov : m_aovs)
		aov.second->setRenderStamp(renderStamp);
}

// -----------------------------------------------------------------------------
void FireRenderAOVs::readFrameBuffers(FireRenderContext& context, bool flip)
{
	for (auto& aov : m_aovs)
		aov.second->readFrameBuffer(context, flip);
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
			aov.second->writeToFile(filePath, colorOnly, imageFormat);
	}
}

int FireRenderAOVs::getNumberOfAOVs() {
	return (int)m_aovs.size();
}