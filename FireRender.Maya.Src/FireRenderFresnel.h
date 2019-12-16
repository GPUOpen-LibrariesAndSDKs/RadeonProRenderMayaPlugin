#pragma once

#include "FireMaya.h"
#include <maya/MPxShadingNodeOverride.h>

namespace FireMaya
{
	class Fresnel : public ValueNode
	{
	public:

		static MTypeId FRTypeID() { return FireMaya::TypeId::FireRenderFresnel; }
		static void* creator();
		static MStatus initialize();

		frw::Value GetValue(const Scope& scope) const override;

		class Override : public MHWRender::MPxShadingNodeOverride
		{
		public:
			Override(const MObject& obj);
			virtual ~Override();

			static MHWRender::MPxShadingNodeOverride* creator(const MObject& obj);

			virtual MHWRender::DrawAPI supportedDrawAPIs() const override;
			virtual bool allowConnections() const override;

			virtual MString fragmentName() const override;
			virtual void getCustomMappings(MHWRender::MAttributeParameterMappingList& mappings) override;

			virtual bool valueChangeRequiresFragmentRebuild(const MPlug* plug) const override;

			virtual MString outputForConnection(const MPlug& sourcePlug, const MPlug& destinationPlug) override;

			virtual void updateDG() override;
			virtual void updateShader(MHWRender::MShaderInstance& shader, const MHWRender::MAttributeParameterMappingList& mappings) override;

			static const char* className();

		private:
			MObject m_shader;
			MString m_fragmentName;
			float m_ior;
		};
	};
}
