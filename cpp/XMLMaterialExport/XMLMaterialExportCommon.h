// This is common part of RPR Material Export
#pragma once

#include <RadeonProRender.h>
#include <RadeonProRender_GL.h>
#include <RprSupport.h>

#include <list>
#include <map>
#include <vector>
#include <cassert>
#ifndef OSMac_
#include <memory>
#endif
#include <set>
#include <string>
#include <unordered_map>

struct EXTRA_IMAGE_DATA
{
	EXTRA_IMAGE_DATA()
	{
		scaleX = 1.0;
		scaleY = 1.0;
	}

	std::string path;
	float scaleX;
	float scaleY;
};

struct RPRX_DEFINE_PARAM_MATERIAL
{
	RPRX_DEFINE_PARAM_MATERIAL(rprx_parameter param_, const std::string& name_)
	{
		param = param_;
		nameInXML = name_;
	}
	rprx_parameter param;
	std::string nameInXML;
};

std::vector<RPRX_DEFINE_PARAM_MATERIAL>& GetRPRXParamList(void);

void InitParamList(void);

struct RPR_MATERIAL_XML_EXPORT_TEXTURE_PARAM
{
	RPR_MATERIAL_XML_EXPORT_TEXTURE_PARAM()
	{
		useGamma = false;
	}
	bool useGamma;
};

// return true if success
bool ParseRPR(
	rpr_material_node material,
	std::set<rpr_material_node>& nodeList,
	std::set<rprx_material>& nodeListX,
	const std::string& folder,
	bool originIsUberColor, // set to TRUE if this 'material' is connected to a COLOR input of the UBER material
	std::unordered_map<rpr_image, RPR_MATERIAL_XML_EXPORT_TEXTURE_PARAM>& textureParameter,
	const std::string& material_name,
	bool exportImageUV
);

// return true if success
bool ParseRPRX(
	rprx_material materialX,
	rprx_context contextX,
	std::set<rpr_material_node>& nodeList,
	std::set<rprx_material>& nodeListX,
	const std::string& folder,
	std::unordered_map<rpr_image, RPR_MATERIAL_XML_EXPORT_TEXTURE_PARAM>& textureParameter,
	const std::string& material_name,
	bool exportImageUV
);

void ExportMaterials(const std::string& filename,
	const std::set<rpr_material_node>& nodeList,
	const std::set<rprx_material>& nodeListX,
	const std::vector<RPRX_DEFINE_PARAM_MATERIAL>& rprxParamList,
	rprx_context contextX,
	std::unordered_map<rpr_image, RPR_MATERIAL_XML_EXPORT_TEXTURE_PARAM>& textureParameter,
	void* closureNode,  // pointer to a rpr_material_node or a rprx_material
	const std::string& material_name,// example : "Emissive_Fluorescent_Magenta"
	const std::map<rpr_image, EXTRA_IMAGE_DATA>& extraImageData,
	bool exportImageUV
);

