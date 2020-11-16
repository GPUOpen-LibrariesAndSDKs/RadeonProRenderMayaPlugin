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


#include <maya/MPxFileTranslator.h>

#include "Context/FireRenderContext.h"
#include "RenderProgressBars.h"

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

	private:
		RenderProgressBars* m_progressBars;
	};
}
