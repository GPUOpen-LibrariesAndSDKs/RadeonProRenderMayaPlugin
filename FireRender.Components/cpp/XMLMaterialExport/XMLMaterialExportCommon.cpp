#include "XMLMaterialExportCommon.h"
#include "Logger.h"
#include <stack>
#include <fstream>
#include <ostream>
#include <sstream>
#include <iostream>

// execute FireRender func and check for an error
#define CHECK_NO_ERROR(func)	{ \
								rpr_int status = func; \
								if (status != RPR_SUCCESS) \
									throw std::runtime_error("Radeon ProRender error (" + std::to_string(status) + ") in " + std::string(__FILE__) + ":" + std::to_string(__LINE__)); \
								}

std::vector<RPRX_DEFINE_PARAM_MATERIAL> g_rprxParamList;

std::vector<RPRX_DEFINE_PARAM_MATERIAL>& GetRPRXParamList(void)
{
	return g_rprxParamList;
}

namespace
{
	const std::string kTab = "    "; // default 4spaces tab for xml writer
#ifdef RPR_VERSION_MAJOR_MINOR_REVISION
	const int kVersion = RPR_VERSION_MAJOR_MINOR_REVISION;
#else
	const int kVersion = RPR_API_VERSION;
#endif
	//need different maps because RPR_MATERIAL* constants have not an unique value;
	const std::map<int, std::string> kMaterialTypeNames{ { RPR_MATERIAL_NODE_DIFFUSE, "DIFFUSE" },
	{ RPR_MATERIAL_NODE_MICROFACET, "MICROFACET" },
	{ RPR_MATERIAL_NODE_REFLECTION, "REFLECTION" },
	{ RPR_MATERIAL_NODE_REFRACTION, "REFRACTION" },
	{ RPR_MATERIAL_NODE_MICROFACET_REFRACTION, "MICROFACET_REFRACTION" },
	{ RPR_MATERIAL_NODE_TRANSPARENT, "TRANSPARENT" },
	{ RPR_MATERIAL_NODE_EMISSIVE, "EMISSIVE" },
	{ RPR_MATERIAL_NODE_WARD, "WARD" },
	{ RPR_MATERIAL_NODE_ADD, "ADD" },
	{ RPR_MATERIAL_NODE_BLEND, "BLEND" },
	{ RPR_MATERIAL_NODE_ARITHMETIC, "ARITHMETIC" },
	{ RPR_MATERIAL_NODE_FRESNEL, "FRESNEL" },
	{ RPR_MATERIAL_NODE_NORMAL_MAP, "NORMAL_MAP" },
	{ RPR_MATERIAL_NODE_IMAGE_TEXTURE, "IMAGE_TEXTURE" },
	{ RPR_MATERIAL_NODE_NOISE2D_TEXTURE, "NOISE2D_TEXTURE" },
	{ RPR_MATERIAL_NODE_DOT_TEXTURE, "DOT_TEXTURE" },
	{ RPR_MATERIAL_NODE_GRADIENT_TEXTURE, "GRADIENT_TEXTURE" },
	{ RPR_MATERIAL_NODE_CHECKER_TEXTURE, "CHECKER_TEXTURE" },
	{ RPR_MATERIAL_NODE_CONSTANT_TEXTURE, "CONSTANT_TEXTURE" },
	{ RPR_MATERIAL_NODE_INPUT_LOOKUP, "INPUT_LOOKUP" },
#if (RPR_VERSION_MINOR < 34)
	{ RPR_MATERIAL_NODE_STANDARD, "STANDARD" },
#else
	{ RPR_MATERIAL_NODE_UBERV2, "UBER" },
#endif
	{ RPR_MATERIAL_NODE_BLEND_VALUE, "BLEND_VALUE" },
	{ RPR_MATERIAL_NODE_PASSTHROUGH, "PASSTHROUGH" },
	{ RPR_MATERIAL_NODE_ORENNAYAR, "ORENNAYAR" },
	{ RPR_MATERIAL_NODE_FRESNEL_SCHLICK, "FRESNEL_SCHLICK" },
	{ RPR_MATERIAL_NODE_DIFFUSE_REFRACTION, "DIFFUSE_REFRACTION" },
	{ RPR_MATERIAL_NODE_BUMP_MAP, "BUMP_MAP" } };

	const std::map<std::string, int> kNodeTypesMap = { { "INPUT_COLOR4F", RPR_MATERIAL_NODE_INPUT_TYPE_FLOAT4 },
	{ "INPUT_FLOAT1", RPR_MATERIAL_NODE_INPUT_TYPE_FLOAT4 },
	{ "INPUT_UINT", RPR_MATERIAL_NODE_INPUT_TYPE_UINT },
	{ "INPUT_NODE", RPR_MATERIAL_NODE_INPUT_TYPE_NODE },
	{ "INPUT_TEXTURE", RPR_MATERIAL_NODE_INPUT_TYPE_IMAGE },
	{ "DIFFUSE", RPR_MATERIAL_NODE_DIFFUSE },
	{ "MICROFACET", RPR_MATERIAL_NODE_MICROFACET },
	{ "REFLECTION", RPR_MATERIAL_NODE_REFLECTION },
	{ "REFRACTION", RPR_MATERIAL_NODE_REFRACTION },
	{ "MICROFACET_REFRACTION", RPR_MATERIAL_NODE_MICROFACET_REFRACTION },
	{ "TRANSPARENT", RPR_MATERIAL_NODE_TRANSPARENT },
	{ "EMISSIVE", RPR_MATERIAL_NODE_EMISSIVE },
	{ "WARD", RPR_MATERIAL_NODE_WARD },
	{ "ADD", RPR_MATERIAL_NODE_ADD },
	{ "BLEND", RPR_MATERIAL_NODE_BLEND },
	{ "ARITHMETIC", RPR_MATERIAL_NODE_ARITHMETIC },
	{ "FRESNEL", RPR_MATERIAL_NODE_FRESNEL },
	{ "NORMAL_MAP", RPR_MATERIAL_NODE_NORMAL_MAP },
	{ "IMAGE_TEXTURE", RPR_MATERIAL_NODE_IMAGE_TEXTURE },
	{ "NOISE2D_TEXTURE", RPR_MATERIAL_NODE_NOISE2D_TEXTURE },
	{ "DOT_TEXTURE", RPR_MATERIAL_NODE_DOT_TEXTURE },
	{ "GRADIENT_TEXTURE", RPR_MATERIAL_NODE_GRADIENT_TEXTURE },
	{ "CHECKER_TEXTURE", RPR_MATERIAL_NODE_CHECKER_TEXTURE },
	{ "CONSTANT_TEXTURE", RPR_MATERIAL_NODE_CONSTANT_TEXTURE },
	{ "INPUT_LOOKUP", RPR_MATERIAL_NODE_INPUT_LOOKUP },
#if (RPR_VERSION_MINOR < 34)
	{ "STANDARD", RPR_MATERIAL_NODE_STANDARD },
#else
	{ "STANDARD", RPR_MATERIAL_NODE_UBERV2 },
	{ "UBER", RPR_MATERIAL_NODE_UBERV2 },
#endif
	{ "BLEND_VALUE", RPR_MATERIAL_NODE_BLEND_VALUE },
	{ "PASSTHROUGH", RPR_MATERIAL_NODE_PASSTHROUGH },
	{ "ORENNAYAR", RPR_MATERIAL_NODE_ORENNAYAR },
	{ "FRESNEL_SCHLICK", RPR_MATERIAL_NODE_FRESNEL_SCHLICK },
	{ "DIFFUSE_REFRACTION", RPR_MATERIAL_NODE_DIFFUSE_REFRACTION },
	{ "BUMP_MAP", RPR_MATERIAL_NODE_BUMP_MAP } };

	struct Param
	{
		std::string type;
		std::string value;
	};

	class XmlWriter
	{
	public:
		XmlWriter(const std::string& file)
			: m_doc(file)
			, top_written(true)
		{}
		~XmlWriter()
		{
			endDocument();
		}
		//write header
		void startDocument()
		{
			m_doc << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << std::endl;
		}
		void endDocument()
		{
			size_t size = m_nodes.size();
			for (size_t i = 0; i < size; ++i)
			{
				endElement();
			}
		}
		void startElement(const std::string& node_name)
		{
			//write prev node with atts
			if (m_nodes.size() != 0 && top_written)
			{
				std::string tab = "";
				for (int i = 0; i < m_nodes.size() - 1; ++i) tab += kTab;
				const Node& node = m_nodes.top();
				m_doc << tab << "<" << node.name << "";
				for (const auto& at : node.atts)
				{
					m_doc << " " << at.type << "=\"" << at.value << "\""; // name="value"
				}
				m_doc << ">" << std::endl;
			}

			m_nodes.push({ node_name });
			top_written = true;
		}

		void endElement()
		{
			std::string tab = "";
			for (int i = 0; i < m_nodes.size() - 1; ++i) tab += kTab;
			const Node& node = m_nodes.top();
			if (top_written)
			{
				m_doc << tab << "<" << node.name << "";
				for (const auto& at : node.atts)
				{
					m_doc << " " << at.type << "=\"" << at.value << "\""; // name="value"
				}
				m_doc << "/>" << std::endl;
			}
			else
				m_doc << tab << "</" << node.name << ">" << std::endl;

			m_nodes.pop();
			top_written = false;
		}

		void writeAttribute(const std::string& name, const std::string& value)
		{
			Node& node = m_nodes.top();
			node.atts.push_back({ name, value });
		}

		void writeTextElement(const std::string& name, const std::string& text)
		{
			std::string tab = "";
			for (int i = 0; i < m_nodes.size() - 1; ++i) tab += kTab;
			if (m_nodes.size() != 0 && top_written)
			{
				const Node& node = m_nodes.top();
				m_doc << tab << "<" << node.name << "";
				for (const auto& at : node.atts)
				{
					m_doc << " " << at.type << "=\"" << at.value << "\""; // name="value"
				}
				m_doc << ">" << std::endl;
			}
			tab += kTab;
			//<name>text</name>
			m_doc << tab << "<" << name << ">" << text << "</" << name << ">" << std::endl;
			top_written = false;

		}

	private:
		std::ofstream m_doc;

		struct Node
		{
			std::string name;
			std::vector<Param> atts;
		};

		std::stack<Node> m_nodes;
		bool top_written; // show is element in top of m_nodes stack already written into xml or not.
	};

}

void InitParamList(void)
{
	if (g_rprxParamList.size() != 0) // if it's the first time , fill the variable
		return;

	g_rprxParamList.push_back(RPRX_DEFINE_PARAM_MATERIAL(RPRX_UBER_MATERIAL_DIFFUSE_COLOR, "diffuse.color"));
	g_rprxParamList.push_back(RPRX_DEFINE_PARAM_MATERIAL(RPRX_UBER_MATERIAL_DIFFUSE_WEIGHT, "diffuse.weight"));
	g_rprxParamList.push_back(RPRX_DEFINE_PARAM_MATERIAL(RPRX_UBER_MATERIAL_DIFFUSE_ROUGHNESS, "diffuse.roughness"));
	g_rprxParamList.push_back(RPRX_DEFINE_PARAM_MATERIAL(RPRX_UBER_MATERIAL_DIFFUSE_NORMAL, "diffuse.normal"));
	g_rprxParamList.push_back(RPRX_DEFINE_PARAM_MATERIAL(RPRX_UBER_MATERIAL_REFLECTION_COLOR, "reflection.color"));
	g_rprxParamList.push_back(RPRX_DEFINE_PARAM_MATERIAL(RPRX_UBER_MATERIAL_REFLECTION_WEIGHT, "reflection.weight"));
	g_rprxParamList.push_back(RPRX_DEFINE_PARAM_MATERIAL(RPRX_UBER_MATERIAL_REFLECTION_ROUGHNESS, "reflection.roughness"));
	g_rprxParamList.push_back(RPRX_DEFINE_PARAM_MATERIAL(RPRX_UBER_MATERIAL_REFLECTION_ANISOTROPY, "reflection.anisotropy"));
	g_rprxParamList.push_back(RPRX_DEFINE_PARAM_MATERIAL(RPRX_UBER_MATERIAL_REFLECTION_ANISOTROPY_ROTATION, "reflection.anistropyRotation"));
	g_rprxParamList.push_back(RPRX_DEFINE_PARAM_MATERIAL(RPRX_UBER_MATERIAL_REFLECTION_MODE, "reflection.mode"));
	g_rprxParamList.push_back(RPRX_DEFINE_PARAM_MATERIAL(RPRX_UBER_MATERIAL_REFLECTION_IOR, "reflection.ior"));
	g_rprxParamList.push_back(RPRX_DEFINE_PARAM_MATERIAL(RPRX_UBER_MATERIAL_REFLECTION_NORMAL, "reflection.normal"));
	g_rprxParamList.push_back(RPRX_DEFINE_PARAM_MATERIAL(RPRX_UBER_MATERIAL_REFRACTION_COLOR, "refraction.color"));
	g_rprxParamList.push_back(RPRX_DEFINE_PARAM_MATERIAL(RPRX_UBER_MATERIAL_REFRACTION_WEIGHT, "refraction.weight"));
	g_rprxParamList.push_back(RPRX_DEFINE_PARAM_MATERIAL(RPRX_UBER_MATERIAL_REFRACTION_ROUGHNESS, "refraction.roughness"));
	g_rprxParamList.push_back(RPRX_DEFINE_PARAM_MATERIAL(RPRX_UBER_MATERIAL_REFRACTION_IOR, "refraction.ior"));
	g_rprxParamList.push_back(RPRX_DEFINE_PARAM_MATERIAL(RPRX_UBER_MATERIAL_REFRACTION_THIN_SURFACE, "refraction.thinSurface"));
	g_rprxParamList.push_back(RPRX_DEFINE_PARAM_MATERIAL(RPRX_UBER_MATERIAL_REFRACTION_ABSORPTION_COLOR, "refraction.absorptionColor"));
	g_rprxParamList.push_back(RPRX_DEFINE_PARAM_MATERIAL(RPRX_UBER_MATERIAL_REFRACTION_ABSORPTION_DISTANCE, "refraction.absorptionDistance"));
	g_rprxParamList.push_back(RPRX_DEFINE_PARAM_MATERIAL(RPRX_UBER_MATERIAL_REFRACTION_CAUSTICS, "refraction.caustics"));
	g_rprxParamList.push_back(RPRX_DEFINE_PARAM_MATERIAL(RPRX_UBER_MATERIAL_COATING_COLOR, "coating.color"));
	g_rprxParamList.push_back(RPRX_DEFINE_PARAM_MATERIAL(RPRX_UBER_MATERIAL_COATING_WEIGHT, "coating.weight"));
	g_rprxParamList.push_back(RPRX_DEFINE_PARAM_MATERIAL(RPRX_UBER_MATERIAL_COATING_ROUGHNESS, "coating.roughness"));
	g_rprxParamList.push_back(RPRX_DEFINE_PARAM_MATERIAL(RPRX_UBER_MATERIAL_COATING_MODE, "coating.mode"));
	g_rprxParamList.push_back(RPRX_DEFINE_PARAM_MATERIAL(RPRX_UBER_MATERIAL_COATING_IOR, "coating.ior"));
	g_rprxParamList.push_back(RPRX_DEFINE_PARAM_MATERIAL(RPRX_UBER_MATERIAL_COATING_METALNESS, "coating.metalness"));
	g_rprxParamList.push_back(RPRX_DEFINE_PARAM_MATERIAL(RPRX_UBER_MATERIAL_COATING_NORMAL, "coating.normal"));
	g_rprxParamList.push_back(RPRX_DEFINE_PARAM_MATERIAL(RPRX_UBER_MATERIAL_COATING_TRANSMISSION_COLOR, "coating.transmissionColor"));
	g_rprxParamList.push_back(RPRX_DEFINE_PARAM_MATERIAL(RPRX_UBER_MATERIAL_COATING_THICKNESS, "coating.thickness"));
	g_rprxParamList.push_back(RPRX_DEFINE_PARAM_MATERIAL(RPRX_UBER_MATERIAL_SHEEN, "sheen"));
	g_rprxParamList.push_back(RPRX_DEFINE_PARAM_MATERIAL(RPRX_UBER_MATERIAL_SHEEN_TINT, "sheen.tint"));
	g_rprxParamList.push_back(RPRX_DEFINE_PARAM_MATERIAL(RPRX_UBER_MATERIAL_SHEEN_WEIGHT, "sheen.weight"));
	g_rprxParamList.push_back(RPRX_DEFINE_PARAM_MATERIAL(RPRX_UBER_MATERIAL_EMISSION_COLOR, "emission.color"));
	g_rprxParamList.push_back(RPRX_DEFINE_PARAM_MATERIAL(RPRX_UBER_MATERIAL_EMISSION_WEIGHT, "emission.weight"));
	g_rprxParamList.push_back(RPRX_DEFINE_PARAM_MATERIAL(RPRX_UBER_MATERIAL_EMISSION_MODE, "emission.mode"));
	g_rprxParamList.push_back(RPRX_DEFINE_PARAM_MATERIAL(RPRX_UBER_MATERIAL_TRANSPARENCY, "transparency"));
#if (RPR_VERSION_MINOR < 34)
	g_rprxParamList.push_back(RPRX_DEFINE_PARAM_MATERIAL(RPRX_UBER_MATERIAL_DISPLACEMENT, "displacement"));
#endif
	g_rprxParamList.push_back(RPRX_DEFINE_PARAM_MATERIAL(RPRX_UBER_MATERIAL_SSS_SCATTER_COLOR, "sss.scatterColor"));
	g_rprxParamList.push_back(RPRX_DEFINE_PARAM_MATERIAL(RPRX_UBER_MATERIAL_SSS_SCATTER_DISTANCE, "sss.scatterDistance"));
	g_rprxParamList.push_back(RPRX_DEFINE_PARAM_MATERIAL(RPRX_UBER_MATERIAL_SSS_SCATTER_DIRECTION, "sss.scatterDirection"));
	g_rprxParamList.push_back(RPRX_DEFINE_PARAM_MATERIAL(RPRX_UBER_MATERIAL_SSS_WEIGHT, "sss.weight"));
	g_rprxParamList.push_back(RPRX_DEFINE_PARAM_MATERIAL(RPRX_UBER_MATERIAL_SSS_MULTISCATTER, "sss.multiscatter"));
	g_rprxParamList.push_back(RPRX_DEFINE_PARAM_MATERIAL(RPRX_UBER_MATERIAL_BACKSCATTER_WEIGHT, "backscatter.weight"));
	g_rprxParamList.push_back(RPRX_DEFINE_PARAM_MATERIAL(RPRX_UBER_MATERIAL_BACKSCATTER_COLOR, "backscatter.color"));
}

// return true if success
bool ParseRPRX (
	rprx_material materialX ,
	rprx_context contextX, 
	std::set<rpr_material_node>& nodeList ,
	std::set<rprx_material>& nodeListX ,
	const std::string& folder ,
	std::unordered_map<rpr_image , RPR_MATERIAL_XML_EXPORT_TEXTURE_PARAM>& textureParameter,
	const std::string& material_name,
	bool exportImageUV
	)
{
	rpr_int status = RPR_SUCCESS;

	//check if the node already stored
	if ( nodeListX.find(materialX) != nodeListX.end() )
	{
		return true;
	}
	
	nodeListX.insert(materialX);

	rpr_material_node_type type = 0;
	rprMaterialNodeGetInfo(materialX, RPR_MATERIAL_NODE_TYPE, sizeof(rpr_material_node_type), &type, nullptr);

#ifdef  _DEBUG
	{
		auto it = kMaterialTypeNames.find(type);
		if (it != kMaterialTypeNames.end())
		{
			std::string matName = it->second;
			DebugPrint("processed node name = %s", matName);
		}
	}
#endif

	for(int iParam=0; iParam<g_rprxParamList.size(); iParam++)
	{
		rpr_parameter_type type = 0;
		status = rprxMaterialGetParameterType(contextX,materialX,g_rprxParamList[iParam].param,&type); 
		if (status != RPR_SUCCESS) 
		{ 
			return false; 
		}

		if ( type == RPRX_PARAMETER_TYPE_FLOAT4 )
		{
			float value4f[] = { 0.0f,0.0f,0.0f,0.0f };
			status = rprxMaterialGetParameterValue(contextX,materialX,g_rprxParamList[iParam].param,&value4f); 
			if (status != RPR_SUCCESS) 
			{ 
				return false; 
			}
		}
		else if ( type == RPRX_PARAMETER_TYPE_UINT )
		{
			unsigned int value1u = 0;
			status = rprxMaterialGetParameterValue(contextX,materialX,g_rprxParamList[iParam].param,&value1u); 
			if (status != RPR_SUCCESS) 
			{ 
				return false; 
			}
		}
		else if ( type == RPRX_PARAMETER_TYPE_NODE )
		{
			rpr_material_node valueN = 0;
			status = rprxMaterialGetParameterValue(contextX,materialX,g_rprxParamList[iParam].param,&valueN); if (status != RPR_SUCCESS) { return false; };

			if ( valueN )
			{
				rpr_bool materialIsRPRX = false;
#if (RPR_VERSION_MINOR < 34)
				// check if the node is RPR or RPRX
				status = rprxIsMaterialRprx(contextX,valueN,nullptr,&materialIsRPRX); if (status != RPR_SUCCESS) { return false; };
#endif

				if ( materialIsRPRX )
				{
					// having a Uber material linked as an input of another Uber Material doesn't make sense and is not managed by
					// the RPRX library. only a rpr_material_node can be input in a rprx_material
				}
				else
				{
					bool originIsUberColor = false;
					if (   g_rprxParamList[iParam].param == RPRX_UBER_MATERIAL_DIFFUSE_COLOR
						|| g_rprxParamList[iParam].param == RPRX_UBER_MATERIAL_REFLECTION_COLOR
						|| g_rprxParamList[iParam].param == RPRX_UBER_MATERIAL_REFRACTION_COLOR
						|| g_rprxParamList[iParam].param == RPRX_UBER_MATERIAL_REFRACTION_ABSORPTION_COLOR
						|| g_rprxParamList[iParam].param == RPRX_UBER_MATERIAL_COATING_COLOR
						|| g_rprxParamList[iParam].param == RPRX_UBER_MATERIAL_COATING_TRANSMISSION_COLOR
						|| g_rprxParamList[iParam].param == RPRX_UBER_MATERIAL_EMISSION_COLOR
						|| g_rprxParamList[iParam].param == RPRX_UBER_MATERIAL_SSS_SCATTER_COLOR
						|| g_rprxParamList[iParam].param == RPRX_UBER_MATERIAL_BACKSCATTER_COLOR
						)
					{
						originIsUberColor = true;
					}

					bool success = ParseRPR(valueN , nodeList , nodeListX , folder , originIsUberColor, textureParameter, material_name, exportImageUV );
					if  ( !success )
					{
						return false;
					}
				}
			}
		}
		else
		{
			return false;
		}

	}

	return true;
}

// return true if success
bool ParseRPR (
	rpr_material_node material,
	std::set<rpr_material_node>& nodeList,
	std::set<rprx_material>& nodeListX,
	const std::string& folder,
	bool originIsUberColor, // set to TRUE if this 'material' is connected to a COLOR input of the UBER material
	std::unordered_map<rpr_image, RPR_MATERIAL_XML_EXPORT_TEXTURE_PARAM>& textureParameter,
	const std::string& material_name,
	bool exportImageUV
)
{
	// check if the node already stored
	if (nodeList.find(material) != nodeList.end())
	{
		return true;
	}

	nodeList.insert(material);

	rpr_int status = RPR_SUCCESS;

	rpr_material_node_type type = 0;
	rprMaterialNodeGetInfo(material, RPR_MATERIAL_NODE_TYPE, sizeof(rpr_material_node_type), &type, nullptr);

#ifdef  _DEBUG
	{
		auto it = kMaterialTypeNames.find(type);
		if (it != kMaterialTypeNames.end())
		{
			std::string matName = it->second;
			DebugPrint("processed node name = %s", matName);
		}
	}
#endif

	size_t input_count = 0;
	status = rprMaterialNodeGetInfo(material, RPR_MATERIAL_NODE_INPUT_COUNT, sizeof(size_t), &input_count, nullptr);

	for (int i = 0; i < input_count; ++i)
	{
		rpr_int in_type;
		status = rprMaterialNodeGetInputInfo(material, i, RPR_MATERIAL_NODE_INPUT_TYPE, sizeof(in_type), &in_type, nullptr);
		if (in_type == RPR_MATERIAL_NODE_INPUT_TYPE_NODE)
		{
			size_t read_count = 0;
			rprMaterialNodeGetInputInfo(material, i, RPR_MATERIAL_NODE_INPUT_NAME_STRING, sizeof(size_t), nullptr, &read_count);
			std::vector<char> param_name(read_count, ' ');
			rprMaterialNodeGetInputInfo(material, i, RPR_MATERIAL_NODE_INPUT_NAME_STRING, sizeof(param_name), param_name.data(), nullptr);

			rpr_material_node ref_node = nullptr;
			status = rprMaterialNodeGetInputInfo(material, i, RPR_MATERIAL_NODE_INPUT_VALUE, sizeof(ref_node), &ref_node, nullptr);
			if (ref_node)
			{

				if (!exportImageUV  // if UV not exported
					&& type == RPR_MATERIAL_NODE_IMAGE_TEXTURE
					&& param_name.size() == 3 && param_name[0] == 'u' && param_name[1] == 'v' && param_name[2] == 0
					)
				{
					// not supported atm
				}
				else
				{
					ParseRPR(ref_node, nodeList, nodeListX, folder, originIsUberColor, textureParameter, material_name, exportImageUV);
				}
			}
		}
		else if (in_type == RPR_MATERIAL_NODE_INPUT_TYPE_IMAGE)
		{
			rpr_image img = nullptr;
			status = rprMaterialNodeGetInputInfo(material, i, RPR_MATERIAL_NODE_INPUT_VALUE, sizeof(img), &img, nullptr);
			size_t name_size = 0;
			status = rprImageGetInfo(img, RPR_OBJECT_NAME, 0, nullptr, &name_size);
			//create file if filename is empty(name == "\0")
			if (name_size == 1)
			{
				// not supported atm
			}
			else
			{
				std::string mat_name;
				mat_name.resize(name_size - 1);
				rprImageGetInfo(img, RPR_OBJECT_NAME, name_size, &mat_name[0], nullptr);
			}

			textureParameter[img].useGamma = originIsUberColor ? true : false;
		}
	}

	return true;
}

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
)
{
	XmlWriter writer(filename);
	if (nodeListX.size() == 0 && nodeList.size() == 0)
	{
		std::cout << "MaterialExport error: materials input array is nullptr" << std::endl;
		return;
	}

	try
	{
		writer.startDocument();
		writer.startElement("material");
		writer.writeAttribute("name", material_name);

		// in case we need versioning the material XMLs...  -  unused for the moment.
		writer.writeAttribute("version_exporter", "10");

		// version of the RPR_API_VERSION used to generate this XML
		std::stringstream hex_version;
		hex_version << "0x" << std::hex << kVersion;
		writer.writeAttribute("version_rpr", hex_version.str());

		// file path as key, id - for material nodes connections
		std::map<std::string, std::pair< std::string, rpr_image >  > textures;
		std::map<void*, std::string> objects; // <object, node_name> map

		std::set<std::string> used_names; // to avoid duplicating node names

		std::string closureNodeName = "";

		// fill materials first
		for (const auto& iNode : nodeList)
		{
			rpr_material_node mat = iNode;
			if (objects.count(mat))
				continue;
			size_t name_size = 0;
			CHECK_NO_ERROR(rprMaterialNodeGetInfo(mat, RPR_OBJECT_NAME, NULL, nullptr, &name_size));
			std::string mat_node_name;
			mat_node_name.resize(name_size - 1); // exclude extra terminator
			CHECK_NO_ERROR(rprMaterialNodeGetInfo(mat, RPR_OBJECT_NAME, name_size, &mat_node_name[0], nullptr));
			int postfix_id = 0;

			if (mat_node_name.empty())
				mat_node_name = "node" + std::to_string(postfix_id);

			while (used_names.count(mat_node_name))
			{
				mat_node_name = "node" + std::to_string(postfix_id);
				++postfix_id;
			}

			if (mat == closureNode)
				closureNodeName = mat_node_name;

			objects[mat] = mat_node_name;
			used_names.insert(mat_node_name);
		}

		for (const auto& iNode : nodeListX)
		{
			rprx_material mat = iNode;
			if (objects.count(mat))
				continue;

			int postfix_id = 0;
			std::string mat_node_name = "Uber_" + std::to_string(postfix_id);

			while (used_names.count(mat_node_name))
			{
				mat_node_name = "Uber_" + std::to_string(postfix_id);
				++postfix_id;
			}

			if (mat == closureNode)
				closureNodeName = mat_node_name;

			objects[mat] = mat_node_name;
			used_names.insert(mat_node_name);
		}

		// closure_node is the name of the node containing the final output of the material
		writer.writeAttribute("closure_node", closureNodeName);

		// look for images
		for (const auto& iNode : nodeList)
		{
			rpr_material_node mat = iNode;
			size_t input_count = 0;
			CHECK_NO_ERROR(rprMaterialNodeGetInfo(mat, RPR_MATERIAL_NODE_INPUT_COUNT, sizeof(size_t), &input_count, nullptr));

			for (int input_id = 0; input_id < input_count; ++input_id)
			{
				rpr_material_node_type input_type;
				CHECK_NO_ERROR(rprMaterialNodeGetInputInfo(mat, input_id, RPR_MATERIAL_NODE_INPUT_TYPE, sizeof(input_type), &input_type, nullptr));

				if (input_type != RPR_MATERIAL_NODE_INPUT_TYPE_IMAGE)
					continue;

				rpr_image img = nullptr;
				CHECK_NO_ERROR(rprMaterialNodeGetInputInfo(mat, input_id, RPR_MATERIAL_NODE_INPUT_VALUE, sizeof(img), &img, nullptr));

				if (objects.count(img)) // already mentioned
					continue;

				std::string img_node_name = "node0";
				int postfix_id = 0;
				while (used_names.count(img_node_name))
				{
					img_node_name = "node" + std::to_string(postfix_id);
					++postfix_id;
				}

				objects[img] = img_node_name;
				used_names.insert(img_node_name);
			}
		}

		// optionally write description (at the moment there is no description)
		writer.writeTextElement("description", "");

		for (const auto& iNode : nodeList)
		{
			rpr_material_node node = iNode;

			rpr_material_node_type type;
			size_t input_count = 0;
			CHECK_NO_ERROR(rprMaterialNodeGetInfo(node, RPR_MATERIAL_NODE_TYPE, sizeof(rpr_material_node_type), &type, nullptr));
			CHECK_NO_ERROR(rprMaterialNodeGetInfo(node, RPR_MATERIAL_NODE_INPUT_COUNT, sizeof(size_t), &input_count, nullptr));
			writer.startElement("node");
			writer.writeAttribute("name", objects[node]);
			writer.writeAttribute("type", kMaterialTypeNames.at(type));

			for (int input_id = 0; input_id < input_count; ++input_id)
			{
				size_t read_count = 0;
				CHECK_NO_ERROR(rprMaterialNodeGetInputInfo(node, input_id, RPR_MATERIAL_NODE_INPUT_NAME_STRING, sizeof(size_t), nullptr, &read_count));
				std::vector<char> param_name(read_count, ' ');
				CHECK_NO_ERROR(rprMaterialNodeGetInputInfo(node, input_id, RPR_MATERIAL_NODE_INPUT_NAME_STRING, sizeof(param_name), param_name.data(), nullptr));

				if (!exportImageUV  // if UV not exported
					&& type == RPR_MATERIAL_NODE_IMAGE_TEXTURE
					&& param_name.size() == 3 && param_name[0] == 'u' && param_name[1] == 'v' && param_name[2] == 0
					)
				{
					continue; // ignore this parameter
				}

				std::string type = "";
				std::string value = "";
				rpr_material_node_type input_type;
				CHECK_NO_ERROR(rprMaterialNodeGetInputInfo(node, input_id, RPR_MATERIAL_NODE_INPUT_TYPE, sizeof(input_type), &input_type, &read_count));

				switch (input_type)
				{
					case RPR_MATERIAL_NODE_INPUT_TYPE_FLOAT4:
					{
						rpr_float fvalue[4] = { 0.f, 0.f, 0.f, 0.f };
						CHECK_NO_ERROR(rprMaterialNodeGetInputInfo(node, input_id, RPR_MATERIAL_NODE_INPUT_VALUE, sizeof(fvalue), fvalue, nullptr));
						//fvalue converted to string
						std::stringstream ss;
						ss << fvalue[0] << ", " <<
							fvalue[1] << ", " <<
							fvalue[2] << ", " <<
							fvalue[3];
						type = "float4";
						value = ss.str();
						break;
					}

					case RPR_MATERIAL_NODE_INPUT_TYPE_UINT:
					{
						rpr_int uivalue = RPR_SUCCESS;
						CHECK_NO_ERROR(rprMaterialNodeGetInputInfo(node, input_id, RPR_MATERIAL_NODE_INPUT_VALUE, sizeof(uivalue), &uivalue, nullptr));

						//value converted to string
						type = "uint";
						value = std::to_string(uivalue);
						break;
					}

					case RPR_MATERIAL_NODE_INPUT_TYPE_NODE:
					{
						rpr_material_node connection = nullptr;
						rpr_int res = rprMaterialNodeGetInputInfo(node, input_id, RPR_MATERIAL_NODE_INPUT_VALUE, sizeof(connection), &connection, nullptr);
						CHECK_NO_ERROR(rprMaterialNodeGetInputInfo(node, input_id, RPR_MATERIAL_NODE_INPUT_VALUE, sizeof(connection), &connection, nullptr));
						type = "connection";
						if (!objects.count(connection) && connection)
						{
							throw std::runtime_error("input material node is missing");
						}
						value = objects[connection];
						break;
					}

					case RPR_MATERIAL_NODE_INPUT_TYPE_IMAGE:
					{
						type = "connection";
						rpr_image tex = nullptr;
						CHECK_NO_ERROR(rprMaterialNodeGetInputInfo(node, input_id, RPR_MATERIAL_NODE_INPUT_VALUE, sizeof(tex), &tex, nullptr));
						size_t name_size = 0;
						CHECK_NO_ERROR(rprImageGetInfo(tex, RPR_OBJECT_NAME, NULL, nullptr, &name_size));
						std::string tex_name;
						tex_name.resize(name_size - 1);
						CHECK_NO_ERROR(rprImageGetInfo(tex, RPR_OBJECT_NAME, name_size, &tex_name[0], nullptr));

						// replace \\ by /
						// so we ensure all paths are using same convention  ( better for Unix )
						for (int i = 0; i < tex_name.length(); i++)
						{
							if (tex_name[i] == '\\')
								tex_name[i] = '/';
						}

						// we don't want path that looks like :  "H:/RPR/MatLib/MaterialLibrary/1.0 - Copy/RadeonProRMaps/Glass_Used_normal.jpg"
						// all paths must look like "RadeonProRMaps/Glass_Used_normal.jpg"
						// the path RadeonProRMaps/  is hard coded here for the moment .. needs to be exposed in exporter GUI if we change it ?
						const std::string radeonProRMapsFolder = "/RadeonProRMaps/";
						size_t pos = tex_name.find(radeonProRMapsFolder);

						if (pos != std::string::npos)
						{
							tex_name = tex_name.substr(pos + 1);
							int a = 0;
						}

						if (!textures.count(tex_name))
						{
							int tex_node_id = 0;
							std::string tex_node_name = objects[tex];
							textures[tex_name] = std::pair<std::string, rpr_image>(tex_node_name, tex);
						}

						value = textures[tex_name].first;
						break;
					}

					default:
						throw std::runtime_error("unexpected material node input type " + std::to_string(input_type));
				}

				if (!value.empty())
				{
					writer.startElement("param");
					writer.writeAttribute("name", param_name.data());
					writer.writeAttribute("type", type);
					writer.writeAttribute("value", value);
					writer.endElement();
				}
			}

			writer.endElement();
		}

		for (const auto& iNode : nodeListX)
		{
			rprx_material materialX = iNode;

			writer.startElement("node");
			writer.writeAttribute("name", objects[materialX]);
			writer.writeAttribute("type", "UBER");

			for (int iParam = 0; iParam < rprxParamList.size(); iParam++)
			{
				std::string type = "";
				std::string value = "";

				rprx_parameter_type input_type = 0;
				CHECK_NO_ERROR(rprxMaterialGetParameterType(contextX, materialX, rprxParamList[iParam].param, &input_type));

				if (input_type == RPRX_PARAMETER_TYPE_FLOAT4)
				{
					rpr_float fvalue[4] = { 0.f, 0.f, 0.f, 0.f };
					CHECK_NO_ERROR(rprxMaterialGetParameterValue(contextX, materialX, rprxParamList[iParam].param, &fvalue));

					// fvalue converted to string
					std::stringstream ss;
					ss << fvalue[0] << ", " <<
						fvalue[1] << ", " <<
						fvalue[2] << ", " <<
						fvalue[3];
					type = "float4";
					value = ss.str();
				}
				else if (input_type == RPRX_PARAMETER_TYPE_UINT)
				{
					rpr_int uivalue = RPR_SUCCESS;

					CHECK_NO_ERROR(rprxMaterialGetParameterValue(contextX, materialX, rprxParamList[iParam].param, &uivalue));

					// value converted to string
					type = "uint";
					value = std::to_string(uivalue);

				}
				else if (input_type == RPRX_PARAMETER_TYPE_NODE)
				{
					rpr_material_node connection = 0;
					CHECK_NO_ERROR(rprxMaterialGetParameterValue(contextX, materialX, rprxParamList[iParam].param, &connection));

					if (connection)
					{
						type = "connection";
						if (!objects.count(connection) && connection)
						{
							throw std::runtime_error("input material node is missing");
						}

						const auto& connectName = objects.find(connection);
						if (connectName != objects.end())
						{
							value = connectName->second;
						}
						else
						{
							// ERROR
						}
					}
				}
				else
				{
					throw std::runtime_error("unexpected material nodeX input type " + std::to_string(input_type));
				}

				if (!value.empty())
				{
					writer.startElement("param");
					writer.writeAttribute("name", rprxParamList[iParam].nameInXML);
					writer.writeAttribute("type", type);
					writer.writeAttribute("value", value);
					writer.endElement();
				}
			}

			writer.endElement();
		}

		for (const auto& tex : textures)
		{
			writer.startElement("node");
			writer.writeAttribute("name", tex.second.first);
			writer.writeAttribute("type", "INPUT_TEXTURE");
			writer.startElement("param");
			writer.writeAttribute("name", "path");
			writer.writeAttribute("type", "file_path");
			writer.writeAttribute("value", tex.first);
			writer.endElement();

			writer.startElement("param");
			writer.writeAttribute("name", "gamma");
			writer.writeAttribute("type", "float");
			bool gammaset = false;

			if (textureParameter.find(tex.second.second) != textureParameter.end())
			{
				RPR_MATERIAL_XML_EXPORT_TEXTURE_PARAM& param = textureParameter[tex.second.second];
				if (param.useGamma)
				{
					writer.writeAttribute("value", std::to_string(2.2f));
					gammaset = true;
				}
			}

			if (!gammaset)
			{
				writer.writeAttribute("value", std::to_string(1.0f));
			}

			writer.endElement();

			if (extraImageData.find(tex.second.second) != extraImageData.end())
			{
				const EXTRA_IMAGE_DATA& extra = extraImageData.at(tex.second.second);

				if (extra.scaleX != 1.0f)
				{
					writer.startElement("param");
					writer.writeAttribute("name", "tiling_u");
					writer.writeAttribute("type", "float");
					writer.writeAttribute("value", std::to_string(extra.scaleX));
					writer.endElement();
				}

				if (extra.scaleY != 1.0f)
				{
					writer.startElement("param");
					writer.writeAttribute("name", "tiling_v");
					writer.writeAttribute("type", "float");
					writer.writeAttribute("value", std::to_string(extra.scaleY));
					writer.endElement();
				}
			}

			writer.endElement();
		}

		writer.endDocument();
	}
	catch (const std::exception& ex)
	{
		std::cout << "MaterialExport error: " << ex.what() << std::endl;
	}

	return;
}

