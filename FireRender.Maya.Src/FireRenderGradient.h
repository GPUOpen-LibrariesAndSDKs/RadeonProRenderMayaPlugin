#pragma once

#include "FireMaya.h"

namespace FireMaya
{
	class Gradient : public ValueNode
	{
	public:

		// initialize and type info
		static MTypeId FRTypeID() { return FireMaya::TypeId::FireRenderGradient; }
		static void* creator();
		static MStatus initialize();

		frw::Value GetValue(Scope& scope) override;
	};
}


