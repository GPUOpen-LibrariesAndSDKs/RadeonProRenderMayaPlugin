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
#include "FireRenderRamp.h"
#include "FireRenderUtils.h"
#include "MayaStandardNodesSupport/RampNodeConverter.h"

#include <maya/MFnNumericAttribute.h>
#include <maya/MColor.h>

namespace
{
	namespace Attribute
	{
		MObject rampInterpolationMode;
		MObject rampUVType;
		MObject inputRamp;

		MObject	output;
	}
}

MStatus FireMaya::RPRRamp::initialize()
{
	MStatus status;

	MFnEnumAttribute eAttr;
	Attribute::rampInterpolationMode = eAttr.create("rampInterpolationMode", "rinm", frw::InterpolationModeLinear);
	eAttr.addField("None", frw::InterpolationModeNone);
	eAttr.addField("Linear", frw::InterpolationModeLinear);
	MAKE_INPUT_CONST(eAttr);
	status = MPxNode::addAttribute(Attribute::rampInterpolationMode);
	CHECK_MSTATUS(status);

	Attribute::rampUVType = eAttr.create("rampUVType", "ruvt", VRamp);
	eAttr.addField("V Ramp", VRamp);
	eAttr.addField("U Ramp", URamp);
	eAttr.addField("Diagonal Ramp", DiagonalRamp);
	eAttr.addField("Circular Ramp", CircularRamp);
	MAKE_INPUT_CONST(eAttr);
	status = MPxNode::addAttribute(Attribute::rampUVType);
	CHECK_MSTATUS(status);

	MRampAttribute rAttr;
	Attribute::inputRamp = rAttr.createColorRamp("inputRamp", "rinp", &status);
	CHECK_MSTATUS(status);
	status = MPxNode::addAttribute(Attribute::inputRamp);
	CHECK_MSTATUS(status);

	MFnNumericAttribute nAttr;
	Attribute::output = nAttr.createPoint("out", "rout");
	MAKE_OUTPUT(nAttr);
	status = addAttribute(Attribute::output);
	CHECK_MSTATUS(status);

	status = attributeAffects(Attribute::inputRamp, Attribute::output);
	CHECK_MSTATUS(status);
	status = attributeAffects(Attribute::rampInterpolationMode, Attribute::output);
	CHECK_MSTATUS(status);
	status = attributeAffects(Attribute::rampUVType, Attribute::output);
	CHECK_MSTATUS(status);

	return MS::kSuccess;
}

void* FireMaya::RPRRamp::creator()
{
	return new RPRRamp;
}

frw::Value FireMaya::RPRRamp::GetValue(const Scope& scope) const
{
	MFnDependencyNode shaderNode(thisMObject());

	frw::RampNode rampNode(scope.MaterialSystem());

	frw::RampInterpolationMode mode = frw::InterpolationModeNone;
	MPlug plug = shaderNode.findPlug(Attribute::rampInterpolationMode, false);
	if (plug.isNull())
		return frw::Value();

	int temp = 0;
	if (MStatus::kSuccess == plug.getValue(temp))
		mode = static_cast<frw::RampInterpolationMode>(temp);

	using CtrlPointT = RampCtrlPoint<MColor>;
	std::vector<CtrlPointT> outRampCtrlPoints;
	MPlug ctrlPointsPlug = shaderNode.findPlug(Attribute::inputRamp, false);
	bool isRampParced = GetRampValues<MColorArray>(ctrlPointsPlug, outRampCtrlPoints);
	if (!isRampParced)
		return frw::Value();

	std::vector<float> ctrlPointsVals;
	ctrlPointsVals.reserve(outRampCtrlPoints.size() * 4);
	for (const CtrlPointT& tCtrl : outRampCtrlPoints)
	{
		ctrlPointsVals.push_back(tCtrl.position);
		ctrlPointsVals.push_back(tCtrl.ctrlPointData.r);
		ctrlPointsVals.push_back(tCtrl.ctrlPointData.g);
		ctrlPointsVals.push_back(tCtrl.ctrlPointData.b);
	}

	rampNode.SetInterpolationMode(mode);
	rampNode.SetControlPoints(ctrlPointsVals.data(), ctrlPointsVals.size());

	// get proper lookup
	MPlug rampPlug = shaderNode.findPlug(Attribute::rampUVType, false);
	if (rampPlug.isNull())
		return frw::Value();

	temp = 1;
	RampUVType rampType = VRamp;
	if (MStatus::kSuccess == rampPlug.getValue(temp))
		rampType = static_cast<RampUVType>(temp);

	frw::ArithmeticNode lookupTree = GetRampNodeLookup(scope, rampType);
	rampNode.SetLookup(lookupTree);

	return rampNode;
}

void FireMaya::RPRRamp::postConstructor()
{
	// set default values for ramps
	// - if we don't then there will be no default control point and Maya will display a ui error
	// - this happens despite api documentation saying that default control point is created automatically - it's not.
	MPlug colorRampPlug(thisMObject(), Attribute::inputRamp);
	const float colorSrc[][4] = 
	{ 
		1.0f, 0.0f, 0.0f, 1.0f, 
		0.0f, 1.0f, 0.0f, 1.0f, 
		0.0f, 0.0f, 1.0f, 1.0f 
	};
	MColorArray colorValues(colorSrc, 3);
	SetRampValues(colorRampPlug, colorValues);

	ValueNode::postConstructor();
}

/*MStatus FireMaya::RPRRamp::compute(const MPlug& plug, MDataBlock& block)
{
	if ((plug == Attribute::output) || (plug.parent() == Attribute::output))
	{
		ForceEvaluateAllAttributes(true);



		return MS::kSuccess;
	}
	else
		return MS::kUnknownParameter;

	return MS::kSuccess;
}*/
