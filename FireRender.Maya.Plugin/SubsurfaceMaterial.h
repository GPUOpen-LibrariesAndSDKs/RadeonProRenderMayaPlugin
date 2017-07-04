#pragma once
#include "FireMaya.h"

namespace FireMaya
{
	class SubsurfaceMaterial : public ShaderNode
	{
	public:

		virtual MStatus compute(const MPlug&, MDataBlock&) override;

		virtual void    postConstructor() override;

		static  void *  creator();
		static  MStatus initialize();
		static MTypeId FRTypeID() { return FireMaya::TypeId::FireRenderSubsurfaceMaterial; }

		frw::Shader GetShader(Scope& scope) override;
		frw::Shader GetVolumeShader( Scope& scope ) override;
	};
}