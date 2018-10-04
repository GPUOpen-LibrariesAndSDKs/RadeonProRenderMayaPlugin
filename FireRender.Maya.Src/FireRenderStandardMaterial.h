#pragma once
#include "FireMaya.h"
#include <maya/MFnDependencyNode.h>

namespace FireMaya
{
	class StandardMaterial : public ShaderNode
	{
	public:
		virtual MStatus compute(const MPlug&, MDataBlock&) override;

		virtual void    postConstructor() override;

		static  void *  creator();
		static  MStatus initialize();
		static MTypeId FRTypeID() { return FireMaya::TypeId::FireRenderStandardMaterial; }

		virtual frw::Shader GetShader(Scope& scope) override;
		virtual frw::Shader GetVolumeShader( Scope& scope ) override;
		virtual MObject GetDisplacementNode() override;

		virtual MStatus shouldSave(const MPlug& plug, bool& isSaving) override;

	private:
		virtual void OnFileLoaded() override;
		void UpgradeMaterial();

		struct NormalMapParams
		{
			static const unsigned int MATERIAL_INVALID_PARAM = 0xFFFF;

			Scope& scope;
			MFnDependencyNode& shaderNode;
			MObject attrUseCommonNormalMap;
			MObject mapPlug;
			unsigned int param;
			frw::Shader& material;

			NormalMapParams(Scope& _scope, frw::Shader& _material, MFnDependencyNode& _shaderNode);

			bool IsValid(void) const;
		};

		void ApplyNormalMap(NormalMapParams& params);
		bool IsNormalOrBumpMap(const MObject& attrNormal, NormalMapParams& params) const;
	};
}
