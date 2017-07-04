#ifndef MATERIAL_LOADER_H_
#define MATERIAL_LOADER_H_

#include "frWrap.h"
#include <string>
#include <map>

#include <maya/MObject.h>

struct Param
{
	std::string type;
	std::string value;
};
struct MaterialNode
{
	std::string name;
	std::string type;
	std::map<std::string, Param> params;
	MObject parsedObject;
	bool parsed;
	bool root;
};

std::string getDirectory(std::string filePath);

void ExportMaterials(const std::string& filename, rpr_material_node* materials, int mat_count);
bool ImportMaterials(const std::string& filename, std::map<std::string, MaterialNode> &nodes);

extern const char* Place2dNodeName;

#endif //MATERIAL_LOADER_H_