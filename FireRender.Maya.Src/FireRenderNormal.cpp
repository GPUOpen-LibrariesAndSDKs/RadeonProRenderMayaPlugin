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
#include "FireRenderNormal.h"

#include <maya/MFnTypedAttribute.h>
#include "maya/MGlobal.h"

namespace
{
	namespace Attribute
	{
		MObject color;
		MObject source;		// will be replaced with "color" when upgrading scene
		MObject strength;
		MObject bevel;

		MObject	output;
	}
}


void* FireMaya::Normal::creator()
{
	return new FireMaya::Normal;
}

void FireMaya::Normal::OnFileLoaded()
{
	// Older version of this node used texture file name. Convert it to regular graph node.
	ConvertFilenameToFileInput(Attribute::source, Attribute::color);
}

MStatus FireMaya::Normal::initialize()
{
#define DEPRECATED_PARAM(attr) \
	CHECK_MSTATUS(attr.setCached(false)); \
	CHECK_MSTATUS(attr.setStorable(false)); \
	CHECK_MSTATUS(attr.setHidden(true));

	MFnTypedAttribute tAttr;
	MFnNumericAttribute nAttr;

	Attribute::color = nAttr.createColor("color", "c");
	MAKE_INPUT(nAttr);

	Attribute::bevel = nAttr.createColor("bevel", "b");
	MAKE_INPUT(nAttr);

	Attribute::source = tAttr.create("filename", "src", MFnData::kString);
	tAttr.setUsedAsFilename(true);
	MAKE_INPUT_CONST(tAttr);
	DEPRECATED_PARAM(tAttr);

	Attribute::strength = nAttr.create("strength", "str", MFnNumericData::kFloat, 1.0);
	MAKE_INPUT(nAttr);
	nAttr.setSoftMax(1.0);
	nAttr.setSoftMin(0.0);

	Attribute::output = nAttr.createColor("out", "o");
	MAKE_OUTPUT(nAttr);

	CHECK_MSTATUS(addAttribute(Attribute::color));
	CHECK_MSTATUS(addAttribute(Attribute::bevel));
	CHECK_MSTATUS(addAttribute(Attribute::source));
	CHECK_MSTATUS(addAttribute(Attribute::strength));
	CHECK_MSTATUS(addAttribute(Attribute::output));

	CHECK_MSTATUS(attributeAffects(Attribute::color, Attribute::output));
	CHECK_MSTATUS(attributeAffects(Attribute::bevel, Attribute::output));
	CHECK_MSTATUS(attributeAffects(Attribute::source, Attribute::output)); //
	CHECK_MSTATUS(attributeAffects(Attribute::strength, Attribute::output));

	return MS::kSuccess;
}

frw::Value FireMaya::Normal::GetValue(const Scope& scope) const
{
	MFnDependencyNode shaderNode(thisMObject());

	frw::Value color = scope.GetConnectedValue(shaderNode.findPlug(Attribute::color, false));
	if (color)
	{
		frw::NormalMapNode imageNode(scope.MaterialSystem());
		imageNode.SetMap(color);

		const IFireRenderContextInfo* ctxInfo = scope.GetIContextInfo();
		assert(ctxInfo);
		if (ctxInfo->IsUberScaleSupported())
		{
			frw::Value strength = scope.GetValue(shaderNode.findPlug(Attribute::strength, false));
			if (strength != 1.)
				imageNode.SetValue(RPR_MATERIAL_INPUT_SCALE, strength);
		}

		frw::Value bevel = scope.GetConnectedValue(shaderNode.findPlug(Attribute::bevel, false));
		if (bevel && (bevel.GetNodeType() == frw::ValueTypeBevel))
		{
			imageNode.SetValue(RPR_MATERIAL_INPUT_BASE_NORMAL, bevel);
		}

		return imageNode;
	}

	return nullptr;
}