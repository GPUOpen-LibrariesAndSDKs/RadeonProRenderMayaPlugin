#pragma once
#include "FireMaya.h"

namespace FireMaya
{
	class TransparentMaterial : public ShaderNode
	{
	public:

		virtual MStatus compute(const MPlug&, MDataBlock&);

		virtual void    postConstructor();

		static  void *  creator();
		static  MStatus initialize();
		static MTypeId FRTypeID() { return FireMaya::TypeId::FireRenderTransparentMaterial; }

		frw::Shader GetShader(Scope& scope) override;

	};
}