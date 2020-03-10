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

namespace FireMaya
{
	class FireRenderPBRMaterial : public ShaderNode
	{
	public:
		static void* creator();
		static MStatus initialize();
		static MTypeId FRTypeID() { return FireMaya::TypeId::FireRenderPBRMaterial; }

		virtual MStatus compute(const MPlug& plug, MDataBlock& block) override;
		frw::Shader GetShader(Scope& scope) override;
		frw::Shader GetVolumeShader(Scope& scope) override;

	private:
		static void AddAttribute(MObject& nAttr);
		static void AddColorAttribute(MObject& attrRef, const char* name, const char* shortName, bool haveConnection, const MColor& color);
		static void AddBoolAttribute(MObject& attrRef, const char* name, const char* shortName, bool flag);
		static void AddFloatAttribute(MObject& attrRef, const char* name, const char* shortName, float min, float max, float def);

		frw::Value GetAttributeValue(const MObject& attribute, const MFnDependencyNode& shaderNode, Scope& scope);
	};
}