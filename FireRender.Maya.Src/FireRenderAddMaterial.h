#pragma once

#include "FireMaya.h"

namespace FireMaya
{
	class AddMaterial : public ShaderNode
	{
	public:

		static void* creator();
		static MStatus initialize();
		static MTypeId FRTypeID() { return FireMaya::TypeId::FireRenderAddMaterial; }

		virtual MStatus compute(const MPlug& plug, MDataBlock& block) override;
		frw::Shader GetShader(Scope& scope) override;
	};
}
