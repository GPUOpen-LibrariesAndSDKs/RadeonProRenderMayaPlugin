#pragma once

#include "FireMaya.h"

namespace FireMaya
{
	class Texture : public ValueNode
	{
	public:

		enum Type {
			kTexture_Channel0 = 0,
			kTexture_Channel1
		};

		// initialize and type info
		static MTypeId FRTypeID() { return FireMaya::TypeId::FireRenderTexture; }
		static void* creator();
		static MStatus initialize();

		frw::Value GetValue(const Scope& scope) const override;

		MString getMapPath();
	};
}


