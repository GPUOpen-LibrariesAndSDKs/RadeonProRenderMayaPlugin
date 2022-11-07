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
#include "FireRenderBevel.h"
#include "FireRenderObjects.h"
#include "maya/MMatrix.h"
#include "maya/MTransformationMatrix.h"

namespace
{
	namespace Attribute
	{
		MObject radius;
		MObject samples;
		MObject	output;
	}
}

void* FireMaya::Bevel::creator()
{
	return new FireMaya::Bevel;
}

MStatus FireMaya::Bevel::initialize()
{
	MFnNumericAttribute nAttr;

	Attribute::samples = nAttr.create("samples", "s", MFnNumericData::kByte, 6);
	nAttr.setMin(0);
	nAttr.setSoftMax(40);
	MAKE_INPUT(nAttr);

	Attribute::radius = nAttr.create("radius", "r", MFnNumericData::kFloat, 0.05f);
	nAttr.setSoftMax(0.5f);
	nAttr.setMin(0.0f);
	MAKE_INPUT(nAttr);

	Attribute::output = nAttr.createColor("out", "o");
	MAKE_OUTPUT(nAttr);

	CHECK_MSTATUS(addAttribute(Attribute::samples));
	CHECK_MSTATUS(addAttribute(Attribute::radius));
	CHECK_MSTATUS(addAttribute(Attribute::output));

	CHECK_MSTATUS(attributeAffects(Attribute::samples, Attribute::output));
	CHECK_MSTATUS(attributeAffects(Attribute::radius, Attribute::output));

	return MS::kSuccess;
}

frw::Value FireMaya::Bevel::GetValue(const Scope& scope) const
{
	MFnDependencyNode shaderNode(thisMObject());

	frw::ValueNode valueNode(scope.MaterialSystem(), frw::ValueTypeBevel);

	unsigned int samples = shaderNode.findPlug(Attribute::samples, false).asInt();
	valueNode.SetValueInt(RPR_MATERIAL_INPUT_SAMPLES, samples);

	frw::Value radius (shaderNode.findPlug(Attribute::radius, false).asFloat(), 0.0f, 0.0f, 0.0f);
	const FireRenderMeshCommon* pMesh = scope.GetCurrentlyParsedMesh();
	if (pMesh != nullptr)
	{
		MMatrix matrix = const_cast<FireRenderMeshCommon*>(pMesh)->GetSelfTransform();
		ScaleMatrixFromCmToM(matrix);
		MTransformationMatrix transform(matrix);
		std::vector<double> scale{ 0.0f, 0.0f, 0.0f };
		transform.getScale(scale.data(), MSpace::kWorld);
		std::sort(scale.begin(), scale.end());
		valueNode.SetValue(RPR_MATERIAL_INPUT_RADIUS, scale.at(0) * radius);
	}
	else
	{
		valueNode.SetValue(RPR_MATERIAL_INPUT_RADIUS, radius);
	}

	return valueNode;
}

