#pragma once

#include "FireRenderContext.h"
#include <maya/MPxFileTranslator.h>

namespace FireMaya
{
	// Probably we should move cleanScene call in the destructor of FireRenderContext
	// But it needs to be tested
	class ContextAutoCleaner
	{
	public:
		ContextAutoCleaner(FireRenderContext* context) : m_Context(context){}
		~ContextAutoCleaner()
		{
			if (m_Context != nullptr)
			{
				m_Context->cleanScene();
			}
		}
	private:
		FireRenderContext* m_Context;
	};
	
	class GLTFTranslator : public MPxFileTranslator
	{
	public:
		GLTFTranslator();
		~GLTFTranslator();

		static void* creator();

		virtual MStatus	writer(const MFileObject& file,
			const MString& optionsString,
			FileAccessMode mode);
		//We only support export for GLTF
		virtual bool haveWriteMethod() const { return true; }

		virtual MString defaultExtension() const { return "gltf"; }
		virtual MString filter() const { return "*.gltf"; }

		//We can't open/import GLTF files
		virtual bool canBeOpened() const { return false; }
	};
}
