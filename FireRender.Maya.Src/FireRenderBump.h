#pragma once

#include "FireMaya.h"

namespace FireMaya
{
	class Bump : public ValueNode
	{
	public:

		enum Type {
			kTexture_Channel0 = 0,
			kTexture_Channel1
		};

		// initialize and type info
		static MTypeId FRTypeID() { return FireMaya::TypeId::FireRenderBump; }
		static void* creator();
		static MStatus initialize();

		frw::Value GetValue(Scope& scope) override;

	private:
		virtual void OnFileLoaded() override;
	};
}


