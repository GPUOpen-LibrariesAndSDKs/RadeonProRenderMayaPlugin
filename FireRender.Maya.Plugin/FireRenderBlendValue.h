#pragma once

#include "FireMaya.h"

namespace FireMaya
{
	class BlendValue : public ValueNode
	{
	public:

		// initialize and type info
		static MTypeId FRTypeID() { return FireMaya::TypeId::FireRenderBlendValue; }
		static void* creator();
		static MStatus initialize();

		frw::Value GetValue(Scope& scope) override;
	};
}
