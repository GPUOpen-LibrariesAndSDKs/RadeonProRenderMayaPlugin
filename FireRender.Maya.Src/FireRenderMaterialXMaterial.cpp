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
#include "FireRenderMaterialXMaterial.h"
#include "FireMaya.h"
#include "FireRenderUtils.h"
#include "Context/FireRenderContext.h"
#include "RadeonProRender_MaterialX.h"

#include <maya/MFnNumericAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MColor.h>
#include <maya/MDGMessage.h>

#include <filesystem>

namespace
{
	namespace Attribute
	{
		MObject output;

		// path to materialX file
		MObject filename;

		// uv input for materialX textures used
		MObject	uv;
		MObject	uvSize;
	}
}

// Register attribute and make it affecting output color and alpha
#define ADD_ATTRIBUTE(attr) \
	CHECK_MSTATUS(addAttribute(attr)); \
	CHECK_MSTATUS(attributeAffects(attr, Attribute::output));

MStatus FireMaya::MaterialXMaterial::initialize()
{
	MFnNumericAttribute nAttr;
	MFnTypedAttribute tAttr;

	Attribute::output = nAttr.createColor("outColor", "oc");
	MAKE_OUTPUT(nAttr);

	Attribute::filename = tAttr.create("filename", "fnm", MFnData::kString);
	tAttr.setUsedAsFilename(true);
	MAKE_INPUT_CONST(tAttr);

	Attribute::uv = nAttr.create("uvCoord", "uv", MFnNumericData::k2Float);
	MAKE_INPUT(nAttr);
	// Note: we are not using this, but we have to have it to support classification of this node as a texture2D in Maya:
	Attribute::uvSize = nAttr.create("uvFilterSize", "fs", MFnNumericData::k2Float);
	MAKE_INPUT(nAttr);

	// Adding all attributes to the node type
	addAttribute(Attribute::output);

	ADD_ATTRIBUTE(Attribute::filename);
	ADD_ATTRIBUTE(Attribute::uv);
	ADD_ATTRIBUTE(Attribute::uvSize);

	return MS::kSuccess;
}

void* FireMaya::MaterialXMaterial::creator()
{
	return new MaterialXMaterial;
}

MStatus FireMaya::MaterialXMaterial::compute(const MPlug& plug, MDataBlock& block)
{
	if ((plug == Attribute::output) || (plug.parent() == Attribute::output))
	{
		block.setClean(plug);

		return MS::kSuccess;
	}
	else
	{
		return MS::kUnknownParameter;
	}
}

frw::Shader FireMaya::MaterialXMaterial::GetShader(Scope& scope)
{
	MStatus status;
	MFnDependencyNode shaderNode(thisMObject());

	frw::Shader shader(scope.MaterialSystem(), frw::ShaderTypeMaterialX);

	MPlug filePlug = shaderNode.findPlug(Attribute::filename, false, &status);
	assert(status == MStatus::kSuccess);
	std::string strFilePath = filePlug.asString(&status).asChar();
	assert(status == MStatus::kSuccess);

	if (!std::filesystem::exists(strFilePath))
	{
		return shader;
	}

	std::filesystem::path filePath = strFilePath;
	if (filePath.extension() != ".mtlx")
	{
		return shader;
	}

	// using direct RPR call here because this is a very special case 
	rpr_int res = rprMaterialXSetFile(shader.Handle(), strFilePath.c_str());
	checkStatus(res);

	return shader;
}

void FireMaya::MaterialXMaterial::postConstructor()
{
	ShaderNode::postConstructor();
}

FireMaya::MaterialXMaterial::~MaterialXMaterial()
{
}

