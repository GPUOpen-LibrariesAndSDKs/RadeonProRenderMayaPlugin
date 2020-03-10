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
	class Displacement : public ShaderNode
	{
	public:

		enum Type {
			kDisplacement_EdgeAndCorner = 0,
			kDisplacement_EdgeOnly
		};

		static  void *  creator();
		static  MStatus initialize();
		static MTypeId FRTypeID() { return FireMaya::TypeId::FireRenderDisplacement; }

		frw::Shader GetShader(Scope& scope) override;

		struct DisplacementParams
		{
			frw::Value map;
			float minHeight;
			float maxHeight;
			int subdivision;
			float creaseWeight;
			int boundary;
			bool isAdaptive;
			float adaptiveFactor;

			DisplacementParams()
				: minHeight(0.0f)
				, maxHeight(0.0f)
				, subdivision(1)
				, creaseWeight(0.0f)
				, boundary(RPR_SUBDIV_BOUNDARY_INTERFOP_TYPE_EDGE_ONLY)
				, isAdaptive(false)
				, adaptiveFactor(1.0f)
			{}
		};

		bool getValues(Scope& scope, DisplacementParams& params);
	};
}
