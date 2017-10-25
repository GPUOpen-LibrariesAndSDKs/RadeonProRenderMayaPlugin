#pragma once

#include "FireMaya.h"
#include <maya/MPxSurfaceShadingNodeOverride.h>

namespace FireMaya
{
	class BlendMaterial : public ShaderNode
	{
	public:

		static void* creator();
		static MStatus initialize();
		static MTypeId FRTypeID() { return FireMaya::TypeId::FireRenderBlendMaterial; }

		virtual MStatus compute(const MPlug& plug, MDataBlock& block);
		frw::Shader GetShader(Scope& scope) override;
		frw::Shader GetVolumeShader(Scope& scope) override;

		class Override : public MHWRender::MPxSurfaceShadingNodeOverride
		{
		public:
			Override(const MObject& obj);
			virtual ~Override();

			static MHWRender::MPxSurfaceShadingNodeOverride* creator(const MObject& obj);

			virtual MHWRender::DrawAPI supportedDrawAPIs() const override;
			virtual bool allowConnections() const override;

			virtual MString fragmentName() const override;
			virtual void getCustomMappings(MHWRender::MAttributeParameterMappingList& mappings) override;

			virtual bool valueChangeRequiresFragmentRebuild(const MPlug* plug) const override;

			virtual void updateDG() override;
			virtual void updateShader(MHWRender::MShaderInstance& shader, const MHWRender::MAttributeParameterMappingList& mappings) override;

			static const char* className();

		private:
			MObject m_shader;
		};
	};
}
