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
	class Bump : public ValueNode
	{
	public:

		enum Type {
			kTexture_Channel0 = 0,
			kTexture_Channel1
		};

		// initialize and type info
		static MTypeId FRTypeID() { return FireMaya::TypeId::FireRenderBump; }
		static void* creator();
		static MStatus initialize();

		frw::Value GetValue(const Scope& scope) const override;

	private:
		virtual void OnFileLoaded() override;
	};
}


