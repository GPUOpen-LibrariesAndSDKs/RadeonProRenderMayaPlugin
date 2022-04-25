/**********************************************************************
Copyright 2021 Advanced Micro Devices, Inc
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
#include "FireRenderVoronoi.h"

#include <maya/MFnEnumAttribute.h>

namespace
{
	namespace Attribute
	{
		MObject	uv;
		MObject uvSize;
		MObject scale;
		MObject randomness;
		MObject dimension;
		MObject	output;
		MObject mapChannel;
		MObject outType;
	}
}

void* FireMaya::Voronoi::creator()
{
	return new FireMaya::Voronoi;
}

MStatus FireMaya::Voronoi::initialize()
{
	MFnNumericAttribute nAttr;
	MFnEnumAttribute eAttr;

	Attribute::uv = nAttr.create("uvCoord", "uv", MFnNumericData::k2Float);
	MAKE_INPUT(nAttr);

	// Note: we are not using this, but we have to have it to support classification of this node as a texture2D in Maya:
	Attribute::uvSize = nAttr.create("uvFilterSize", "fs", MFnNumericData::k2Float);
	MAKE_INPUT(nAttr);

	Attribute::scale = nAttr.create("scale", "s", MFnNumericData::kFloat, 1);
	MAKE_INPUT(nAttr);

	Attribute::randomness = nAttr.create("randomness", "r", MFnNumericData::kFloat, 1);
	MAKE_INPUT(nAttr);
	nAttr.setMax(1.0);
	nAttr.setMin(0.0);

	Attribute::output = nAttr.createColor("out", "o");
	MAKE_OUTPUT(nAttr);

	Attribute::mapChannel = eAttr.create("mapChannel", "mc", 0);
	eAttr.addField("0", kTexture_Channel0);
	eAttr.addField("1", kTexture_Channel1);
	MAKE_INPUT_CONST(eAttr);

	Attribute::dimension = eAttr.create("dimensions", "d", 0);
	eAttr.addField("2D", 2);
	eAttr.addField("3D", 3);
	MAKE_INPUT_CONST(eAttr);

	Attribute::outType = eAttr.create("outType", "ot", RPR_VORONOI_OUT_TYPE_COLOR);
	eAttr.addField("Color", RPR_VORONOI_OUT_TYPE_COLOR);
	eAttr.addField("Distance", RPR_VORONOI_OUT_TYPE_DISTANCE);
	eAttr.addField("Position", RPR_VORONOI_OUT_TYPE_POSITION);
	MAKE_INPUT_CONST(eAttr);

	CHECK_MSTATUS(addAttribute(Attribute::uv));
	CHECK_MSTATUS(addAttribute(Attribute::uvSize));
	CHECK_MSTATUS(addAttribute(Attribute::scale));
	CHECK_MSTATUS(addAttribute(Attribute::randomness));
	CHECK_MSTATUS(addAttribute(Attribute::output));
	CHECK_MSTATUS(addAttribute(Attribute::mapChannel));
	CHECK_MSTATUS(addAttribute(Attribute::dimension));
	CHECK_MSTATUS(addAttribute(Attribute::outType));

	CHECK_MSTATUS(attributeAffects(Attribute::uv, Attribute::output));
	CHECK_MSTATUS(attributeAffects(Attribute::uvSize, Attribute::output));
	CHECK_MSTATUS(attributeAffects(Attribute::scale, Attribute::output));
	CHECK_MSTATUS(attributeAffects(Attribute::randomness, Attribute::output));
	CHECK_MSTATUS(attributeAffects(Attribute::mapChannel, Attribute::output));
	CHECK_MSTATUS(attributeAffects(Attribute::dimension, Attribute::output));
	CHECK_MSTATUS(attributeAffects(Attribute::outType, Attribute::output));

	return MS::kSuccess;
}

frw::Value FireMaya::Voronoi::GetValue(const Scope& scope) const
{
	MFnDependencyNode shaderNode(thisMObject());

	frw::ValueNode valueNode(scope.MaterialSystem(), frw::ValueTypeVoronoiMap);

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

	auto uv = scope.GetConnectedValue(shaderNode.findPlug("uvCoord", false)) | scope.MaterialSystem().ValueLookupUV(mapChannel);
	valueNode.SetValue(RPR_MATERIAL_INPUT_UV, uv);

	const IFireRenderContextInfo* ctxInfo = scope.GetIContextInfo();
	assert(ctxInfo);
	if (ctxInfo->IsUberScaleSupported())
	{
		auto scale = scope.GetValue(shaderNode.findPlug(Attribute::scale, false));
		valueNode.SetValue(RPR_MATERIAL_INPUT_SCALE, scale);
	}

	auto randomness = scope.GetValue(shaderNode.findPlug(Attribute::randomness, false));
	valueNode.SetValue(RPR_MATERIAL_INPUT_RANDOMNESS, randomness);

	auto dimension = scope.GetValue(shaderNode.findPlug(Attribute::dimension, false));
	valueNode.SetValue(RPR_MATERIAL_INPUT_DIMENSION, dimension);

	auto outType = scope.GetValue(shaderNode.findPlug(Attribute::outType, false));
	valueNode.SetValueInt(RPR_MATERIAL_INPUT_OUTTYPE, outType.GetInt());

	return valueNode;
}
