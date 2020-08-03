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
#include "frWrap.h"

#include "FireRenderAOVs.h"
#include "FireRenderGlobals.h"
#include "Context/FireRenderContext.h"
#include "FireRenderImageUtil.h"
#include "FireMaya.h"

#include <maya/MStatus.h>
#include <maya/MFnEnumAttribute.h>
#include <wchar.h>

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

	AddAOV(RPR_AOV_VOLUME, "aovVolume", "Subsurface / Volume", "volume",
		{ { "R", "G", "B" },{ TypeDesc::FLOAT, TypeDesc::VEC3, TypeDesc::COLOR } });

	AddAOV(RPR_AOV_LIGHT_GROUP0, "aovLightGroup0", "Light Group 0", "lightGroup0",
		{ { "R", "G", "B" },{ TypeDesc::FLOAT, TypeDesc::VEC3, TypeDesc::COLOR } });

	AddAOV(RPR_AOV_LIGHT_GROUP1, "aovLightGroup1", "Light Group 1", "lightGroup1",
		{ { "R", "G", "B" },{ TypeDesc::FLOAT, TypeDesc::VEC3, TypeDesc::COLOR } });

	AddAOV(RPR_AOV_LIGHT_GROUP2, "aovLightGroup2", "Light Group 2", "lightGroup2",
		{ { "R", "G", "B" },{ TypeDesc::FLOAT, TypeDesc::VEC3, TypeDesc::COLOR } });

	AddAOV(RPR_AOV_LIGHT_GROUP3, "aovLightGroup3", "Light Group 3", "lightGroup3",
		{ { "R", "G", "B" },{ TypeDesc::FLOAT, TypeDesc::VEC3, TypeDesc::COLOR } });

	AddAOV(RPR_AOV_DIFFUSE_ALBEDO, "aovAlbedo", "Albedo", "albedo",
		{ { "R", "G", "B" },{ TypeDesc::FLOAT, TypeDesc::VEC3, TypeDesc::COLOR } });

	AddAOV(RPR_AOV_VARIANCE, "aovVariance", "Variance", "variance",
		{ { "R", "G", "B" },{ TypeDesc::FLOAT, TypeDesc::VEC3, TypeDesc::COLOR } });

	AddAOV(RPR_AOV_REFLECTION_CATCHER, "aovReflectionCatcher", "ReflectionCatcher", "refcatcher",
		{ { "R", "G", "B" },{ TypeDesc::FLOAT, TypeDesc::VEC3, TypeDesc::COLOR } });

	InitEXRCompressionMap();
}

// -----------------------------------------------------------------------------
FireRenderAOVs::~FireRenderAOVs()
{
	freePixels();
}

void FireRenderAOVs::InitEXRCompressionMap()
{
	m_exrCompressionTypesMap[EXRCM_NONE] = "none";
	m_exrCompressionTypesMap[EXRCM_RLE] = "rle";
	m_exrCompressionTypesMap[EXRCM_ZIP] = "zip";
	m_exrCompressionTypesMap[EXRCM_PIZ] = "piz";
	m_exrCompressionTypesMap[EXRCM_PXR24] = "pxr24";

	m_exrCompressionTypesMap[EXRCM_B44] = "b44";
	m_exrCompressionTypesMap[EXRCM_B44A] = "b44a";
	m_exrCompressionTypesMap[EXRCM_DWAA] = "dwaa";
	m_exrCompressionTypesMap[EXRCM_DWAB] = "dwab";
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
FireRenderAOV* FireRenderAOVs::getAOV(unsigned int id)
{
	if (m_aovs.find(id) == m_aovs.end())
	{
		return nullptr;
	}

	return m_aovs[id].get();
}

// -----------------------------------------------------------------------------
FireRenderAOV& FireRenderAOVs::getRenderViewAOV()
{
	return *getAOV(m_renderViewAOVId);
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
		if (aov->id == RPR_AOV_COLOR || aov->id == m_renderViewAOVId)
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

	MObject renGlobalsObj;
	GetDefaultRenderGlobals(renGlobalsObj);
	MFnDependencyNode defaultGlobalsNode(renGlobalsObj);

	plug = defaultGlobalsNode.findPlug("exrCompression");

	int compressionIndex = -1;
	if (!plug.isNull())
	{ 
		compressionIndex = plug.asInt();
		m_exrCompressionType = m_exrCompressionTypesMap[compressionIndex];
	}
	else
	{
		assert(false);
	}

	plug = defaultGlobalsNode.findPlug("exrPixelType");

	if (!plug.isNull())
	{
		m_channelFormat = plug.asInt() == 0 ? TypeDesc::FLOAT : TypeDesc::HALF;
	}
	else
	{
		assert(false);
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
void FireRenderAOVs::setFromContext(FireRenderContext& context)
{
	for (auto& aov : m_aovs)
	{
		if (context.isAOVEnabled(aov.second->id))
			aov.second->active = true;
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
void FireRenderAOVs::writeToFile(const MString& filePath, unsigned int imageFormat, FireRenderAOV::FileWrittenCallback fileWrittenCallback)
{
	// Check if only the color AOV is active.
	bool colorOnly = getActiveAOVCount() == 1;

	// For EXR, may want save all AOVs to a single multichannel file.
	MString extension = FireRenderImageUtil::getImageFormatExtension(imageFormat);

	if ((extension == "exr") && (FireRenderGlobalsData::isExrMultichannelEnabled()) )
	{
		if (FireRenderImageUtil::saveMultichannelAOVs(filePath,
			m_region.getWidth(), m_region.getHeight(), imageFormat, *this))
		{
			if (fileWrittenCallback != nullptr)
			{
				fileWrittenCallback(filePath);
			}
		}
	}

	// Otherwise, write active AOVs to individual files.
	else
	{
		for (auto& aov : m_aovs)
		{
			aov.second->writeToFile(filePath, colorOnly, imageFormat, fileWrittenCallback);
		}
	}
}

int FireRenderAOVs::getNumberOfAOVs() 
{
	return (int)m_aovs.size();
}

MString FireRenderAOVs::GetEXRCompressionType() const 
{ 
	return m_exrCompressionType; 
}

TypeDesc::BASETYPE FireRenderAOVs::GetChannelFormat() const
{
	return m_channelFormat;
}

void FireRenderAOVs::ForEachActiveAOV(std::function<void(FireRenderAOV& aov)> actionFunc)
{
	for (auto& aov : m_aovs)
	{
		if (aov.second->active)
			actionFunc(*aov.second.get());
	}
}
