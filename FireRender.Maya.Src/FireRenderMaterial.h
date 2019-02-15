#pragma once

//
// Copyright 2016 (C) AMD
//

#include "FireMaya.h"

namespace FireMaya
{
	class Material : public ShaderNode
	{
	public:

		enum Type {
			kDiffuse = 0,
			kMicrofacet,
			kMicrofacetRefract,
			kReflect,
			kRefract,
			kTransparent,
			kEmissive,
			kWard,
			kOrenNayar,
			kDiffuseRefraction,	// 1.7.16
			kPassThrough,
		};


		MStatus		compute(const MPlug&, MDataBlock&) override;
		void		postConstructor() override;

		static  void *  creator();
		static  MStatus initialize();
		static MTypeId FRTypeID() { return FireMaya::TypeId::FireRenderMaterial; }

		frw::Shader GetShader(Scope& scope) override;
	};
}