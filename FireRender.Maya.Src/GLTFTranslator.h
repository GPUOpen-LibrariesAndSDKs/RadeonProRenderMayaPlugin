#pragma once

#include <maya/MPxFileTranslator.h>

namespace FireMaya
{
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
