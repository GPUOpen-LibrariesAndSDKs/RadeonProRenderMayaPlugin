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
#include "MayaStandardNodesSupport/NodeProcessingUtils.h"

#include <maya/MFnNumericAttribute.h>
#include <maya/MColor.h>

namespace
{
	namespace Attribute
	{
		MObject	uv;
		MObject	uvSize;
		MObject rampInterpolationMode;
		MObject rampUVType;
		MObject inputRamp;

		MObject	output;
	}
}

MStatus FireMaya::RPRRamp::initialize()
{
	MStatus status;
	MFnNumericAttribute nAttr;
	MFnEnumAttribute eAttr;

	Attribute::uv = nAttr.create("uvCoord", "uv", MFnNumericData::k2Float);
	MAKE_INPUT(nAttr);
	status = MPxNode::addAttribute(Attribute::uv);
	// Note: we are not using this, but we have to have it to support classification of this node as a texture2D in Maya:
	Attribute::uvSize = nAttr.create("uvFilterSize", "fs", MFnNumericData::k2Float);
	MAKE_INPUT(nAttr);
	status = MPxNode::addAttribute(Attribute::uvSize);

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
	status = attributeAffects(Attribute::uv, Attribute::output);
	CHECK_MSTATUS(status);
	status = attributeAffects(Attribute::uvSize, Attribute::output);
	CHECK_MSTATUS(status);

	return MS::kSuccess;
}

void* FireMaya::RPRRamp::creator()
{
	return new RPRRamp;
}

// control point can be MColor OR it can be connection to other node; in this case we need to save both node and connection plug name
using CtrlPointDataT = std::tuple<MColor, MString, MObject>; 
using CtrlPointT = RampCtrlPoint<CtrlPointDataT>;

// this is called for every element of compound plug of a Ramp attribute;
// in Maya a single control point of a Ramp attribute is represented as a compound plug;
// thus Ramp attribute is an array of compound plugs
template <typename T>
bool ProcessCompundPlugElement(MPlug& childPlug, T& out)
{
	static_assert(std::is_same<CtrlPointDataT, T>::value, "data container type mismatch!");

	// find color input plug
	std::string elementName = childPlug.name().asChar();
	if (elementName.find("inputRamp_Color") == std::string::npos)
		return true; // this is not element that we are looking for => caller will continue processing elements of compound plug

	// get connections
	MStatus status;
	MPlugArray connections;
	bool connectedTo = childPlug.connectedTo(connections, true, true, &status);
	if (!connectedTo)
	{
		std::get<MString>(out) = "";
		std::get<MObject>(out) = MObject::kNullObj;

		return true; // no connections found
	}

	// color input has connected node => save its name and object to output
	assert(connections.length() == 1);

	for (auto& it : connections)
	{
		// save connected node
		std::get<MObject>(out) = it.node();

		// get connected node output name (is needed to setup proper connection for material node tree)
		std::string attrName = it.name().asChar();
		MFnDependencyNode depN(it.node());
		std::string connectedNodeName = depN.name().asChar();
		connectedNodeName += ".";
		attrName.erase(attrName.find(connectedNodeName), connectedNodeName.length());

		MPlug outPlug = depN.findPlug(attrName.c_str(), &status);
		assert(status == MStatus::kSuccess);
		assert(!outPlug.isNull());
		std::string plugName = outPlug.partialName().asChar();

		// save connected node output name
		std::get<MString>(out) = plugName.c_str();
	}

	return true; // connection found and processed
}

// this is called for every element of Ramp control points array;
// in Maya control points of the Ramp attribute are stored as an array attribute
template <typename T>
bool ProcessRampArrayPlugElement(MPlug& elementPlug, T& out)
{
	static_assert(std::is_same<typename std::remove_reference<decltype(out)>::type, typename std::vector<CtrlPointT>::iterator>::value, "data container type mismatch!");

	bool success = MayaStandardNodeConverters::ForEachPlugInCompoundPlug<CtrlPointDataT>(elementPlug, out->ctrlPointData, ProcessCompundPlugElement<CtrlPointDataT>);

	out++;

	return success;
}

// iterate through all control points of the Ramp and save connected nodes data to corresponding control points array entries if such nodes exist
template <typename RampCtrlPointDataT>
bool GetConnectedCtrlPointsObjects(MPlug& rampPlug, std::vector<RampCtrlPointDataT>& rampCtrlPoints)
{
	MStatus status;

	// ensure valid input
	bool isArray = rampPlug.isArray(&status);
	assert(isArray);
	if (!isArray)
		return false;

	bool doArraysMatch = rampCtrlPoints.size() == rampPlug.numElements();
	if (!doArraysMatch)
	{
		rampCtrlPoints.pop_back(); // remove "technical" control point created by helper if necessary
	}
	doArraysMatch = rampCtrlPoints.size() == rampPlug.numElements();
	assert(doArraysMatch);
	if (!doArraysMatch)
		return false;

	// iterate through control points; passing iterator as a data container here to avoid extra unnecessary data copy
	auto currCtrlPointIt = rampCtrlPoints.begin();
	using containerIterT = decltype(currCtrlPointIt);
	static_assert(std::is_same<containerIterT, typename std::vector<RampCtrlPointDataT>::iterator>::value, "data container type mismatch!");

	bool success = MayaStandardNodeConverters::ForEachPlugInArrayPlug<containerIterT>(rampPlug, currCtrlPointIt, ProcessRampArrayPlugElement<containerIterT>);
	assert(success);
	assert(currCtrlPointIt == rampCtrlPoints.end());

	// sort values by position (maya returns control points in random order)
	// we have to do it here since plugs cannot be sorted and if we sort out points before reading plugs, they might become misaligned
	// instead of alligning them, we sort points only after reading all the plugs
	std::sort(rampCtrlPoints.begin(), rampCtrlPoints.end(), [](auto first, auto second)->bool {
		return (first.position < second.position); });

	// add control points to the beginning and the end as copies of neighboring points to remove black edges
	// needed for rpr ramp internal logic
	if (rampCtrlPoints.front().position > FLT_EPSILON)
	{
		rampCtrlPoints.emplace_back();
		const auto& first = rampCtrlPoints.begin();
		auto& ctrlPointRef = rampCtrlPoints.back();
		ctrlPointRef.ctrlPointData = first->ctrlPointData;
		ctrlPointRef.method = first->method;
		ctrlPointRef.position = 0.0f;
		ctrlPointRef.index = 99; // this value is irrelevant
		std::rotate(rampCtrlPoints.begin(), rampCtrlPoints.end() - 1, rampCtrlPoints.end());
	}

	if ((1.0f - rampCtrlPoints.back().position) > FLT_EPSILON)
	{
		rampCtrlPoints.emplace_back();
		const auto& last = rampCtrlPoints.end() - 2;
		auto& ctrlPointRef = rampCtrlPoints.back();
		ctrlPointRef.ctrlPointData = last->ctrlPointData;
		ctrlPointRef.method = last->method;
		ctrlPointRef.position = 1.0f;
		ctrlPointRef.index = 100; // this value is irrelevant
	}
	return success;
}

// note that we can have either MColor OR connected node as ramp control point data value
template <typename T>
void TranslateControlPoints(frw::RampNode& rampNode, const FireMaya::Scope& scope, const std::vector<T>& outRampCtrlPoints)
{
	// control points values; need to set them for both color and node inputs
	std::vector<float> ctrlPointsVals;
	ctrlPointsVals.reserve(outRampCtrlPoints.size() * 4);

	unsigned countMaterialInputs = 0; // necessary to pass this for RPRRampNode inputs logic

	// iterate through control points array
	for (const CtrlPointT& tCtrl : outRampCtrlPoints)
	{
		ctrlPointsVals.push_back(tCtrl.position);

		MObject ctrlPointConnectedObject = std::get<MObject>(tCtrl.ctrlPointData);
		if (ctrlPointConnectedObject == MObject::kNullObj)
		{
			// control point is a plain color and not a connected node
			const MColor& colorValue = std::get<MColor>(tCtrl.ctrlPointData);
			ctrlPointsVals.push_back(colorValue.r);
			ctrlPointsVals.push_back(colorValue.g);
			ctrlPointsVals.push_back(colorValue.b);
			continue;
		}

		// control point is a connected node and must be processed accordingly
		// - set color override values (so that RPR knows it must override color input with node input)
		ctrlPointsVals.push_back(-1.0f);
		ctrlPointsVals.push_back(-1.0f);
		ctrlPointsVals.push_back(-1.0f);

		// - add node input
		frw::Value materialNode = scope.GetValue(ctrlPointConnectedObject, std::get<MString>(tCtrl.ctrlPointData));
		rampNode.SetMaterialControlPointValue(countMaterialInputs++, materialNode);
	}

	rampNode.SetControlPoints(ctrlPointsVals.data(), ctrlPointsVals.size());
}

frw::Value FireMaya::RPRRamp::GetValue(const Scope& scope) const
{
	MFnDependencyNode shaderNode(thisMObject());

	// create node
	frw::RampNode rampNode(scope.MaterialSystem());

	MPlug interpPlug = shaderNode.findPlug(Attribute::rampInterpolationMode, false);
	if (interpPlug.isNull())
		return frw::Value();

	int type = interpPlug.asInt();

	// process ramp parameters
	frw::RampInterpolationMode mode = frw::InterpolationModeNone;
	mode = static_cast<frw::RampInterpolationMode>(type);

	rampNode.SetInterpolationMode(mode);

	// read input ramp
	std::vector<CtrlPointT> outRampCtrlPoints;
	MPlug ctrlPointsPlug = shaderNode.findPlug(Attribute::inputRamp, false);

	// - read simple ramp control point values
	bool isRampParced = GetRampValues<MColorArray>(ctrlPointsPlug, outRampCtrlPoints, false);
	if (!isRampParced)
		return frw::Value();

	// - read and process node ramp control point values
	// we also add up to 2 points at the begining and the end to remove black edges
	bool success = GetConnectedCtrlPointsObjects(ctrlPointsPlug, outRampCtrlPoints);
	if (!success)
		return frw::Value();

	// - translate control points to RPR representation
	TranslateControlPoints(rampNode, scope, outRampCtrlPoints);

	// get proper lookup
	MPlug rampPlug = shaderNode.findPlug(Attribute::rampUVType, false);
	if (rampPlug.isNull())
		return frw::Value();
	int uvIntType = rampPlug.asInt();

	frw::Value uv = scope.GetConnectedValue(shaderNode.findPlug(Attribute::uv, false));
	frw::ArithmeticNode uvMod(scope.MaterialSystem(), frw::OperatorMod, uv, frw::Value(1.0f, 1.0f, 1.0f, 1.0f));
	frw::ArithmeticNode uvTransformed = ApplyUVType(scope, uvMod, uvIntType);
	
	frw::ArithmeticNode abs(scope.MaterialSystem(), frw::OperatorAbs, uvTransformed);
	frw::ArithmeticNode mod(scope.MaterialSystem(), frw::OperatorMod, abs, frw::Value(1.0f, 1.0f, 1.0f, 1.0f));
	rampNode.SetValue(RPR_MATERIAL_INPUT_UV, mod);
	return rampNode;
}

frw::ArithmeticNode FireMaya::RPRRamp::ApplyUVType(const Scope& scope, frw::ArithmeticNode& source, int uvIntType) const {
	RampUVType uvType = static_cast<RampUVType>(uvIntType);

	switch (uvType)
	{
	case URamp:
		return frw::ArithmeticNode(scope.MaterialSystem(), frw::OperatorSelectY, source);
	case DiagonalRamp:
	{
		frw::ArithmeticNode arith_X(scope.MaterialSystem(), frw::OperatorSelectX, source);
		frw::ArithmeticNode arith_Y(scope.MaterialSystem(), frw::OperatorSelectY, source);
		frw::ArithmeticNode arith_1minusX(scope.MaterialSystem(), frw::OperatorSubtract, frw::Value(1.0f, 1.0f, 1.0f, 1.0f), arith_X);
		frw::ArithmeticNode arith_add(scope.MaterialSystem(), frw::OperatorAdd, arith_1minusX, arith_Y);
		frw::ArithmeticNode arith_div(scope.MaterialSystem(), frw::OperatorDivide, arith_add, frw::Value(2.0f, 2.0f, 2.0f, 2.0f));
		return arith_div;
	}
	case CircularRamp:
	{
		frw::ArithmeticNode arith_XYminus05(scope.MaterialSystem(), frw::OperatorSubtract, source, frw::Value(0.5f, 0.5f, 0.0f, 0.0f));
		frw::ArithmeticNode arith_dot(scope.MaterialSystem(), frw::OperatorDot, arith_XYminus05, arith_XYminus05);
		frw::ArithmeticNode arith_sqrt(scope.MaterialSystem(), frw::OperatorPow, arith_dot, frw::Value(0.5f, 0.0f, 0.0f, 0.0f));
		frw::ArithmeticNode arith_mul(scope.MaterialSystem(), frw::OperatorMultiply, arith_sqrt, frw::Value(2.0f, 0.0f, 0.0f, 0.0f));
		frw::ArithmeticNode arith_min(scope.MaterialSystem(), frw::OperatorMin, arith_mul, frw::Value(0.99f, 0.99f, 0.99f, 0.0f));
		return arith_min;
	}
	default:
		// VRamp
		return frw::ArithmeticNode(scope.MaterialSystem(), frw::OperatorAdd, source);
	}
}

void FireMaya::RPRRamp::postConstructor()
{
	// we now set default values at AERPRCreateDefaultRampColors, called from AERPRRampTemplate
	// setting them here causes some bugs related to overwriting black points and other strange problems

	ValueNode::postConstructor();
}
