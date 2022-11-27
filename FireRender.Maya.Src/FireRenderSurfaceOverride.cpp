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
#include "FireRenderSurfaceOverride.h"
#include <maya/MStringArray.h>
#include <maya/MFragmentManager.h>
#include <maya/MGlobal.h>
#include <maya/MShaderManager.h>
#include <maya/MDrawContext.h>
#include <maya/MPlugArray.h>
#include <maya/MHardwareRenderer.h>
#include <maya/MPxHwShaderNode.h>
#include <fstream>
#include "FireRenderMaterial.h"
#include "FireRenderStandardMaterial.h"

//-----------------------------------------------------------------------------
// FireRenderMaterial
//-----------------------------------------------------------------------------

FireRenderMaterialNodeOverride::FireRenderMaterialNodeOverride(const MObject& obj) : MPxSurfaceShadingNodeOverride(obj)
{
	m_shader = obj;
	m_type = getType();
}

FireRenderMaterialNodeOverride::~FireRenderMaterialNodeOverride()
{

}

int FireRenderMaterialNodeOverride::getType() const
{
	int res = -1;
	if (!m_shader.isNull())
	{
		MFnDependencyNode shaderNode(m_shader);
		MPlug plug = shaderNode.findPlug("type", false);
		if (!plug.isNull())
			plug.getValue(res);
	}
	return res;
}

MHWRender::MPxSurfaceShadingNodeOverride* FireRenderMaterialNodeOverride::creator(const MObject& obj)
{
	return new FireRenderMaterialNodeOverride(obj);
}

MHWRender::DrawAPI FireRenderMaterialNodeOverride::supportedDrawAPIs() const
{
	return MHWRender::DrawAPI::kAllDevices;
}

bool FireRenderMaterialNodeOverride::allowConnections() const
{
	return true;
}

MString FireRenderMaterialNodeOverride::fragmentName() const
{
	int type = getType(); // can't update m_type here
	switch (type)
	{
		case FireMaya::Material::kMicrofacetRefract:
		case FireMaya::Material::kReflect:
		case FireMaya::Material::kRefract:
		case FireMaya::Material::kTransparent:
		case FireMaya::Material::kEmissive:
		case FireMaya::Material::kDiffuseRefraction:
		case FireMaya::Material::kDiffuse:
		case FireMaya::Material::kMicrofacet:
		case FireMaya::Material::kOrenNayar:
			return "mayaBlinnSurface";
	}
	return "mayaBlinnSurface";
}

bool FireRenderMaterialNodeOverride::valueChangeRequiresFragmentRebuild(const MPlug* plug) const
{
	bool res = false;
	if (plug)
	{
		MString pname = plug->partialName(false, false, false, false, true, true);
		res = (pname == "type");
	}
	return res;
}

void FireRenderMaterialNodeOverride::updateDG()
{
}

void FireRenderMaterialNodeOverride::updateShader(MHWRender::MShaderInstance& shader, const MHWRender::MAttributeParameterMappingList& mappings)
{
	MStatus result;

	switch (m_type)
	{
		case FireMaya::Material::kMicrofacet:
		{
			float diff[] = { 0.f, 0.f, 0.f };
			result = shader.setParameter("color", diff);
			result = shader.setParameter("specularRollOff", 1.0f);
		}
		break;

		case FireMaya::Material::kDiffuse:
		case FireMaya::Material::kOrenNayar:
		case FireMaya::Material::kTransparent:
		case FireMaya::Material::kEmissive:
		{
			float diff[] = { 0.f, 0.f, 0.f };
			result = shader.setParameter("specularColor", diff);
			result = shader.setParameter("specularRollOff", 0.0f);
			result = shader.setParameter("eccentricity", 0.0f);

			// uncomment the below to get rid of the color
			// Maya computes the final color as: primary - transparency
			// therefore our surface color is inverted
			// since we only have one input parameter, transparency => the same
			// parameter ends up in both the fragment's transparency and the
			// fragment's primary color
			/*
			if (m_type == FireMaya::Material::kTransparent)
			{
				result = shader.setParameter("color", diff);
			}
			*/
		}
		break;
	}
}

MString FireRenderMaterialNodeOverride::primaryColorParameter() const
{
	switch (getType())
	{
		case FireMaya::Material::kMicrofacet:
			return "specularColor";
		case FireMaya::Material::kDiffuse:
		case FireMaya::Material::kOrenNayar:
			return "color";
		case FireMaya::Material::kTransparent:
			return "transparency";
	}
	return "color";
}

MString FireRenderMaterialNodeOverride::transparencyParameter() const
{
	switch (getType())
	{
		case FireMaya::Material::kTransparent:
			return "transparency";
	}
	return "";
}

const char* FireRenderMaterialNodeOverride::className()
{
	return "RPRMaterialNodeOverride";
}


//-----------------------------------------------------------------------------
// FireRenderStandardMaterial
//-----------------------------------------------------------------------------

// The preferred approach is to use fully custom shader fragment for this. However this approach would
// require much more effort for coding and testing.

FireRenderStandardMaterialNodeOverride::FireRenderStandardMaterialNodeOverride(const MObject& obj)
: MPxSurfaceShadingNodeOverride(obj)
, m_shader(obj)
{
}

FireRenderStandardMaterialNodeOverride::~FireRenderStandardMaterialNodeOverride()
{
}

MHWRender::MPxSurfaceShadingNodeOverride* FireRenderStandardMaterialNodeOverride::creator(const MObject& obj)
{
	return new FireRenderStandardMaterialNodeOverride(obj);
}

MHWRender::DrawAPI FireRenderStandardMaterialNodeOverride::supportedDrawAPIs() const
{
	return MHWRender::DrawAPI::kAllDevices;
}

bool FireRenderStandardMaterialNodeOverride::allowConnections() const
{
	return true;
}

MString FireRenderStandardMaterialNodeOverride::fragmentName() const
{
	return "mayaBlinnSurface";
}

#define GET_BOOL(_attrib_) \
	shaderNode.findPlug(_attrib_, false).asBool()

void FireRenderStandardMaterialNodeOverride::getCustomMappings(MHWRender::MAttributeParameterMappingList& mappings)
{
	if (m_shader.isNull())
		return;

	MFnDependencyNode shaderNode(m_shader);

	// We're mapping "float" value from material to "color" value in HW shader, so we
	// can't use automatic mapping.

	bool transparancyEnabled = GET_BOOL("transparencyEnable");
	bool useTransparancy3Comp = GET_BOOL("useTransparency3Components");

	if (transparancyEnabled && useTransparancy3Comp)
	{
		mappings.append(MHWRender::MAttributeParameterMapping("transparency", "transparency3Components", true, true));
	}
	else
	{
		mappings.append(MHWRender::MAttributeParameterMapping("transparency", "", true, true));
	}

	mappings.append(MHWRender::MAttributeParameterMapping("color", GET_BOOL("diffuse") ? "diffuseColor" : "", true, true));
	mappings.append(MHWRender::MAttributeParameterMapping("incandescence", GET_BOOL("emissive") ? "emissiveColor" : "", true, true));

	// Moved to avoid mapping here. It crashes for unknown reason.
	// UpdateShader call assigns eccentricity parameter to float value from reflectRoughness
	// It affects Default Maya material viewer does not show map (if selected in reflectRoughness) but we avoid crash
	mappings.append(MHWRender::MAttributeParameterMapping("eccentricity", "", true, true));

	bool hasReflection = GET_BOOL("reflections");
	mappings.append(MHWRender::MAttributeParameterMapping("specularColor", hasReflection ? "reflectColor" : "", true, true));
	mappings.append(MHWRender::MAttributeParameterMapping("specularRollOff", hasReflection ? "reflectWeight" : "", true, true));
}

bool FireRenderStandardMaterialNodeOverride::valueChangeRequiresFragmentRebuild(const MPlug* plug) const
{
	if (plug)
	{
		// Check if any attribute which affects results of getCustomMappings() is changed.
		MString pname = plug->partialName(false, false, false, false, true, true);
		if (pname == "diffuse" || pname == "emissive" || pname == "reflections" ||
			pname == "transparencyEnable" || pname == "useTransparency3Components")
		{
			return true;
		}
	}
	return false;
}

void FireRenderStandardMaterialNodeOverride::updateDG()
{
}

static MString GetResolvedParam(const MHWRender::MAttributeParameterMappingList& mappings, const MString& paramName)
{
	MString fResolvedParamName;
	const MHWRender::MAttributeParameterMapping* mapping = mappings.findByParameterName(paramName);
	if (mapping)
	{
		return mapping->resolvedParameterName();
	}
	return "";
}

void FireRenderStandardMaterialNodeOverride::updateShader(MHWRender::MShaderInstance& shader, const MHWRender::MAttributeParameterMappingList& mappings)
{
	// Note: MShaderInstance::setParameter() has effect only if this shader didn't have automatic connections
	// set up in getCustomMappings(). It seems shader works in one of two modes, depending on what was done
	// first after creation:
	// 1. if shader have got mappings set up, setParameter will not have any effect
	// 2. if shader was tweaked with setParameter, automatic mappings won't work

	// To solve this problem, parameter names should be resolved (see calls to "GetResolvedParam").

	if (m_shader.isNull())
		return;

	MFnDependencyNode shaderNode(m_shader);

	static const float white[3] = { 1.0f, 1.0f, 1.0f };
	static const float black[3] = { 0.0f, 0.0f, 0.0f };

	if (!GET_BOOL("diffuse"))
	{
		// When diffuse is off, it's white
		MString fResolvedParamName = GetResolvedParam(mappings, "color");
		if (fResolvedParamName.length() > 0)
		{
			shader.setParameter(fResolvedParamName, white);
		}
	}

	if (!GET_BOOL("reflections"))
	{
		MString fResolvedParamName = GetResolvedParam(mappings, "specularColor");
		if (fResolvedParamName.length() > 0)
		{
			shader.setParameter(fResolvedParamName, black);
		}
	}
	else
	{
		MString fResolvedParamName = GetResolvedParam(mappings, "eccentricity");

		if (fResolvedParamName.length() > 0)
		{
			float rr = 0.5f;
			shaderNode.findPlug("reflectRoughness", false).getValue(rr);

			shader.setParameter(fResolvedParamName, rr);
		}
	}
}

MString FireRenderStandardMaterialNodeOverride::primaryColorParameter() const
{
	return "color";
}

MString FireRenderStandardMaterialNodeOverride::transparencyParameter() const
{
	return "transparency";
}

const char* FireRenderStandardMaterialNodeOverride::className()
{
	return "RPRMaterialStandardNodeOverride";
}
