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
