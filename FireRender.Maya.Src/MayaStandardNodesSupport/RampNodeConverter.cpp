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
#include "RampNodeConverter.h"
#include "FireMaya.h"
#include "FireRenderUtils.h"
#include "NodeProcessingUtils.h"
#include "RampNodeConverter.h"
#include <maya/MItDependencyGraph.h>

namespace MayaStandardNodeConverters
{ 
RampNodeConverter::RampNodeConverter(const ConverterParams& params) : BaseConverter(params)
{

}

namespace
{
	unsigned int RampsTypesSupportedByRPR = VRamp | URamp | DiagonalRamp | CircularRamp;
}

// this is executed for each element of compound plug (color plug)
bool ProcessColorPlug(MPlug& colorValuePlug, std::vector<float>& out)
{
	std::vector<MString> attrs = dumpAttributeNamesDbg(colorValuePlug.attribute());

	MStatus status;
	MObject node = colorValuePlug.attribute();
	MFn::Type dataType = node.apiType();

	if (dataType != MFn::kNumericAttribute)
		return false;

	float value = colorValuePlug.asFloat(&status);
	assert(status == MStatus::kSuccess);

	out.push_back(value);

	return true;
}


using CtrlPointDataT = std::tuple<MColor, MString, MObject>;
using CtrlPointT = RampCtrlPoint<CtrlPointDataT>;

void ProcessRampValueAttribute(MPlug& fakeRampPlug, CtrlPointT& ctrlPoint)
{
	// color value
	// - save plug name (need to access node that acts as an input; maya doesn't allow us to get this ndoe otherwise)
	std::get<MString>(ctrlPoint.ctrlPointData) = fakeRampPlug.name(); // string

	// color value
	// - this is compound attribute of 3 floats RGB
	std::vector<float> color;
	color.reserve(3);
	bool res = ForEachPlugInCompoundPlug<std::vector<float>>(fakeRampPlug, color, ProcessColorPlug);
	assert(res);
	std::get<MColor>(ctrlPoint.ctrlPointData) = MColor(MColor::kRGB, color[0], color[1], color[2]);
}

bool ProcessFakeRampAttribute(MPlug& fakeRampPlug, std::vector<CtrlPointT>& out)
{
	MStatus status;
	MObject node = fakeRampPlug.attribute();
	CtrlPointT& ctrlPoint = out.back();

	MFn::Type dataType = node.apiType();
	switch (dataType)
	{
		case MFn::kAttribute3Float: 
		{
			// color value
			// - this is compouind attribute of 3 floats RGB
			ProcessRampValueAttribute(fakeRampPlug, ctrlPoint);
			break;
		}
		case MFn::kNumericAttribute: 
		{
			// control point position
			float positionValue = fakeRampPlug.asFloat(&status);
			assert(status == MStatus::kSuccess);
			ctrlPoint.position = positionValue;
			break;
		}
		case MFn::kEnumAttribute:
		{
			// this is interpolation; ignored
			break;
		}
		default:
			return false;
	}

	return true;
}

bool CreateCtrlPointsFromPlug(MObject rampObject, std::vector<CtrlPointT>& out)
{
	out.clear();

	MStatus status;
	MFnDependencyNode fnRampObject(rampObject, &status);

	// get pseudo ramp attribute from ramp node
	MPlug rampPlug = fnRampObject.findPlug("colorEntryList", &status);
	bool isArray = rampPlug.isArray(&status);
	if (!isArray)
		return false;

	unsigned int count = rampPlug.numElements();
	if (count == 0)
		return false;

	out.reserve(count);

	// this is executed for each element of array plug fakeRampPlug
	auto func = [](MPlug& fakeRampPlug, std::vector<CtrlPointT>& out)->bool
	{
		out.emplace_back();
		CtrlPointT& ctrlPoint = out.back();
		ctrlPoint.method = InterpolationMethod::kLinear;
		ctrlPoint.index = (unsigned int)out.size() - 1;

		return ForEachPlugInCompoundPlug<std::vector<CtrlPointT>>(fakeRampPlug, out, ProcessFakeRampAttribute);
	};

	// creates ramp control point from attributes data in pseudo ramp attribute rampPlug
	bool res = ForEachPlugInArrayPlug<std::vector<CtrlPointT>>(rampPlug, out, func);
	if (!res)
		return false;

	// add last ctrl point (is not added by Maya but is convinient for further calclulations)
	CtrlPointDataT tCtrlPointValue = out.back().ctrlPointData;
	out.emplace_back();
	out.back().position = 1.0f;
	out.back().ctrlPointData = tCtrlPointValue;

	return true;
}

frw::Value GetConnectedArithmetic(MObject rampObject, const FireMaya::Scope& scope)
{
	MStatus status;

	// get ramp node UV connection
	MFnDependencyNode fnRampObject(rampObject, &status);
	MPlug uvPlug = fnRampObject.findPlug("uv", &status);
	if (status != MStatus::kSuccess)
		return frw::Value();

	// continue parsing the node tree
	frw::Value lookupValue = scope.GetValue(uvPlug);
	return lookupValue;
}

frw::ArithmeticNode GetLookupForVRamp(const FireMaya::Scope& scope)
{
	// create lookup node
	frw::LookupNode lookupNode(scope.MaterialSystem(), frw::LookupTypeUV0);

	// to have proper return type
	frw::ArithmeticNode bufferLookupV(scope.MaterialSystem(), frw::OperatorSelectY, lookupNode);
	return bufferLookupV;
}

frw::ArithmeticNode GetLookupForURamp(const FireMaya::Scope& scope)
{
	// create lookup node
	frw::LookupNode lookupNode(scope.MaterialSystem(), frw::LookupTypeUV0);

	// to have proper return type
	frw::ArithmeticNode bufferLookupU(scope.MaterialSystem(), frw::OperatorSelectX, lookupNode);
	return bufferLookupU;
}

frw::ArithmeticNode GetLookupForDiagonalRamp(const FireMaya::Scope& scope)
{
	// create lookup node
	frw::LookupNode lookupNode(scope.MaterialSystem(), frw::LookupTypeUV0);

	// create arithmetic node tree for desired effect
	frw::ArithmeticNode arith_X(scope.MaterialSystem(), frw::OperatorSelectX, lookupNode);
	frw::ArithmeticNode arith_Y(scope.MaterialSystem(), frw::OperatorSelectY, lookupNode);
	frw::ArithmeticNode arith_1minusX(scope.MaterialSystem(), frw::OperatorSubtract, frw::Value(1.0f, 1.0f, 1.0f, 1.0f), arith_X);
	frw::ArithmeticNode arith_add(scope.MaterialSystem(), frw::OperatorAdd, arith_1minusX, arith_Y);
	frw::ArithmeticNode arith_div(scope.MaterialSystem(), frw::OperatorDivide, arith_add, frw::Value(2.0f, 2.0f, 2.0f, 2.0f));

	return arith_div;
}

frw::ArithmeticNode GetLookupForCircularRamp(const FireMaya::Scope& scope)
{
	// create lookup node
	frw::LookupNode lookupNode(scope.MaterialSystem(), frw::LookupTypeUV0);

	// create arithmetic node tree for desired effect
	frw::ArithmeticNode arith_XYminus05(scope.MaterialSystem(), frw::OperatorSubtract, lookupNode, frw::Value(0.5f, 0.5f, 0.0f, 0.0f));
	frw::ArithmeticNode arith_dot(scope.MaterialSystem(), frw::OperatorDot, arith_XYminus05, arith_XYminus05);
	frw::ArithmeticNode arith_sqrt(scope.MaterialSystem(), frw::OperatorPow, arith_dot, frw::Value(0.5f, 0.0f, 0.0f, 0.0f));
	frw::ArithmeticNode arith_mul(scope.MaterialSystem(), frw::OperatorMultiply, arith_sqrt, frw::Value(2.0f, 0.0f, 0.0f, 0.0f));

	return arith_mul;
}

using RampNodeTreeGenerator = std::function<frw::ArithmeticNode(const FireMaya::Scope& scope)>;
static const std::map<RampUVType, RampNodeTreeGenerator> m_rampGenerators = {
	{	RampUVType::VRamp,			RampNodeTreeGenerator(GetLookupForVRamp)},
	{	RampUVType::URamp,			RampNodeTreeGenerator(GetLookupForURamp)},
	{	RampUVType::DiagonalRamp,	RampNodeTreeGenerator(GetLookupForDiagonalRamp)},
	{	RampUVType::CircularRamp,	RampNodeTreeGenerator(GetLookupForCircularRamp)},
};

using ArithmeticNodesBuffer = std::vector<std::tuple<MString, MObject, MColor>>;

frw::Value GetNodeFromABuffer(
	const FireMaya::Scope& scope, 
	ArithmeticNodesBuffer& abuffer)
{
	auto el = abuffer.back();

	if (std::get<MObject>(el) != MObject::kNullObj)
	{
		return scope.GetValue(std::get<MObject>(el), "out");
	}
	else
	{
		MColor color = std::get<MColor>(el);
		return frw::Value(color.r, color.g, color.b);
	}
}

frw::Value GetBlendConvertorRecursive(
	MObject& shaderNodeObject, 
	const FireMaya::Scope& scope, 
	RampUVType rampType,
	ArithmeticNodesBuffer& abuffer)
{	
	// first arithm node input
	frw::Value firstANode = GetNodeFromABuffer(scope, abuffer);
	abuffer.pop_back();

	// second arithm node input
	frw::Value secondANode;
	if (abuffer.size() != 1)
	{
		assert(abuffer.size() != 0);
		secondANode = GetBlendConvertorRecursive(shaderNodeObject, scope, rampType, abuffer);
	}
	else
	{
		secondANode = GetNodeFromABuffer(scope, abuffer);
	}

	const auto& nodeTreeGeneratorImpl = m_rampGenerators.find(rampType);
	assert(nodeTreeGeneratorImpl != m_rampGenerators.end());
	frw::ArithmeticNode rampNodeTree = nodeTreeGeneratorImpl->second(scope);
	
	// create blend node
	frw::Value blendNode = scope.MaterialSystem().ValueBlend(
		/*color0*/ firstANode,
		/*color1*/ secondANode,
		/*weight*/ rampNodeTree);
	return blendNode;
}

void ArrangeBufferViaRampCtrlPoints(MObject& shaderNodeObject, const FireMaya::Scope& scope, ArithmeticNodesBuffer& abuffer)
{
	// extract values from ramp node ramp
	std::vector<RampCtrlPoint<std::pair<MColor, MString>>> rampCtrlPoints;
	
	// create arithmetic nodes from regular color inputs
	for (auto& ctrlPoint : rampCtrlPoints)
	{
		auto it = std::find_if(abuffer.begin(), abuffer.end(), [&ctrlPoint](auto& el)->bool {
			return (std::get<MString>(el) == ctrlPoint.ctrlPointData.second);
		});

		if (it != abuffer.end())
			continue;

		abuffer.emplace_back(
			std::make_tuple(ctrlPoint.ctrlPointData.second, MObject::kNullObj, ctrlPoint.ctrlPointData.first)
		);
	}

	// sort arithmetic nodes buffer
	std::vector<unsigned int> positions;
	positions.reserve(abuffer.size());
	for (std::tuple<MString, MObject, MColor>& tmp : abuffer)
	{
		auto it = std::find_if(rampCtrlPoints.begin(), rampCtrlPoints.end(), [&tmp](auto& el)->bool {
			return (std::get<MString>(tmp) == el.ctrlPointData.second);
		});
		assert(it != rampCtrlPoints.end());
		unsigned int position = (unsigned int) std::distance(rampCtrlPoints.begin(), it);
		positions.push_back(position);
	}

	for (int idx = 0; idx < positions.size(); ++idx)
	{
		unsigned int pos = positions[idx];
		std::swap(abuffer[idx], abuffer[pos]);
		std::swap(positions[idx], positions[pos]);
	}
}

template <typename T>
bool ProcessCompundPlugElementMayaRamp(MPlug& childPlug, T& out)
{
	static_assert(std::is_same<CtrlPointDataT, T>::value, "data container type mismatch!");

	// find color input plug
	std::string elementName = childPlug.name().asChar();
	if (elementName.find("color") == std::string::npos)
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

template <typename T>
bool ProcessMayaRampArrayPlugElement(MPlug& elementPlug, T& out)
{
	static_assert(std::is_same<typename std::remove_reference<decltype(out)>::type, typename std::vector<CtrlPointT>::iterator>::value, "data container type mismatch!");

	bool success = ForEachPlugInCompoundPlug<CtrlPointDataT>(elementPlug, out->ctrlPointData, ProcessCompundPlugElementMayaRamp<CtrlPointDataT>);

	out++;

	return success;
}


bool GetConnectedCtrlPointsObjectsMayaRamp(MPlug& rampPlug, std::vector<CtrlPointT>& rampCtrlPoints)
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
	static_assert(std::is_same<containerIterT, typename std::vector<CtrlPointT>::iterator>::value, "data container type mismatch!");

	bool success = MayaStandardNodeConverters::ForEachPlugInArrayPlug<containerIterT>(rampPlug, currCtrlPointIt, MayaStandardNodeConverters::ProcessMayaRampArrayPlugElement<containerIterT>);
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

frw::Value CreateRPRRampFromMayaRamp(
	MObject& shaderNodeObject, 
	const FireMaya::Scope& scope,
	RampUVType rampType)
{
	// create node
	frw::RampNode rampNode(scope.MaterialSystem());

	// extract values from ramp node ramp
	MStatus status;
	MFnDependencyNode fnRampObject(shaderNodeObject, &status);

	// read input ramp
	std::vector<RampCtrlPoint<CtrlPointDataT>> outRampCtrlPoints;
	MPlug ctrlPointsPlug = fnRampObject.findPlug("colorEntryList", &status);
	// - read simple ramp control point values

	bool res = CreateCtrlPointsFromPlug(shaderNodeObject, outRampCtrlPoints);
	if (!res)
		return frw::Value();

	// - read and process node ramp control point values
	bool success = GetConnectedCtrlPointsObjectsMayaRamp(ctrlPointsPlug, outRampCtrlPoints);
	if (!success)
		return frw::Value();
	
	// - translate control points to RPR representation
	TranslateControlPoints(rampNode, scope, outRampCtrlPoints);
	
	frw::Value uv = scope.GetConnectedValue(fnRampObject.findPlug("uv", false));
	frw::ArithmeticNode uvMod(scope.MaterialSystem(), frw::OperatorMod, uv, frw::Value(1.0f, 1.0f, 1.0f, 1.0f));
	frw::ArithmeticNode uvTransformed = ApplyUVType(scope, uvMod, rampType);
	
	frw::ArithmeticNode abs(scope.MaterialSystem(), frw::OperatorAbs, uvTransformed);
	frw::ArithmeticNode mod(scope.MaterialSystem(), frw::OperatorMod, abs, frw::Value(1.0f, 1.0f, 1.0f, 1.0f));
	rampNode.SetValue(RPR_MATERIAL_INPUT_UV, mod);
	return rampNode;
}

bool IsRampSupportedbyRPR(MFnDependencyNode& fnNode, RampUVType rampType)
{
	if (!(rampType & RampsTypesSupportedByRPR))
		return false;

	MStatus status;

	MPlug rampNoisePlug = fnNode.findPlug("noise", &status);
	float rampNoise = rampNoisePlug.asFloat(&status);
	if (rampNoise > FLT_EPSILON)
		return false;

	MPlug rampUWavePlug = fnNode.findPlug("uWave", &status);
	float rampUWave = rampUWavePlug.asFloat(&status);
	if (rampUWave > FLT_EPSILON)
		return false;

	MPlug rampVWavePlug = fnNode.findPlug("vWave", &status);
	float rampVWave = rampVWavePlug.asFloat(&status);
	if (rampVWave > FLT_EPSILON)
		return false;

	return true;
}

frw::Value RampNodeConverter::Convert() const
{
	MStatus status;
	MObject shaderNodeObject = m_params.shaderNode.object();

	MFnDependencyNode fnShaderNodeObject(shaderNodeObject, &status);
	MPlug rampTypePlug = fnShaderNodeObject.findPlug("type", &status);
	RampUVType rampType = (RampUVType)(1 << rampTypePlug.asInt(&status)); // get the type to rpr ramp format (check RampUVType for details)

	return CreateRPRRampFromMayaRamp(shaderNodeObject, m_params.scope, rampType);
}

frw::ArithmeticNode GetRampNodeLookup(const FireMaya::Scope& scope, RampUVType rampType)
{
	const auto& nodeTreeGeneratorImpl = MayaStandardNodeConverters::m_rampGenerators.find(rampType);
	assert(nodeTreeGeneratorImpl != MayaStandardNodeConverters::m_rampGenerators.end());
	frw::ArithmeticNode rampNodeTree = nodeTreeGeneratorImpl->second(scope);

	return rampNodeTree;
}

}
