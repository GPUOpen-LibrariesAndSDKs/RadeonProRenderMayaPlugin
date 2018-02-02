#pragma once

/*********************************************************************************************************************************
* Radeon ProRender for plugins
* Copyright (c) 2018 AMD
* All Rights Reserved
*
* FireRenderPBRMaterial class header.
*********************************************************************************************************************************/

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