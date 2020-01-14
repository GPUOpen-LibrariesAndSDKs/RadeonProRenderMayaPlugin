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


void frw::Shader::SetShadowCatcher(bool isShadowCatcher)
{
	data().isShadowCatcher = isShadowCatcher;
}

bool frw::Shader::IsShadowCatcher() const
{
	return data().isShadowCatcher;
}

void frw::Shader::SetShadowColor(float r, float g, float b, float a)
{
	data().mShadowCatcherParams.mShadowR = r;
	data().mShadowCatcherParams.mShadowG = g;
	data().mShadowCatcherParams.mShadowB = b;
	data().mShadowCatcherParams.mShadowA = a;
}
void frw::Shader::GetShadowColor(float* r, float* g, float* b, float* a) const
{
	*r = data().mShadowCatcherParams.mShadowR;
	*g = data().mShadowCatcherParams.mShadowG;
	*b = data().mShadowCatcherParams.mShadowB;
	*a = data().mShadowCatcherParams.mShadowA;
}

void frw::Shader::SetShadowWeight(float w) 
{ 
	data().mShadowCatcherParams.mShadowWeight = w; 
}

float frw::Shader::GetShadowWeight() const 
{ 
	return data().mShadowCatcherParams.mShadowWeight; 
}

void frw::Shader::SetBackgroundIsEnvironment(bool bgIsEnv) 
{ 
	data().mShadowCatcherParams.mBgIsEnv = bgIsEnv; 
}

bool frw::Shader::BgIsEnv() const 
{ 
	return data().mShadowCatcherParams.mBgIsEnv; 
}

void frw::Shader::ClearDependencies(void) 
{ 
	data().ClearDependencies(); 
}

void frw::Shader::SetReflectionCatcher(bool isReflectionCatcher) 
{ 
	data().isReflectionCatcher = isReflectionCatcher; 
}

bool frw::Shader::IsReflectionCatcher(void) const 
{ 
	return data().isReflectionCatcher; 
}

frw::Shader::Shader(DataPtr p)
{
	m = p;
}

frw::Shader::Shader(const MaterialSystem& ms, ShaderType type, bool destroyOnDelete) : Node(ms, type, destroyOnDelete, new Data())
{
	data().shaderType = type;
}

frw::Shader::Shader(const MaterialSystem& ms, const Context& context) : Node(ms, ShaderType::ShaderTypeStandard, false, new Data())
{
	data().shaderType = ShaderType::ShaderTypeStandard;
}

void frw::Shader::_SetInputNode(rpr_material_node_input key, const Shader& shader)
{
	Node::_SetInputNode(key, shader);

	if (shader)
	{
		AddDependentShader(shader);
	}
}

void frw::Shader::AddDependentShader(frw::Shader shader)
{
	data().dependentShaders.push_back(shader);
}

frw::ShaderType frw::Shader::GetShaderType() const
{
	if (!IsValid())
		return ShaderTypeInvalid;
	return data().shaderType;
}

void frw::Shader::SetDirty(bool value)
{
	data().bDirty = value;

	if (value)
	{
		data().SetCommittedState(false);
	}
}

bool frw::Shader::IsDirty() const
{
	return data().bDirty;
}

void frw::Shader::xMaterialCommit() const
{
	Data& d = data();

	if (Handle())
	{
		try
		{
			if (d.IsCommitted())
			{
				return;
			}
			d.SetCommittedState(true);
		}
		catch (...)
		{
			DebugPrint("Failed to rprx commit: Context=0x%016llX x_material=0x%016llX", d.context, Handle());

			throw;
		}
	}
}

void frw::Shader::AttachToShape(Shape::Data& shape)
{
	Data& d = data();
	d.numAttachedShapes++;
	rpr_int res;
	if (Handle())
	{
		FRW_PRINT_DEBUG("\tShape.AttachMaterial: d: 0x%016llX - numAttachedShapes: %d shape=0x%016llX x_material=0x%016llX", &d, d.numAttachedShapes, shape.Handle(), Handle());
		res = rprShapeSetMaterial(shape.Handle(), Handle());
		checkStatus(res);

		if (d.isShadowCatcher)
		{
			res = rprShapeSetShadowCatcher(shape.Handle(), true);
			if (res != RPR_ERROR_UNSUPPORTED)
			{
				checkStatus(res);
			}
		}

		if (d.isReflectionCatcher)
		{
			res = rprShapeSetReflectionCatcher(shape.Handle(), true);
			checkStatus(res);
		}
	}
	else
	{
		FRW_PRINT_DEBUG("\tShape.AttachMaterial: d: 0x%016llX - numAttachedShapes: %d shape=0x%016llX material=0x%016llX", d, d.numAttachedShapes, shape.Handle(), d.Handle());
		res = rprShapeSetMaterial(shape.Handle(), d.Handle());
		checkStatus(res);
	}

	d.SetCommittedState(false);
}
void frw::Shader::DetachFromShape(Shape::Data& shape)
{
	Data& d = data();
	d.numAttachedShapes--;
	FRW_PRINT_DEBUG("\tShape.DetachMaterial: d: 0x%016llX - numAttachedShapes: %d shape=0x%016llX, material=0x%016llX", &d, d.numAttachedShapes, shape.Handle(), Handle());
	if (Handle())
	{
		rpr_int res = rprShapeSetMaterial(shape.Handle(), nullptr);
		checkStatus(res);

		if (d.isShadowCatcher)
		{
			res = rprShapeSetShadowCatcher(shape.Handle(), false);

			if (res != RPR_ERROR_UNSUPPORTED)
			{
				checkStatus(res);
			}
		}
	}
	//			else -- RPR 1.262 crashes when changing any RPRX parameter (when creating new material and replacing old one); mirroring call to rpr API fixes that
	{
		rpr_int res = rprShapeSetMaterial(shape.Handle(), nullptr);
		checkStatus(res);
	}
}

void frw::Shader::AttachToCurve(frw::Curve::Data& crv)
{
	Data& d = data();
	d.numAttachedShapes++;

	if (Handle())
	{
		FRW_PRINT_DEBUG("\tShape.AttachMaterial: d: 0x%016llX - numAttachedShapes: %d shape=0x%016llX x_material=0x%016llX", &d, d.numAttachedShapes, shape.Handle(), Handle());
		rpr_int res;
		res = rprCurveSetMaterial(crv.Handle(), Handle());
		checkStatus(res);
	}
	else
	{
		FRW_PRINT_DEBUG("\tShape.AttachMaterial: d: 0x%016llX - numAttachedShapes: %d shape=0x%016llX material=0x%016llX", d, d.numAttachedShapes, shape.Handle(), d.Handle());
		rpr_int res;
		res = rprCurveSetMaterial(crv.Handle(), Handle());
		checkStatus(res);
	}

	d.SetCommittedState(false);
}

void frw::Shader::DetachFromCurve(frw::Curve::Data& crv)
{
	Data& d = data();
	d.numAttachedShapes--;
	FRW_PRINT_DEBUG("\tShape.DetachMaterial: d: 0x%016llX - numAttachedShapes: %d shape=0x%016llX, material=0x%016llX", &d, d.numAttachedShapes, shape.Handle(), Handle());

	if (Handle())
	{
		rpr_int res;
		res = rprCurveSetMaterial(crv.Handle(), nullptr);
		checkStatus(res);
	}

	{
		rpr_int res;
		res = rprCurveSetMaterial(crv.Handle(), nullptr);
		checkStatus(res);
	}
}

void frw::Shader::Commit()
{
	Data& d = data();
	rpr_int res = RPR_SUCCESS;

	if (d.IsCommitted())
	{
		return;
	}

	if (!d.context || !Handle())
	{
		return;
	}

	d.SetCommittedState(true);
	checkStatus(res);
}

void frw::Shader::AttachToMaterialInput(rpr_material_node node, rpr_material_node_input inputKey) const
{
	auto& d = data();
	rpr_int res;
	FRW_PRINT_DEBUG("\tShape.AttachToMaterialInput: node=0x%016llX, material=0x%016llX on %s", node, Handle(), inputKey);
	if (Handle())
	{
		// Attach rpr shader output to some material's input
		auto it = d.inputs.find(inputKey);
		if (it != d.inputs.end())
			DetachFromMaterialInput(it->second, it->first);

		res = rprMaterialNodeSetInputNByKey(node, inputKey, Handle());
		checkStatus(res);
		d.inputs.emplace(inputKey, node);
	}
	else
	{
		res = rprMaterialNodeSetInputNByKey(node, inputKey, d.Handle());
	}
	checkStatus(res);
}

void frw::Shader::DetachFromAllMaterialInputs()
{
	auto& d = data();
	rpr_int res = RPR_ERROR_INVALID_PARAMETER;
	for (const auto& input : d.inputs)
	{
		res = rprMaterialNodeSetInputNByKey(input.second, input.first, nullptr);
		checkStatus(res);
	}
	d.inputs.clear();
}

void frw::Shader::DetachFromMaterialInput(rpr_material_node node, rpr_material_node_input inputKey) const
{
	auto& d = data();
	rpr_int res = RPR_ERROR_INVALID_PARAMETER;
	FRW_PRINT_DEBUG("\tShape.DetachFromMaterialInput: node=0x%016llX, material=0x%016llX on %s", node, Handle(), inputKey);
	if (Handle())
	{
		// Detach rpr shader output from some material's input
		res = rprMaterialNodeSetInputNByKey(node, inputKey, nullptr);
		d.inputs.erase(inputKey);
	}
	checkStatus(res);
}

void frw::Shader::xSetParameterN(rpr_material_node_input parameter, rpr_material_node node)
{
	const Data& d = data();
	rpr_int res = rprMaterialNodeSetInputNByKey(Handle(), parameter, node);

	if (res == RPR_ERROR_UNSUPPORTED ||
		res == RPR_ERROR_INVALID_PARAMETER)
	{
		// print error/warning if needed
	}
	else
	{
		checkStatus(res);
	}
}
void frw::Shader::xSetParameterU(rpr_material_node_input parameter, rpr_uint value)
{
	const Data& d = data();
	rpr_int res = rprMaterialNodeSetInputUByKey(Handle(), parameter, value);
	if (res == RPR_ERROR_UNSUPPORTED ||
		res == RPR_ERROR_INVALID_PARAMETER)
	{
		// print error/warning if needed
	}
	else
	{
		checkStatus(res);
	}

}
void frw::Shader::xSetParameterF(rpr_material_node_input parameter, rpr_float x, rpr_float y, rpr_float z, rpr_float w)
{
	const Data& d = data();
	rpr_int res = rprMaterialNodeSetInputFByKey(Handle(), parameter, x, y, z, w);
	if (res == RPR_ERROR_UNSUPPORTED ||
		res == RPR_ERROR_INVALID_PARAMETER)
	{
		// print error/warning if needed
	}
	else
	{
		checkStatus(res);
	}
}

bool frw::Shader::xSetValue(rpr_material_node_input parameter, const Value& v)
{
	switch (v.type)
	{
		case Value::FLOAT:
			xSetParameterF(parameter, v.x, v.y, v.z, v.w);
			return true;
		case Value::NODE:
		{
			if (!v.node)	// in theory we should now allow this, as setting a NULL input is legal (as of FRSDK 1.87)
				return false;
			AddReference(v.node);
			xSetParameterN(parameter, v.node.Handle());	// should be ok to set null here now
			return true;
		}
	}
	assert(!"bad type");
	return false;
}