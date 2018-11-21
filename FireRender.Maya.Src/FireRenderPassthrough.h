#pragma once

#include "FireMaya.h"

namespace FireMaya
{
	class Passthrough : public ShaderNode
	{
	public:
		// initialize and type info
		static MTypeId FRTypeID() { return FireMaya::TypeId::FireRenderPassthrough; }
		static void* creator();
		static MStatus initialize();

		frw::Shader GetShader(Scope& scope) override;
	};
}


