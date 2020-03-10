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

#include "maya/MString.h"
#include "maya/MFnDependencyNode.h"
#include "frWrap.h"

namespace FireMaya 
{ 
	class Scope; 
}

namespace MayaStandardNodeConverters
{

	struct ConverterParams
	{
		const MFnDependencyNode& shaderNode;
		const FireMaya::Scope& scope;
		const MString& outPlugName;
	};

	class BaseConverter
	{
	public:
		BaseConverter(const ConverterParams& params);
		virtual ~BaseConverter() = default;

		virtual frw::Value Convert() const = 0;

	protected:
		const ConverterParams& m_params;
	};

}
