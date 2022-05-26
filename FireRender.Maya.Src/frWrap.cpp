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
#include <GL/glew.h>
#include <iostream>
#include "frWrap.h"

namespace frw
{
	Value Value::unknown = Value(1, 0, 1);

	FrameBuffer::FrameBuffer(const Context& context, rpr_GLuint* glTextureId) :
		Object(nullptr, context)
	{
		FRW_PRINT_DEBUG("CreateGLInteropFrameBuffer()");
		rpr_framebuffer h = nullptr;
		auto res = rprContextCreateFramebufferFromGLTexture2D(context.Handle(), GL_TEXTURE_2D, 0, *glTextureId, &h);
		checkStatusThrow(res);
		m->Attach(h);
	}

	Value Value::Inverse() const
	{
		return Value(1) / *this;
	}

	template <typename... Args>
	inline void DebugPrint(const char* format, const Args&... args)
	{
#ifdef _DEBUG
#ifdef _WIN32
		static char buf[0x10000] = {};
		sprintf_s(buf, 0x10000, format, args...);
		OutputDebugStringA(buf);
#elif defined(__linux__)
		static char buf[0x10000] = {};
		snprintf(buf, 0x10000, format, args...);
		std::clog << buf;
#else
		// other platforms?
#endif

#endif
	}

#define NODE_ENTRY(a) { frw::a, #a }

	void Context::DumpParameterInfo()
	{
		int stackSize = GetMaterialStackSize();

		DebugPrint("Radeon ProRender Context Information\n");
		DebugPrint("\tMaterial Stack Size: %d\n", stackSize);
		DebugPrint("\tContext Parameters: \n");

		for (int i = 0, n = GetParameterCount(); i < n; i++)
		{
			auto info = GetParameterInfo(i);
			DebugPrint("\t + %s; id 0x%X; type 0x%X; // %s\n", info.name.c_str(), info.id, info.type, info.description.c_str());
		}

		MaterialSystem ms(*this);

		struct { int type; const char* typeName; } nodeTypes[] =
		{
			NODE_ENTRY(ValueTypeArithmetic),
			NODE_ENTRY(ValueTypeFresnel),
			NODE_ENTRY(ValueTypeNormalMap),
			NODE_ENTRY(ValueTypeBumpMap),
			NODE_ENTRY(ValueTypeImageMap),
			NODE_ENTRY(ValueTypeNoiseMap),
			NODE_ENTRY(ValueTypeDotMap),
			NODE_ENTRY(ValueTypeGradientMap),
			NODE_ENTRY(ValueTypeCheckerMap),
			NODE_ENTRY(ValueTypeConstant),
			NODE_ENTRY(ValueTypeLookup),
			NODE_ENTRY(ValueTypeBlend),
			NODE_ENTRY(ValueTypeFresnelSchlick),
			NODE_ENTRY(ValueTypePassthrough),

			NODE_ENTRY(ShaderTypeDiffuse),
			NODE_ENTRY(ShaderTypeMicrofacet),
			NODE_ENTRY(ShaderTypeReflection),
			NODE_ENTRY(ShaderTypeRefraction),
			NODE_ENTRY(ShaderTypeMicrofacetRefraction),
			NODE_ENTRY(ShaderTypeTransparent),
			NODE_ENTRY(ShaderTypeEmissive),
			NODE_ENTRY(ShaderTypeBlend),
			NODE_ENTRY(ShaderTypeOrenNayer),
			NODE_ENTRY(ShaderTypeDiffuseRefraction),
			NODE_ENTRY(ShaderTypeAdd),
			NODE_ENTRY(ShaderTypeVolume),
		};

		DebugPrint("Material Nodes: \n");
		for (auto it : nodeTypes)
		{
			if (auto node = frw::Node(ms, it.type))
			{
				DebugPrint("\t\tMaterial Node %s (0x%X):\n", it.typeName, it.type);
				for (int i = 0, n = node.GetParameterCount(); i < n; i++)
				{
					auto info = node.GetParameterInfo(i);
					DebugPrint("\t\t + %s; id 0x%X; type 0x%X;\n", info.name.c_str(), info.id, info.type);
				}
			}
		}
	}

	void UVProceduralNode::SetOrigin(const frw::Value& value)
	{
		SetValue(NodeInputOrigin, value);
	}

	void UVTriplanarNode::SetOrigin(const frw::Value& value)
	{
		SetValue(NodeInputOffsset, value);
	}

	void BaseUVNode::SetInputZAxis(const frw::Value& value)
	{
		SetValue(NodeInputZAxis, value);
	}

	void BaseUVNode::SetInputXAxis(const frw::Value& value)
	{
		SetValue(NodeInputXAxis, value);
	}

	void BaseUVNode::SetInputUVScale(const frw::Value& value)
	{
		SetValue(NodeInputUVScale, value);
	}

	void Node::_SetInputNode(rpr_material_node_input key, const Shader& shader)
	{
		if (shader)
		{
			AddReference(shader);
			shader.AttachToMaterialInput(Handle(), key);
		}
	}
}
