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
#include "FireRenderDot.h"

#include <maya/MFnEnumAttribute.h>

namespace
{
	namespace Attribute
	{
		MObject	uv;
		MObject	uvScale;
		MObject	output;
		MObject mapChannel;
	}
}


void* FireMaya::Dot::creator()
{
	return new FireMaya::Dot;
}


MStatus FireMaya::Dot::initialize()
{
	MFnNumericAttribute nAttr;
	MFnEnumAttribute eAttr;

	Attribute::uv = nAttr.create("uvCoord", "uv", MFnNumericData::k2Float);
	MAKE_INPUT(nAttr);
	nAttr.setSoftMin(0.1);
	nAttr.setSoftMax(100.0);

	Attribute::uvScale = nAttr.create("uvScale", "uvs", MFnNumericData::kFloat, 1);
	MAKE_INPUT(nAttr);

	Attribute::output = nAttr.createColor("out", "o");
	MAKE_OUTPUT(nAttr);


	Attribute::mapChannel = eAttr.create("mapChannel", "mc", 0);
	eAttr.addField("0", kTexture_Channel0);
	eAttr.addField("1", kTexture_Channel1);
	MAKE_INPUT_CONST(eAttr);


	CHECK_MSTATUS(addAttribute(Attribute::uv));
	CHECK_MSTATUS(addAttribute(Attribute::uvScale));
	CHECK_MSTATUS(addAttribute(Attribute::output));
	CHECK_MSTATUS(addAttribute(Attribute::mapChannel));

	CHECK_MSTATUS(attributeAffects(Attribute::uv, Attribute::output));
	CHECK_MSTATUS(attributeAffects(Attribute::uvScale, Attribute::output));
	CHECK_MSTATUS(attributeAffects(Attribute::mapChannel, Attribute::output));

	return MS::kSuccess;
}


frw::Value FireMaya::Dot::GetValue(const Scope& scope) const
{
	MFnDependencyNode shaderNode(thisMObject());

	frw::ValueNode valueNode(scope.MaterialSystem(), frw::ValueTypeDotMap);

	Type mapChannel = kTexture_Channel0;

	MPlug mapChannelPlug = shaderNode.findPlug(Attribute::mapChannel, false);
	if (!mapChannelPlug.isNull())
	{
		int n = 0;
		if (MStatus::kSuccess == mapChannelPlug.getValue(n))
		{
			mapChannel = static_cast<Type>(n);
		}
	}

	valueNode.SetValue(RPR_MATERIAL_INPUT_UV, scope.GetConnectedValue(shaderNode.findPlug(Attribute::uv, false)) | scope.MaterialSystem().ValueLookupUV(mapChannel));

	auto uvScale = scope.GetValue(shaderNode.findPlug(Attribute::uvScale, false));
	if (uvScale != 1.)
		valueNode.SetValue(RPR_MATERIAL_INPUT_UV_SCALE, uvScale);

	return valueNode;
}