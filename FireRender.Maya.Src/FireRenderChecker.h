#pragma once

#include "FireMaya.h"

namespace FireMaya
{
	class Checker : public ValueNode
	{
	public:

		enum Type {
			kTexture_Channel0 = 0,
			kTexture_Channel1
		};

		// initialize and type info
		static MTypeId FRTypeID() { return FireMaya::TypeId::FireRenderChecker; }
		static void* creator();
		static MStatus initialize();

		frw::Value GetValue(const Scope& scope) const override;
	};
}


