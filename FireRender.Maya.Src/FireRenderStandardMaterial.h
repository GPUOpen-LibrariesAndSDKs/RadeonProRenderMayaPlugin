/**********************************************************************
Copyright 2020 Advanced Micro Devices, Inc
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
********************************************************************/
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
