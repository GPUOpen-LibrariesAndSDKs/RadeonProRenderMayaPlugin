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

	bool IsUber (void) const { return type == "UBER"; }
	bool IsBlend(void) const { return type == "BLEND"; }
	bool IsDiffuse(void) const { return type == "DIFFUSE"; }
	bool IsReflective(void) const { return type == "REFLECTION"; }
	bool IsSupportedMaterial(void) const { return IsBlend() || IsDiffuse() || IsReflective(); }
	std::string GetName(void) const;
	typedef std::vector<std::pair<std::string, std::string>> ConnectionsList;
	ConnectionsList GetConnections (void) const;
};

std::string getDirectory(std::string filePath);

void ExportMaterials(const std::string& filename, rpr_material_node* materials, int mat_count);
bool ImportMaterials(const std::string& filename, std::map<std::string, MaterialNode> &nodes, std::string& materialName);

extern const char* Place2dNodeName;

#endif //MATERIAL_LOADER_H_