#pragma once

#include "FireMaya.h"

namespace FireMaya
{
	class Arithmetic : public ValueNode
	{
	public:

		// initialize and type info
		static MTypeId FRTypeID() { return FireMaya::TypeId::FireRenderArithmetic; }
		static void* creator();
		static MStatus initialize();

		frw::Value GetValue(const Scope& scope) const override;
	};
}
