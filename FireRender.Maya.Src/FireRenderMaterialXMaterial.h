#pragma once

#include "FireMaya.h"

namespace FireMaya
{
	class MaterialXMaterial : public ShaderNode
	{
	public:
		static void* creator();
		static MStatus initialize();
		static MTypeId FRTypeID() { return FireMaya::TypeId::FireRenderMaterialXMaterial; }

		virtual MStatus compute(const MPlug&, MDataBlock&) override;
		virtual frw::Shader GetShader(Scope& scope) override;

		void postConstructor() override;

		~MaterialXMaterial();

	private:
	};
}

