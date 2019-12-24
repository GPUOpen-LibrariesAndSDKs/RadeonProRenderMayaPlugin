#pragma once

#include "FireMaya.h"

namespace FireMaya
{
	class FresnelSchlick : public ValueNode
	{
	public:

		static MTypeId FRTypeID() { return FireMaya::TypeId::FireRenderFresnelSchlick; }
		static void* creator();
		static MStatus initialize();

		frw::Value GetValue(const Scope& scope) const override;
	};
}
