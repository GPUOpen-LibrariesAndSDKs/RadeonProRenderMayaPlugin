#pragma once

#include "FireMaya.h"

namespace FireMaya
{
	class Noise : public ValueNode
	{
	public:
		// initialize and type info
		static MTypeId FRTypeID() { return FireMaya::TypeId::FireRenderNoise; }
		static void* creator();
		static MStatus initialize();

		frw::Value GetValue(Scope& scope) override;
	};
}


