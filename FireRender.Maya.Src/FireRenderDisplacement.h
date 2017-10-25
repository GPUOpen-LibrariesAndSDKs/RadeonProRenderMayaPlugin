#pragma once

#include "FireMaya.h"

namespace FireMaya
{
	class Displacement : public ShaderNode
	{
	public:

		enum Type {
			kDisplacement_EdgeAndCorner = 0,
			kDisplacement_EdgeOnly
		};

		static  void *  creator();
		static  MStatus initialize();
		static MTypeId FRTypeID() { return FireMaya::TypeId::FireRenderDisplacement; }

		frw::Shader GetShader(Scope& scope) override;

		bool getValues(frw::Value &map, Scope &scope, float &minHeight, float &maxHeight, int &subdivision, float &creaseWeight, int &boundary);
	};
}
