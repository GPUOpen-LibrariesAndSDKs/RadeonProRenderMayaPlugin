#pragma once

#include "FireMaya.h"

namespace FireMaya
{
	class ToonMaterial : public ShaderNode
	{
	public:
		static  void *  creator();
		static  MStatus initialize();
		static MTypeId FRTypeID() { return FireMaya::TypeId::FireRenderToonMaterial; }

		virtual MStatus compute(const MPlug&, MDataBlock&) override;
		virtual frw::Shader GetShader(Scope& scope) override;

		void postConstructor() override;

		~ToonMaterial();

	private:
		void linkLight(Scope& scope, frw::Shader& shader);
		static void onLightAdded(MObject& node, void* clientData);
		static void onLightRemoved(MObject& node, void* clientData);
		static void onLightRenamed(MObject& node, const MString& prevName, void* clientData);

		MCallbackId nodeAddedCallback;
		MCallbackId nodeRemovedCallback;
		MCallbackId nodeRenamedCallback;
	};
}


