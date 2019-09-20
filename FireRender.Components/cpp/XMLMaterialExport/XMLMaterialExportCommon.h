// This is common part of RPR Material Export
#pragma once

#include <RadeonProRender.h>
#include <RadeonProRender_GL.h>
#include <rprDeprecatedApi.h>

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

static std::map<std::string, uint32_t> param2id = {

	{"color",RPR_MATERIAL_INPUT_COLOR},
	{"color0",RPR_MATERIAL_INPUT_COLOR0},
	{"color1",RPR_MATERIAL_INPUT_COLOR1},
	{"color2",RPR_MATERIAL_INPUT_COLOR2},
	{"color3",RPR_MATERIAL_INPUT_COLOR3},
	{"normal",RPR_MATERIAL_INPUT_NORMAL},
	{"uv",RPR_MATERIAL_INPUT_UV},
	{"data",RPR_MATERIAL_INPUT_DATA},
	{"roughness",RPR_MATERIAL_INPUT_ROUGHNESS},
	{"ior",RPR_MATERIAL_INPUT_IOR},
	{"roughness_x",RPR_MATERIAL_INPUT_ROUGHNESS_X},
	{"roughness_y",RPR_MATERIAL_INPUT_ROUGHNESS_Y},
	{"rotation",RPR_MATERIAL_INPUT_ROTATION},
	{"weight",RPR_MATERIAL_INPUT_WEIGHT},
	{"op",RPR_MATERIAL_INPUT_OP},
	{"invec",RPR_MATERIAL_INPUT_INVEC},
	{"uv_scale",RPR_MATERIAL_INPUT_UV_SCALE},
	{"value",RPR_MATERIAL_INPUT_VALUE},
	{"reflectance",RPR_MATERIAL_INPUT_REFLECTANCE},
	{"bumpscale",RPR_MATERIAL_INPUT_SCALE},
	{"sigmas",RPR_MATERIAL_INPUT_SCATTERING},
	{"sigmaa",RPR_MATERIAL_INPUT_ABSORBTION},
	{"emission",RPR_MATERIAL_INPUT_EMISSION},
	{"g",RPR_MATERIAL_INPUT_G},
	{"multiscatter",RPR_MATERIAL_INPUT_MULTISCATTER},
	{"anisotropic",RPR_MATERIAL_INPUT_ANISOTROPIC},
	{"frontface",RPR_MATERIAL_INPUT_FRONTFACE},
	{"backface",RPR_MATERIAL_INPUT_BACKFACE},
	{"origin",RPR_MATERIAL_INPUT_ORIGIN},
	{"zaxis",RPR_MATERIAL_INPUT_ZAXIS},
	{"xaxis",RPR_MATERIAL_INPUT_XAXIS},
	{"threshold",RPR_MATERIAL_INPUT_THRESHOLD},
	{"offset",RPR_MATERIAL_INPUT_OFFSET},
	{"uv_type",RPR_MATERIAL_INPUT_UV_TYPE},
	{"radius",RPR_MATERIAL_INPUT_RADIUS},
	{"side",RPR_MATERIAL_INPUT_SIDE},
	{"caustics",RPR_MATERIAL_INPUT_CAUSTICS},
	{"transmission_color",RPR_MATERIAL_INPUT_TRANSMISSION_COLOR},
	{"thickness",RPR_MATERIAL_INPUT_THICKNESS},
	{"input0",RPR_MATERIAL_INPUT_0},
	{"input1",RPR_MATERIAL_INPUT_1},
	{"input2",RPR_MATERIAL_INPUT_2},
	{"input3",RPR_MATERIAL_INPUT_3},
	{"input4",RPR_MATERIAL_INPUT_4},
	{"schlickapprox",RPR_MATERIAL_INPUT_SCHLICK_APPROXIMATION},
	{"applysurface",RPR_MATERIAL_INPUT_APPLYSURFACE},


	{"diffuse.color",RPR_UBER_MATERIAL_INPUT_DIFFUSE_COLOR},
	{"diffuse.weight",RPR_UBER_MATERIAL_INPUT_DIFFUSE_WEIGHT},
	{"diffuse.roughness",RPR_UBER_MATERIAL_INPUT_DIFFUSE_ROUGHNESS},
	{"diffuse.normal",RPR_UBER_MATERIAL_INPUT_DIFFUSE_NORMAL},
	{"reflection.color",RPR_UBER_MATERIAL_INPUT_REFLECTION_COLOR},
	{"reflection.weight",RPR_UBER_MATERIAL_INPUT_REFLECTION_WEIGHT},
	{"reflection.roughness",RPR_UBER_MATERIAL_INPUT_REFLECTION_ROUGHNESS},
	{"reflection.anisotropy",RPR_UBER_MATERIAL_INPUT_REFLECTION_ANISOTROPY},
	{"reflection.anistropyrotation",RPR_UBER_MATERIAL_INPUT_REFLECTION_ANISOTROPY_ROTATION},
	{"reflection.mode",RPR_UBER_MATERIAL_INPUT_REFLECTION_MODE},
	{"reflection.ior",RPR_UBER_MATERIAL_INPUT_REFLECTION_IOR},
	{"reflection.metalness",RPR_UBER_MATERIAL_INPUT_REFLECTION_METALNESS},
	{"reflection.normal",RPR_UBER_MATERIAL_INPUT_REFLECTION_NORMAL},
	{"refraction.color",RPR_UBER_MATERIAL_INPUT_REFRACTION_COLOR},
	{"refraction.weight",RPR_UBER_MATERIAL_INPUT_REFRACTION_WEIGHT},
	{"refraction.roughness",RPR_UBER_MATERIAL_INPUT_REFRACTION_ROUGHNESS},
	{"refraction.ior",RPR_UBER_MATERIAL_INPUT_REFRACTION_IOR},
	{"refraction.normal",RPR_UBER_MATERIAL_INPUT_REFRACTION_NORMAL},
	{"refraction.thinsurface",RPR_UBER_MATERIAL_INPUT_REFRACTION_THIN_SURFACE},
	{"refraction.absorptioncolor",RPR_UBER_MATERIAL_INPUT_REFRACTION_ABSORPTION_COLOR},
	{"refraction.absorptiondistance",RPR_UBER_MATERIAL_INPUT_REFRACTION_ABSORPTION_DISTANCE},
	{"refraction.caustics",RPR_UBER_MATERIAL_INPUT_REFRACTION_CAUSTICS},
	{"coating.color",RPR_UBER_MATERIAL_INPUT_COATING_COLOR},
	{"coating.weight",RPR_UBER_MATERIAL_INPUT_COATING_WEIGHT},
	{"coating.roughness",RPR_UBER_MATERIAL_INPUT_COATING_ROUGHNESS},
	{"coating.mode",RPR_UBER_MATERIAL_INPUT_COATING_MODE},
	{"coating.ior",RPR_UBER_MATERIAL_INPUT_COATING_IOR},
	{"coating.metalness",RPR_UBER_MATERIAL_INPUT_COATING_METALNESS},
	{"coating.normal",RPR_UBER_MATERIAL_INPUT_COATING_NORMAL},
	{"coating.transmissioncolor",RPR_UBER_MATERIAL_INPUT_COATING_TRANSMISSION_COLOR},
	{"coating.thickness",RPR_UBER_MATERIAL_INPUT_COATING_THICKNESS},
	{"sheen",RPR_UBER_MATERIAL_INPUT_SHEEN},
	{"sheen.tint",RPR_UBER_MATERIAL_INPUT_SHEEN_TINT},
	{"sheen.weight",RPR_UBER_MATERIAL_INPUT_SHEEN_WEIGHT},
	{"emission.color",RPR_UBER_MATERIAL_INPUT_EMISSION_COLOR},
	{"emission.weight",RPR_UBER_MATERIAL_INPUT_EMISSION_WEIGHT},
	{"emission.mode",RPR_UBER_MATERIAL_INPUT_EMISSION_MODE},
	{"transparency",RPR_UBER_MATERIAL_INPUT_TRANSPARENCY},
	//{"displacement",RPR_UBER_MATERIAL_INPUT_DISPLACEMENT},//removedwithUberinTahoeCorefeature
	{"sss.scattercolor",RPR_UBER_MATERIAL_INPUT_SSS_SCATTER_COLOR},
	{"sss.scatterdistance",RPR_UBER_MATERIAL_INPUT_SSS_SCATTER_DISTANCE},
	{"sss.scatterdirection",RPR_UBER_MATERIAL_INPUT_SSS_SCATTER_DIRECTION},
	{"sss.weight",RPR_UBER_MATERIAL_INPUT_SSS_WEIGHT},
	{"sss.multiscatter",RPR_UBER_MATERIAL_INPUT_SSS_MULTISCATTER},
	{"backscatter.weight",RPR_UBER_MATERIAL_INPUT_BACKSCATTER_WEIGHT},
	{"backscatter.color",RPR_UBER_MATERIAL_INPUT_BACKSCATTER_COLOR},
	{"schlickapprox",RPR_UBER_MATERIAL_INPUT_FRESNEL_SCHLICK_APPROXIMATION},


	//
	//list from RPRHybrid: this project is not using the same strings that RPR-Tahoe
	//
	{"uberv2.diffuse_color",RPR_UBER_MATERIAL_INPUT_DIFFUSE_COLOR},
	{"uberv2.diffuse_roughness",RPR_UBER_MATERIAL_INPUT_DIFFUSE_ROUGHNESS},
	{"uberv2.diffuse_normal",RPR_UBER_MATERIAL_INPUT_DIFFUSE_NORMAL},
	{"uberv2.diffuse_weight",RPR_UBER_MATERIAL_INPUT_DIFFUSE_WEIGHT},
	{"uberv2.reflection_color",RPR_UBER_MATERIAL_INPUT_REFLECTION_COLOR},
	{"uberv2.reflection_roughness",RPR_UBER_MATERIAL_INPUT_REFLECTION_ROUGHNESS},
	{"uberv2.reflection_anisotropy",RPR_UBER_MATERIAL_INPUT_REFLECTION_ANISOTROPY},
	{"uberv2.reflection_anysotropy_rotation",RPR_UBER_MATERIAL_INPUT_REFLECTION_ANISOTROPY_ROTATION},
	{"uberv2.reflection_mode",RPR_UBER_MATERIAL_INPUT_REFLECTION_MODE},
	{"uberv2.reflection_ior",RPR_UBER_MATERIAL_INPUT_REFLECTION_IOR},
	{"uberv2.reflection_metalness",RPR_UBER_MATERIAL_INPUT_REFLECTION_METALNESS},
	{"uberv2.reflection_weight",RPR_UBER_MATERIAL_INPUT_REFLECTION_WEIGHT},
	{"uberv2.refraction_color",RPR_UBER_MATERIAL_INPUT_REFRACTION_COLOR},
	{"uberv2.refraction_roughness",RPR_UBER_MATERIAL_INPUT_REFRACTION_ROUGHNESS},
	{"uberv2.refraction_ior",RPR_UBER_MATERIAL_INPUT_REFRACTION_IOR},
	{"uberv2.refraction_thin_surface",RPR_UBER_MATERIAL_INPUT_REFRACTION_THIN_SURFACE},
	{"uberv2.refraction_weight",RPR_UBER_MATERIAL_INPUT_REFRACTION_WEIGHT},
	{"uberv2.coating_color",RPR_UBER_MATERIAL_INPUT_COATING_COLOR},
	{"uberv2.coating_roughness",RPR_UBER_MATERIAL_INPUT_COATING_ROUGHNESS},
	{"uberv2.coating_mode",RPR_UBER_MATERIAL_INPUT_COATING_MODE},
	{"uberv2.coating_ior",RPR_UBER_MATERIAL_INPUT_COATING_IOR},
	{"uberv2.coating_metalness",RPR_UBER_MATERIAL_INPUT_COATING_METALNESS},
	{"uberv2.coating_normal",RPR_UBER_MATERIAL_INPUT_COATING_NORMAL},
	{"uberv2.coating_weight",RPR_UBER_MATERIAL_INPUT_COATING_WEIGHT},
	{"uberv2.emission_color",RPR_UBER_MATERIAL_INPUT_EMISSION_COLOR},
	{"uberv2.emission_weight",RPR_UBER_MATERIAL_INPUT_EMISSION_WEIGHT},
	{"uberv2.transparency",RPR_UBER_MATERIAL_INPUT_TRANSPARENCY},


};

template<typename Key, typename T>
static std::map<T, Key> reverse_map(const std::map<Key, T>& source) 
{
	std::map<T, Key> out;
	for (const auto& v : source)
		out[v.second] = v.first;
	return out;
}

static std::map<uint32_t, std::string> id2param = reverse_map(param2id);
