#pragma once

#include "FireMaya.h"

namespace FireMaya
{
	class ShadowCatcherMaterial : public ShaderNode
	{
	public:

		static void* creator();
		static MStatus initialize();
		static MTypeId FRTypeID() { return FireMaya::TypeId::FireRenderShadowCatcherMaterial; }

		virtual MStatus compute(const MPlug& plug, MDataBlock& block) override;
		frw::Shader GetShader(Scope& scope) override;
		MObject GetDisplacementNode() override;
	};
}
