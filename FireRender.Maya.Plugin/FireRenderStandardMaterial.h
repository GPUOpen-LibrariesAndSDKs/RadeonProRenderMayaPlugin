#pragma once
#include "FireMaya.h"

namespace FireMaya
{
	class StandardMaterial : public ShaderNode
	{
	public:

		virtual MStatus compute(const MPlug&, MDataBlock&);

		virtual void    postConstructor();

		static  void *  creator();
		static  MStatus initialize();
		static MTypeId FRTypeID() { return FireMaya::TypeId::FireRenderStandardMaterial; }

		virtual frw::Shader GetShader(Scope& scope) override;
		virtual frw::Shader GetVolumeShader( Scope& scope ) override;
		virtual MObject GetDisplacementNode() override;

		virtual MStatus shouldSave(const MPlug& plug, bool& isSaving);

	private:
		virtual void OnFileLoaded() override;
		void UpgradeMaterial();
	};
}