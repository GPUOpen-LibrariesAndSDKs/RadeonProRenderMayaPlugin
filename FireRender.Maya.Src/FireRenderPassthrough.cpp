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
#include "FireRenderPassthrough.h"

namespace
{
	namespace Attribute
	{
		MObject	color;
		MObject	output;
	}
}


void* FireMaya::Passthrough::creator()
{
	return new FireMaya::Passthrough;
}

MStatus FireMaya::Passthrough::initialize()
{
	MFnNumericAttribute nAttr;

	Attribute::color = nAttr.createColor("color", "c");
	nAttr.setDefault(1.0f, 1.0f, 1.0f);
	MAKE_INPUT(nAttr);

	Attribute::output = nAttr.createColor("outColor", "oc");
	MAKE_OUTPUT(nAttr);

	CHECK_MSTATUS(addAttribute(Attribute::color));
	CHECK_MSTATUS(addAttribute(Attribute::output));

	CHECK_MSTATUS(attributeAffects(Attribute::color, Attribute::output));

	return MS::kSuccess;
}

frw::Shader FireMaya::Passthrough::GetShader(Scope& scope)
{
	MFnDependencyNode shaderNode(thisMObject());

	frw::Shader shader(scope.MaterialSystem(), frw::ShaderTypeFlatColor);
	shader.SetValue(RPR_MATERIAL_INPUT_COLOR, scope.GetValue(shaderNode.findPlug(Attribute::color, false)));

	return shader;
}
