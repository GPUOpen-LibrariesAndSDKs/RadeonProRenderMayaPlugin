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

template <typename T>
void ProcessRampValueAttribute(MPlug& fakeRampPlug, RampCtrlPoint<T>& ctrlPoint);

template <>
void ProcessRampValueAttribute(MPlug& fakeRampPlug, RampCtrlPoint<MColor>& ctrlPoint)
{
	// color value
	// - this is compound attribute of 3 floats RGB
	std::vector<float> color;
	color.reserve(3);
	bool res = ForEachPlugInCompoundPlug<std::vector<float>>(fakeRampPlug, color, ProcessColorPlug);
	assert(res);
	ctrlPoint.ctrlPointData.set(MColor::kRGB, color[0], color[1], color[2]);
}

template <>
void ProcessRampValueAttribute(MPlug& fakeRampPlug, RampCtrlPoint<MString>& ctrlPoint)
{
	// color value
	// - save plug name instead of color value (need to access node that acts as an input; maya doesn't allow us to get this ndoe otherwise)
	ctrlPoint.ctrlPointData = fakeRampPlug.name();
}

template <>
void ProcessRampValueAttribute(MPlug& fakeRampPlug, RampCtrlPoint<std::pair<MColor, MString>>& ctrlPoint)
{
	// color value
	// - save plug name (need to access node that acts as an input; maya doesn't allow us to get this ndoe otherwise)
	ctrlPoint.ctrlPointData.second = fakeRampPlug.name();

	// color value
	// - this is compound attribute of 3 floats RGB
	std::vector<float> color;
	color.reserve(3);
	bool res = ForEachPlugInCompoundPlug<std::vector<float>>(fakeRampPlug, color, ProcessColorPlug);
	assert(res);
	ctrlPoint.ctrlPointData.first.set(MColor::kRGB, color[0], color[1], color[2]);
}

// this is executed for each element of array plug (pseudo ramp plug)
template <typename T>
bool ProcessFakeRampAttribute(MPlug& fakeRampPlug, std::vector<RampCtrlPoint<T>>& out)
{
	MStatus status;
	MObject node = fakeRampPlug.attribute();
	RampCtrlPoint<T>& ctrlPoint = out.back();

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
		default:
			return false;
	}

	return true;
}

template <typename T>
bool CreateCtrlPointsFromPlug(MObject rampObject, std::vector<RampCtrlPoint<T>>& out)
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
	auto func = [](MPlug& fakeRampPlug, std::vector<RampCtrlPoint<T>>& out)->bool 
	{
		out.emplace_back();
		RampCtrlPoint<T>& ctrlPoint = out.back();
		ctrlPoint.method = InterpolationMethod::kLinear;
		ctrlPoint.index = (unsigned int)out.size() - 1;

		return ForEachPlugInCompoundPlug<std::vector<RampCtrlPoint<T>>>(fakeRampPlug, out, ProcessFakeRampAttribute<T>);
	};

	// creates ramp control point from attributes data in pseudo ramp attribute rampPlug
	bool res = ForEachPlugInArrayPlug<std::vector<RampCtrlPoint<T>>>(rampPlug, out, func);
	if (!res)
		return false;

	// sort values by position (maya returns control point in random order)
	std::sort(out.begin(), out.end(), [](auto first, auto second)->bool {
		return (first.position < second.position); });

	// add last ctrl point (is not added by Maya but is convinient for further calclulations)
	T tCtrlPointValue = out.back().ctrlPointData;
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
	frw::ArithmeticNode bufferLookupV(scope.MaterialSystem(), frw::OperatorSelectY, lookupNode, frw::Value(1.0f, 1.0f, 1.0f));
	frw::ArithmeticNode bufferLookupMulNode(scope.MaterialSystem(), frw::OperatorMultiply, bufferLookupV, frw::Value(1.0f, 1.0f, 1.0f));
	return bufferLookupMulNode;
}

frw::ArithmeticNode GetLookupForURamp(const FireMaya::Scope& scope)
{
	// create lookup node
	frw::LookupNode lookupNode(scope.MaterialSystem(), frw::LookupTypeUV0);

	// to have proper return type
	frw::ArithmeticNode bufferLookupU(scope.MaterialSystem(), frw::OperatorSelectX, lookupNode, frw::Value(1.0f, 1.0f, 1.0f));
	frw::ArithmeticNode bufferLookupMulNode(scope.MaterialSystem(), frw::OperatorMultiply, bufferLookupU, frw::Value(1.0f, 1.0f, 1.0f));
	return bufferLookupMulNode;
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
	bool res = CreateCtrlPointsFromPlug<std::pair<MColor, MString>>(shaderNodeObject, rampCtrlPoints);
	rampCtrlPoints.pop_back();
	if (!res)
		return;
	
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

frw::Value GetBufferSamplerConvertor(
	MObject& shaderNodeObject, 
	const FireMaya::Scope& scope,
	RampUVType rampType)
{
	// the .colorEntryList property is a compound array attribute; it doesn't get a special function set. 
	// And, confusingly, .colorEntryList is not an MRampAttribute, it's just a regular indexed compound attribute. 

	const unsigned int bufferSize = 256; // same as in Blender

	// extract values from ramp node ramp
	std::vector<RampCtrlPoint<MColor>> rampCtrlPoints;
	bool res = CreateCtrlPointsFromPlug<MColor>(shaderNodeObject, rampCtrlPoints);
	if (!res)
		return frw::Value();

	// create buffer node
	frw::BufferNode bufferNode = CreateRPRRampNode(rampCtrlPoints, scope, bufferSize);

	// get arithmetic mul node tree for lookup
	const auto& nodeTreeGeneratorImpl = m_rampGenerators.find(rampType);
	assert(nodeTreeGeneratorImpl != m_rampGenerators.end());
	frw::ArithmeticNode rampNodeTree = nodeTreeGeneratorImpl->second(scope);
	frw::ArithmeticNode bufferLookupMulNode(scope.MaterialSystem(), frw::OperatorMultiply, rampNodeTree, frw::Value(bufferSize, bufferSize, bufferSize));

	// connect created lookup to buffer (this is needed to make buffer node work in RPR)
	bufferNode.SetUV(bufferLookupMulNode);

	return bufferNode;
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
	// we have 2 branches here
	// first, if RPR Arithmetic node is connected we need to convert Ramp using buffer sampler (as in Blender)
	// second, if RPR Arithmetic is not connected than we need to process Ramp node default way (as picture), because Maya's ramp node has many features that RPR doesn't have
	MStatus status;

	MObject shaderNodeObject = m_params.shaderNode.object();
	MFnDependencyNode fnShaderNodeObject(shaderNodeObject, &status);
	MPlug rampTypePlug = fnShaderNodeObject.findPlug("type", &status);
	RampUVType rampType = (RampUVType)(1 << rampTypePlug.asInt(&status));

	// no unsupprted attributes => process via rpr buffer node
	if (!IsRampSupportedbyRPR(fnShaderNodeObject, rampType))
	{
		// process Ramp node default way (as picture)s
		return m_params.scope.createImageFromShaderNodeUsingFileNode(shaderNodeObject, "outColor");
	}

	// iterate through node connections; search for connected RPRArithmetic
	ArithmeticNodesBuffer arithmeticsConnectionBuffer;

	MItDependencyGraph itdep(
		shaderNodeObject,
		MFn::kDependencyNode,
		MItDependencyGraph::kUpstream,
		MItDependencyGraph::kBreadthFirst,
		MItDependencyGraph::kNodeLevel,
		&status);

	CHECK_MSTATUS(status)

	for (; !itdep.isDone(); itdep.next())
	{
		MFnDependencyNode connectionNode(itdep.currentItem());
		MString connectionNodeTypename = connectionNode.typeName();

		// arithmetic node found => save it and name of attribute it is connected to to the buffer
		// - this is to solve the problem that ramp attribute automatically converts connected nodes to colors
		// - and thus we can't get this node from ramp attribute
		if (connectionNodeTypename == "RPRArithmetic")
		{
			MStatus status;
			MPlug outPlug = connectionNode.findPlug("out", &status);
			MPlugArray theDestinations;
			bool haveDestinations = outPlug.destinations(theDestinations, &status);
			std::vector<MPlug> destinations;
			WriteMayaArrayTo(destinations, theDestinations);
			std::vector<MString> destinationNames;
			for (auto& tplug : destinations)
				destinationNames.push_back(tplug.name());

			assert(destinationNames.size() == 1);
			if (destinationNames.size() != 0)
			{
				// we can't get name of connected nodes from ramp; Maya looses this information
				arithmeticsConnectionBuffer.push_back(std::make_tuple(destinationNames[0], itdep.currentItem(), MColor())); 
			}
		}
	}
	
	// found connected arithmetics => process via rpr blend node
	if (arithmeticsConnectionBuffer.size() > 0)
	{
		// sort connections array according to their position on the ramp
		ArrangeBufferViaRampCtrlPoints(shaderNodeObject, m_params.scope, arithmeticsConnectionBuffer);

		// recursevely create node tree from connections array
		return GetBlendConvertorRecursive(shaderNodeObject, m_params.scope, rampType, arithmeticsConnectionBuffer);
	}

	// use RPR Nodes
	return GetBufferSamplerConvertor(shaderNodeObject, m_params.scope, rampType);
}
}

frw::ArithmeticNode GetRampNodeLookup(const FireMaya::Scope& scope, RampUVType rampType)
{
	const auto& nodeTreeGeneratorImpl = MayaStandardNodeConverters::m_rampGenerators.find(rampType);
	assert(nodeTreeGeneratorImpl != MayaStandardNodeConverters::m_rampGenerators.end());
	frw::ArithmeticNode rampNodeTree = nodeTreeGeneratorImpl->second(scope);

	return rampNodeTree;
}
