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
			NODE_ENTRY(ShaderTypeWard),
			NODE_ENTRY(ShaderTypeBlend),
#if (RPR_VERSION_MINOR < 34)
			NODE_ENTRY(ShaderTypeStandard),
#endif
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
}

inline void frw::Object::Data::Init(void* h, const Context& c, bool destroy)
{
	handle = h;
	context = c.m;
	destroyOnDelete = destroy;
	if (h)
	{
		allocatedObjects++;
#if FRW_LOGGING
		typeNameMirror = GetTypeName();
#endif
		FRW_PRINT_DEBUG("\tFR+ %s 0x%016llX%s (%d total)", GetTypeName(), h, destroyOnDelete ? "" : "*", allocatedObjects);
	}
}

inline frw::Object::Data::~Data()
{
	if (handle)
	{
		// Clear references before destroying the object itself. For example, Scene has referenced Shapes,
		// and these shapes better to be destroyed before the scene itself.
		references.clear();

		if (destroyOnDelete)
		{
			DeleteAndClear();
		}

		allocatedObjects--;

		// Can't use virtual GetTypeName() in destructor, so using "mirrored" type name
		FRW_PRINT_DEBUG("\tFR- %s 0x%016llX%s (%d total)", typeNameMirror, handle, destroyOnDelete ? "" : "*", allocatedObjects);
	}
}

inline void frw::Object::Data::Attach(void* h, bool destroy)
{
	if (handle)
	{
		if (destroyOnDelete)
		{
			DeleteAndClear();
		}

		allocatedObjects--;

		FRW_PRINT_DEBUG("\tFR- %s 0x%016llX%s (%d total)", GetTypeName(), handle, destroyOnDelete ? "" : "*", allocatedObjects);
	}

	if (h)
	{
		destroyOnDelete = destroy;
		handle = h;
		allocatedObjects++;

#if FRW_LOGGING
		typeNameMirror = GetTypeName();
#endif
		FRW_PRINT_DEBUG("\tFR+ %s 0x%016llX%s (%d total)", GetTypeName(), h, destroyOnDelete ? "" : "*", allocatedObjects);
	}
}