#pragma once

#include "FireMaya.h"

namespace FireMaya
{
	class Lookup : public ValueNode
	{
	public:

		// initialize and type info
		static MTypeId FRTypeID() { return FireMaya::TypeId::FireRenderLookup; }
		static void* creator();
		static MStatus initialize();

		frw::Value GetValue(Scope& scope) override;

	private:
		frw::Value CombineVertexColor(const frw::MaterialSystem& materialSystem) const;
	};
}
