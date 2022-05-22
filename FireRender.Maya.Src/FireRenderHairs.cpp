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
#include "FireRenderObjects.h"
#include "Context/FireRenderContext.h"
#include "FireRenderUtils.h"

#include <float.h>
#include <array>
#include <algorithm>
#include <vector>
#include <iterator>

#include <maya/MFnDependencyNode.h>
#include <maya/MPlug.h>
#include <maya/MGlobal.h>
#include <maya/MFnMatrixData.h>
#include <maya/MMatrix.h>
#include <maya/MDagPath.h>
#include <maya/MVector.h>
#include <maya/MPlugArray.h>
#include <maya/MObjectArray.h>
#include <maya/MItDag.h>
#include <maya/MFnRenderLayer.h>
#include <maya/MPlugArray.h>
#include <maya/MAnimControl.h>
#include <maya/MQuaternion.h>
#include <maya/MDagPathArray.h>
#include <maya/MSelectionList.h>
#include <maya/MFloatPointArray.h>
#include <maya/MFloatMatrix.h>
#include <maya/MRenderUtil.h> 
#include <maya/MFnPluginData.h> 
#include <maya/MPxData.h>
#include <maya/MItDependencyGraph.h>

#include <maya/MFnPfxGeometry.h>
#include <maya/MRenderLineArray.h>
#include <maya/MRenderLine.h>

#include <Xgen/src/xgsculptcore/api/XgSplineAPI.h>

// Ornatrix interfaces use host-specific types, so we need to specify the host
#define AUTODESK_MAYA
#undef ASSERT
#include <Ephere/Plugins/Ornatrix/HairInterfacesMaya.h>
#include <Ephere/Ornatrix/IHair.h>

//===================
// Hair shader
//===================
FireRenderHair::FireRenderHair(FireRenderContext* context, const MDagPath& dagPath)
	: FireRenderNode(context, dagPath)
	, m_matrix()
	, m_Curves()
{}

FireRenderHair::~FireRenderHair()
{
	m_matrix = MMatrix();
	clear();
}

// returns true and valid MObject if UberMaterial was found
std::tuple<bool, MObject> GetUberMaterialFromHairShader(MObject& surfaceShader)
{
	if (surfaceShader.isNull())
		return std::make_tuple(false, MObject::kNullObj);

	MFnDependencyNode shaderNode(surfaceShader);
	MString shaderName = shaderNode.name();
	MString shaderType = shaderNode.typeName();

	if (shaderType == "RPRUberMaterial") // this node is Uber
		return std::make_tuple(true, surfaceShader);

	// try find connected Uber
	// get ambientColor connection (this attribute can be used as input for the material for the curve)
	MPlug materialPlug = shaderNode.findPlug("ambientColor");
	if (materialPlug.isNull())
		return std::make_tuple(false, MObject::kNullObj);

	// try to get UberMaterial node
	MPlugArray shaderConnections;
	materialPlug.connectedTo(shaderConnections, true, false);
	if (shaderConnections.length() == 0)
		return std::make_tuple(false, MObject::kNullObj);

	// try to get shader from found node
	MObject uberMObj = shaderConnections[0].node();

	MFnDependencyNode uberFNode(uberMObj);
	MString ubershaderName = uberFNode.name();
	MString ubershaderType = uberFNode.typeName();

	if (ubershaderType == "RPRUberMaterial") // found node is Uber
		return std::make_tuple(true, uberMObj);

	return std::make_tuple(false, MObject::kNullObj);
}

MObject TryFindUber(MDagPath& path)
{
	MStatus status;

	MObject rootNode = path.node();
	MItDependencyGraph itdep(
		rootNode,
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

		// check if Uber is connected to hair shader
		if (connectionNodeTypename == "RPRUberMaterial")
		{
			return itdep.currentItem();
		}

		// check if uber is connected in rpr panel
		else if (connectionNodeTypename == "hairSystem")
		{
			MPlug hairColorPlug = connectionNode.findPlug("rprHairMaterial", &status);
			CHECK_MSTATUS(status);
			if (hairColorPlug.isNull())
				continue;

			MPlugArray connections;
			hairColorPlug.connectedTo(connections, true, false, &status);

			if (connections.length() == 0)
				continue;

			MPlug conn = connections[0];
			MFn::Type type = conn.node().apiType();

			MObject hairMaterialObject = conn.node(&status);
			CHECK_MSTATUS(status);

			return hairMaterialObject;
		}
	}

	// dummy return for the compiler
	return MObject::kNullObj;
}

MObject GetUberMObject(MDagPath& path)
{
	MObjectArray shdrs = getConnectedShaders(path);

	if (shdrs.length() == 0)
		return TryFindUber(path);

	// get hair shader node
	MObject surfaceShader = shdrs[0];
	MFnDependencyNode shaderNode(surfaceShader);

	MString shaderName = shaderNode.name();
	MString shaderType = shaderNode.typeName();

	bool uberMaterrialFound = false;
	MObject uberMObj = MObject::kNullObj;
	std::tie(uberMaterrialFound, uberMObj) = GetUberMaterialFromHairShader(surfaceShader);
	if (!uberMaterrialFound)
		return MObject::kNullObj;

	return uberMObj;
}

bool FireRenderHair::ApplyMaterial(void)
{
	if (m_Curves.empty())
		return false;

	MObject node = Object();
	MDagPath path = MDagPath::getAPathTo(node);

	// try getting uber
	MObject uberMObj = GetUberMObject(path);	

	frw::Shader shader;
	if (!uberMObj.isNull())
		shader = m.context->GetShader(uberMObj);

	if (!shader.IsValid())
	{
		// no uber => try creating uber from node attributes
		shader = ParseNodeAttributes(node, Scope());
	}

	if (!shader.IsValid())
		return false;

	// apply Uber material to Curves
	for (frw::Curve& curve : m_Curves)
	{
		if (curve.IsValid())
			curve.SetShader(shader);
	}

	return true;
}

void FireRenderHair::ApplyTransform(void)
{
	if (m_Curves.empty())
		return;

	const MObject& node = Object();
	MFnDagNode fnDagNode(node);
	MDagPath path = MDagPath::getAPathTo(node);

	// get transform
	MMatrix matrix = path.inclusiveMatrix();
	m_matrix = matrix;

	// convert Maya mesh in cm to m
	MMatrix scaleM;
	scaleM.setToIdentity();
	scaleM[0][0] = scaleM[1][1] = scaleM[2][2] = GetSceneUnitsConversionCoefficient();
	matrix *= scaleM;
	float mfloats[4][4];
	matrix.get(mfloats);

	// apply transform to curves
	for (frw::Curve& curve : m_Curves)
	{
		if (curve.IsValid())
			curve.SetTransform(false, *mfloats);
	}
}

/* from RadeonProRender.h :
	*  A rpr_curve is a set of curves
	*  A curve is a set of segments
	*  A segment is always composed of 4 3D points
*/
const unsigned int PointsPerSegment = 4;

void ProcessHairPoints(
	std::vector<rpr_uint>& outCurveIndicesData,
	unsigned int offset,
	unsigned int length
)
{
	rpr_uint currIdx = offset;
	for (unsigned int idx = 0; idx < length; idx++)
	{
		outCurveIndicesData.push_back(currIdx++);

		// duplicate index of last point in segment if necessary
		if (outCurveIndicesData.size() % PointsPerSegment != 0)
			continue;

		if (idx < (length - 1))
			outCurveIndicesData.push_back(outCurveIndicesData.back());
	}
}

void ProcessHairTail(std::vector<rpr_uint>& outCurveIndicesData)
{
	// Number of points that should be added 
	unsigned int tail = outCurveIndicesData.size() % PointsPerSegment;
	if (tail != 0)
		tail = PointsPerSegment - tail;

	// Extend data indices array if necessary 
	// Segment of RPR curve should always be composed of 4 3D points
	for (unsigned int idx = 0; idx < tail; idx++)
	{
		outCurveIndicesData.push_back(outCurveIndicesData.back());
	}
}

template <typename T>
void ProcessHairWidth(
	std::vector<float>& outRadiuses,
	const T* width,
	const std::vector<rpr_uint>& curveIndicesData)
{
	// ensure correct inputs
	assert(width != nullptr);
	assert(curveIndicesData.size() % PointsPerSegment == 0);

	const unsigned int segmentsInCurve = (unsigned int)(curveIndicesData.size()) / PointsPerSegment;

	// N = 2*(number of segments)
	// In RPR we set 2 widths per segment (segment is 4 points)
	for (unsigned int idx = 0; idx < segmentsInCurve; ++idx)
	{
		// bottom circle
		rpr_uint controlPointIdx = curveIndicesData[idx * PointsPerSegment];
		float radius = (float)width[controlPointIdx] * 0.5f;
		outRadiuses.push_back(radius);

		// top circle
		controlPointIdx = curveIndicesData[idx * PointsPerSegment + (PointsPerSegment - 1)];
		radius = (float)width[controlPointIdx] * 0.5f;
		outRadiuses.push_back(radius);
	}
}

std::tuple<unsigned int, unsigned int> GetHairLengthOffset(const XGenSplineAPI::XgItSpline& splineIt, unsigned int currCurveIdx)
{
	// find length of current segment and its offset in data arrays
	const unsigned int* primitiveInfos = splineIt.primitiveInfos();
	const unsigned int  stride = splineIt.primitiveInfoStride();

	unsigned int length = primitiveInfos[currCurveIdx * stride + 1];
	unsigned int offset = primitiveInfos[currCurveIdx * stride];

	return std::make_tuple(length, offset);
}

struct CurvesBatchData
{
	std::vector<rpr_uint> m_indicesData;
	std::vector<int> m_numPointsPerSegment;
	std::vector<float> m_radiuses; // width
	std::vector<float> m_uvCoord;
	unsigned int m_pointCount;
	const float* m_points;

	CurvesBatchData(void)
		: m_indicesData()
		, m_numPointsPerSegment()
		, m_radiuses() // In RPR we set 2 widths per segment (segment is 4 points)
		, m_uvCoord() // RPR accepts only one UV pair per curve
		, m_pointCount(0) // splineIt.vertexCount() returns wrong number - it returns number of vertexes used, not size of vertex array, which is different number when density mask is used
		, m_points(nullptr)
	{}

	using IHair = Ephere::Plugins::Ornatrix::IHair;
	void Init(const std::shared_ptr<IHair>& sourceHair)
	{
		m_pointCount = sourceHair->GetVertexCount();
		m_indicesData.reserve(m_pointCount * 2);

		int curveCount = sourceHair->GetStrandCount();
		m_uvCoord.reserve(curveCount * 2);
		m_numPointsPerSegment.reserve(curveCount);
		m_radiuses.reserve(m_pointCount);
	}

	void Init(const XGenSplineAPI::XgItSpline& splineIt)
	{
		m_points = splineIt.positions(0)->getValue();

		const unsigned int  curveCount = splineIt.primitiveCount();

		m_indicesData.reserve(splineIt.vertexCount() * 2);
		m_numPointsPerSegment.reserve(curveCount);
		m_radiuses.reserve(splineIt.vertexCount()); // array of N float. If curve is not tapered, N = curveCount. If curve is tapered, N = 2*(number of segments)
		m_uvCoord.reserve(2 * curveCount);
	}

	void Init(MRenderLineArray& mainLines)
	{
		int curveCount = mainLines.length();
		for (int idx = 0; idx < curveCount; ++idx)
		{
			MStatus status;
			MRenderLine renderLine = mainLines.renderLine(idx, &status);
			MVectorArray lineVtxs = renderLine.getLine();
			m_pointCount += lineVtxs.length();
		}

		m_indicesData.reserve(m_pointCount * 2);
		m_numPointsPerSegment.reserve(curveCount);
		m_radiuses.reserve(m_pointCount); // array of N float. If curve is not tapered, N = curveCount. If curve is tapered, N = 2*(number of segments)
		m_uvCoord.reserve(2 * curveCount);
	}

	frw::Curve CreateRPRCurve(frw::Context& currContext)
	{
		return currContext.CreateCurve(m_pointCount, m_points,
			sizeof(float) * 3, m_indicesData.size(), (rpr_uint)m_numPointsPerSegment.size(), m_indicesData.data(),
			m_radiuses.data(), m_uvCoord.data(), m_numPointsPerSegment.data());
	}
};

frw::Curve ProcessCurvesBatch(const XGenSplineAPI::XgItSpline& splineIt, frw::Context currContext)
{
	// create data buffers
	CurvesBatchData batchData;
	batchData.Init(splineIt);

	// for each primitive (for each hair in batch)
	for (unsigned int currCurveIdx = 0; currCurveIdx < splineIt.primitiveCount(); ++currCurveIdx)
	{
		unsigned int offset = 0;
		unsigned int length = 0;
		std::tie(length, offset) = GetHairLengthOffset(splineIt, currCurveIdx);

		// Write indices and copy points
		std::vector<rpr_uint> curveIndicesData;
		ProcessHairPoints(curveIndicesData, offset, length);
		ProcessHairTail(curveIndicesData);
		assert(curveIndicesData.size() % PointsPerSegment == 0);

		// Texcoord using the patch UV from the root point
		const SgVec2f* patchUVs = splineIt.patchUVs();
		batchData.m_uvCoord.push_back(patchUVs[offset][0]);
		batchData.m_uvCoord.push_back(patchUVs[offset][1]);
		// Number of points in the current curve
		const unsigned int segmentsInCurve = (unsigned int)(curveIndicesData.size()) / PointsPerSegment;
		batchData.m_numPointsPerSegment.push_back(segmentsInCurve);

		// add tmp_indicesData to output
		batchData.m_indicesData.insert(batchData.m_indicesData.end(), curveIndicesData.begin(), curveIndicesData.end());

		// Hair segments radiuses
		ProcessHairWidth(batchData.m_radiuses, splineIt.width(), curveIndicesData);
	}

	// find size of points array 
	// splineIt.vertexCount() returns wrong number - it returns number of vertexes used, not size of vertex array, which is different number when density mask is used
	if (!batchData.m_indicesData.empty())
	{
		auto maxIndexElement = std::max_element(batchData.m_indicesData.begin(), batchData.m_indicesData.end());
		batchData.m_pointCount = *maxIndexElement + 1;
	}

	// create RPR curve (create batch of hairs)
	return batchData.CreateRPRCurve(currContext);
}

bool GetCurvesData(XGenSplineAPI::XgFnSpline& out, MFnDagNode& curvesNode)
{
	// Stream out the spline data
	std::string data;
	MPlug       outPlug = curvesNode.findPlug("outRenderData");
	MObject     outObj = outPlug.asMObject();
	MPxData*    outData = MFnPluginData(outObj).data();
	// failed to get data
	if (outData == nullptr)
		return false;

	// Data found => dump data to stream
	std::stringstream opaqueStrm;
	outData->writeBinary(opaqueStrm);
	data = opaqueStrm.str();

	// Compute the padding bytes and number of array elements
	const unsigned int tail = data.size() % sizeof(unsigned int);
	const unsigned int padding = (tail > 0) ? sizeof(unsigned int) - tail : 0;
	const unsigned int nelements = (unsigned int)(data.size() / sizeof(unsigned int) + (tail > 0 ? 1 : 0));

	size_t sampleSize = nelements * sizeof(unsigned int) - padding;
	float sampleTime = 0.0f; // this seems to be an irrelevant parameter

	// get splines
	if (!out.load(opaqueStrm, sampleSize, sampleTime))
		return false;

	// apply masks etc.
	if (!out.executeScript())
		return false;

	return true;
}

void FireRenderHair::Freshen(bool shouldCalculateHash)
{
	detachFromScene();
	clear();

	auto node = Object();
	MFnDagNode fnDagNode(node);
	MString name = fnDagNode.fullPathName();

	bool haveCurves = CreateCurves();

	if (haveCurves)
	{
		MDagPath path = MDagPath::getAPathTo(node);
		if (path.isVisible())
		{
			for (int i = 0; i < m_Curves.size(); i++)
			{
				std::string shapeName = std::string(name.asChar()) + "_" + std::to_string(i);
				m_Curves[i].SetName(shapeName.c_str());
			}

			attachToScene();
		}

		setRenderStats(path);
	}

	FireRenderNode::Freshen(shouldCalculateHash);
}

void FireRenderHair::clear()
{
	m_Curves.clear();

	FireRenderObject::clear();
}

void FireRenderHair::attachToScene()
{
	if (m_isVisible)
		return;

	if (auto scene = context()->GetScene())
	{
		for (frw::Curve& curve : m_Curves)
			scene.Attach(curve);

		m_isVisible = true;
	}
}

void FireRenderHair::detachFromScene()
{
	if (!m_isVisible)
		return;

	if (auto scene = context()->GetScene())
	{
		for (frw::Curve& curve : m_Curves)
			scene.Detach(curve);
	}

	m_isVisible = false;
}

void FireRenderHair::setPrimaryVisibility(bool primaryVisibility)
{
	for (frw::Curve& curve : m_Curves)
	{
		curve.SetPrimaryVisibility(primaryVisibility);
	}
}

void FireRenderHair::setReflectionVisibility(bool reflectionVisibility)
{
	for (frw::Curve& curve : m_Curves)
	{
		curve.SetReflectionVisibility(reflectionVisibility);
	}
}

void FireRenderHair::setRefractionVisibility(bool refractionVisibility)
{
	for (frw::Curve& curve : m_Curves)
	{
		curve.setRefractionVisibility(refractionVisibility);
	}
}

void FireRenderHair::setCastShadows(bool castShadow)
{
	for (frw::Curve& curve : m_Curves)
	{
		curve.SetShadowFlag(castShadow);
	}
}

void FireRenderHair::setReceiveShadows(bool receiveShadow)
{
	for (frw::Curve& curve : m_Curves)
	{
		curve.SetReceiveShadowFlag(receiveShadow);
	}
}

void FireRenderHair::setRenderStats(MDagPath dagPath)
{
	if (!dagPath.isValid())
		return;

	MFnDependencyNode depNode(dagPath.node());

	MPlug visibleInReflectionsPlug = depNode.findPlug("visibleInReflections");
	if (!visibleInReflectionsPlug.isNull())
	{
		MString dbgName = visibleInReflectionsPlug.name();

		bool visibleInReflections = false;
		visibleInReflectionsPlug.getValue(visibleInReflections);
		setReflectionVisibility(visibleInReflections);
	}

	MPlug visibleInRefractionsPlug = depNode.findPlug("visibleInRefractions");
	if (!visibleInRefractionsPlug.isNull())
	{
		bool visibleInRefractions = false;
		visibleInRefractionsPlug.getValue(visibleInRefractions);
		setRefractionVisibility(visibleInRefractions);
	}

	MPlug castsShadowsPlug = depNode.findPlug("castsShadows");
	if (!castsShadowsPlug.isNull())
	{
		bool castsShadows = false;
		castsShadowsPlug.getValue(castsShadows);
		setCastShadows(castsShadows);
	}

	MPlug receivesShadowsPlug = depNode.findPlug("receiveShadows");
	if (!receivesShadowsPlug.isNull())
	{
		bool recievesShadows = false;
		receivesShadowsPlug.getValue(recievesShadows);
		setReceiveShadows(recievesShadows);
	}
}

FireRenderHairXGenGrooming::FireRenderHairXGenGrooming(FireRenderContext* context, const MDagPath& dagPath)
	: FireRenderHair(context, dagPath)
{}

FireRenderHairXGenGrooming::~FireRenderHairXGenGrooming()
{}

bool FireRenderHairXGenGrooming::CreateCurves()
{
	MObject node = Object();
	MFnDagNode fnDagNode(node);

	// get curves data
	XGenSplineAPI::XgFnSpline splines;
	if (!GetCurvesData(splines, fnDagNode))
		return false;

	// create rpr curves (hair batch) for each primitive batch
	for (XGenSplineAPI::XgItSpline splineIt = splines.iterator(); !splineIt.isDone(); splineIt.next())
	{
		m_Curves.push_back(ProcessCurvesBatch(splineIt, Context()));
	}

	// apply transform to curves
	ApplyTransform();

	// apply material to curves
	ApplyMaterial();

	return (m_Curves.size() > 0);
}

FireRenderHairOrnatrix::FireRenderHairOrnatrix(FireRenderContext* context, const MDagPath& dagPath)
	: FireRenderHair(context, dagPath)
{}

FireRenderHairOrnatrix::~FireRenderHairOrnatrix()
{}

bool EnsureValidOrnatrixHairBatch(const std::shared_ptr<Ephere::Plugins::Ornatrix::IHair>& sourceHair)
{
	bool isValidated = sourceHair->ValidateVertexStrandIndices();
	if (!isValidated)
		return false;

	bool hasStrand2ObjTransforms = sourceHair->HasStrandToObjectTransforms();
	if (!hasStrand2ObjTransforms)
		return false;

	if (!sourceHair->HasWidths())
		return false;

	if (!sourceHair->HasStrandToObjectTransforms())
		return false;

	return true;
}

void ProcessOrnatrixTextureCoordinates(
	const std::shared_ptr<Ephere::Plugins::Ornatrix::IHair>& sourceHair, 
	int currStrandVertexCount,
	int currCurveIdx,
	std::vector<int>& firstVertexIndices,
	CurvesBatchData& batchData)
{
	// texture coords
	int countTextureChannels = sourceHair->GetTextureCoordinateChannelCount();
	if (countTextureChannels == 0)
		return;

	// RPR supports only 1 channel!
	/*if (countTextureChannels > 1)
		display some warning; */

	int channel = 0;

	Ephere::Ornatrix::IHair::StrandDataType strandDataType = sourceHair->GetTextureCoordinateDataType(channel);

	std::vector<Ephere::Ornatrix::TextureCoordinate> coords(currStrandVertexCount);
	sourceHair->GetTextureCoordinates(
		channel,
		firstVertexIndices[currCurveIdx],
		currStrandVertexCount,
		coords.data(),
		Ephere::Ornatrix::IHair::PerVertex);

	// RPR supports only one uv coordinate pair per hair strand! Thus we pass UV of the root point
	batchData.m_uvCoord.push_back(coords[0].x());
	batchData.m_uvCoord.push_back(coords[0].y());
}

frw::Curve ProcessCurvesBatch(const std::shared_ptr<Ephere::Plugins::Ornatrix::IHair>& sourceHair, frw::Context currContext)
{
	// ensure hair is described in supported way
	assert(EnsureValidOrnatrixHairBatch(sourceHair));

	// create data buffers
	CurvesBatchData batchData;
	batchData.Init(sourceHair);

	// get overall batch data
	int strandCount = sourceHair->GetStrandCount();

	std::vector<int> pointCounts(strandCount);
	sourceHair->GetStrandPointCounts(0, int(pointCounts.size()), pointCounts.data());

	std::vector<int> firstVertexIndices(strandCount);
	sourceHair->GetStrandFirstVertexIndices(0, int(firstVertexIndices.size()), firstVertexIndices.data());

	// - vertices
	std::vector<Ephere::Ornatrix::Vector3> vertices = sourceHair->GetVerticesVector(0, batchData.m_pointCount, Ephere::Ornatrix::IHair::Strand);
	batchData.m_points = &vertices[0][0];

	// - hairs widths
	std::vector<float> width(batchData.m_pointCount);
	sourceHair->GetWidths(firstVertexIndices[0], batchData.m_pointCount, width.data());

	// - vertex coords tranformations
	std::vector <Ephere::Ornatrix::Xform3> strand2ojb (strandCount);
	sourceHair->GetStrandToObjectTransforms(0, strandCount, strand2ojb.data());

	// for each primitive (for each hair in batch)
	unsigned int offset = 0;
	int currStrandVertexCount = 0;

	for (int currCurveIdx = 0; currCurveIdx < strandCount; ++currCurveIdx, offset += currStrandVertexCount)
	{
		// get current curve
		currStrandVertexCount = sourceHair->GetStrandPointCount(currCurveIdx);
		assert(currStrandVertexCount == pointCounts[currCurveIdx]);

		// transform vertexes from local space
		for (int currVtxIdx = 0; currVtxIdx < currStrandVertexCount; currVtxIdx++)
		{
			Ephere::Ornatrix::Vector3& tcoord = vertices[currVtxIdx + offset];
			tcoord = strand2ojb[currCurveIdx] * tcoord;
		}

		// texture coords
		ProcessOrnatrixTextureCoordinates(sourceHair, currStrandVertexCount, currCurveIdx, firstVertexIndices, batchData);

		// convert data grabbed from ornatrix to rpr
		unsigned int length = pointCounts[currCurveIdx];

		// Write indices and copy points
		std::vector<rpr_uint> curveIndicesData;
		ProcessHairPoints(curveIndicesData, offset, length);
		ProcessHairTail(curveIndicesData);
		assert(curveIndicesData.size() % PointsPerSegment == 0);

		// Number of points in the current curve
		const unsigned int segmentsInCurve = (unsigned int)(curveIndicesData.size()) / PointsPerSegment;
		batchData.m_numPointsPerSegment.push_back(segmentsInCurve);

		// add tmp_indicesData to output
		batchData.m_indicesData.insert(batchData.m_indicesData.end(), curveIndicesData.begin(), curveIndicesData.end());

		// Hair segments radiuses
		ProcessHairWidth(batchData.m_radiuses, width.data(), curveIndicesData);
	}

	// create RPR curve (create batch of hairs)
	return batchData.CreateRPRCurve(currContext);
}

bool FireRenderHairOrnatrix::CreateCurves()
{
	using IHair = Ephere::Plugins::Ornatrix::IHair;
	MObject node = Object();

	// get curves data
	std::shared_ptr<IHair> sourceHair = Ephere::Plugins::Ornatrix::GetHairInterface(node);
	if (sourceHair == nullptr)
		return false;

	// create rpr curves
	m_Curves.push_back(ProcessCurvesBatch(sourceHair, Context()));

	// apply transform to curves
	ApplyTransform();

	// apply material to curves
	ApplyMaterial();

	return (m_Curves.size() > 0);
}

FireRenderHairNHair::FireRenderHairNHair(FireRenderContext* context, const MDagPath& dagPath)
	: FireRenderHair(context, dagPath)
{}

FireRenderHairNHair::~FireRenderHairNHair()
{}

frw::Curve ProcessCurvesBatch(MRenderLineArray& mainLines, frw::Context currContext)
{	
	MStatus status;

	// create data buffers
	CurvesBatchData batchData;
	batchData.Init(mainLines);

	std::vector<float> vertices;
	vertices.reserve(batchData.m_pointCount * 3);
	std::vector<double> widths;
	widths.reserve(batchData.m_pointCount * 3);

	// for each primitive (for each hair)
	int currStrandVertexCount = 0;
	unsigned int offset = 0;

	int countMainLines = mainLines.length();
	for (int idx = 0; idx < countMainLines; ++idx, offset += currStrandVertexCount)
	{
		MRenderLine renderLine = mainLines.renderLine(idx, &status);
		MVectorArray lineVtxs = renderLine.getLine();
		currStrandVertexCount = lineVtxs.length();

		// Copy points
		for (unsigned int vtxIdx = 0; vtxIdx < lineVtxs.length(); ++vtxIdx)
		{
			MVector& tVect = lineVtxs[vtxIdx];
			vertices.push_back((float)tVect.x);
			vertices.push_back((float)tVect.y);
			vertices.push_back((float)tVect.z);
		}

		// Write indices
		std::vector<rpr_uint> curveIndicesData;
		ProcessHairPoints(curveIndicesData, offset, currStrandVertexCount);
		ProcessHairTail(curveIndicesData);
		assert(curveIndicesData.size() % PointsPerSegment == 0);

		// Number of segments in the current curve
		const unsigned int segmentsInCurve = (unsigned int)(curveIndicesData.size()) / PointsPerSegment;
		batchData.m_numPointsPerSegment.push_back(segmentsInCurve);

		// add tmp_indicesData to output
		batchData.m_indicesData.insert(batchData.m_indicesData.end(), curveIndicesData.begin(), curveIndicesData.end());

		// Hair segments radiuses
		MDoubleArray width = renderLine.getWidth();
		std::fill_n(std::back_inserter(widths), width.length(), 0.0);
		width.get(&widths[offset]);
		ProcessHairWidth(batchData.m_radiuses, widths.data(), curveIndicesData);

		// Texcoord
		MDoubleArray parameter = renderLine.getParameter();
		for (unsigned int idx = 0; idx < parameter.length(); ++idx)
		{
			float param = (float)parameter[idx];
			batchData.m_uvCoord.push_back(param);
			batchData.m_uvCoord.push_back(param);
		}
	}

	batchData.m_points = vertices.data();

	// create RPR curve (create batch of hairs)
	return batchData.CreateRPRCurve(currContext);
}

bool FireRenderHairNHair::CreateCurves()
{
	MObject node = Object();

	// ensure valid input
	bool hasNHairs = node.hasFn(MFn::kPfxGeometry);
	if (!hasNHairs)
		return false;

	// get curves data
	MStatus status;
	MFnPfxGeometry hHairs(node, &status);
	assert(status == MStatus::kSuccess);

	MRenderLineArray mainLines;
	MRenderLineArray leafLines;
	MRenderLineArray flowerLines;
	status = hHairs.getLineData(mainLines, leafLines, flowerLines,
		true, //doLines
		false, //doTwist
		true, //doWidth
		false, //doFlatness
		true, //doParameter
		false, //doColor
		false, //doIncandescence
		false, //doTransparency
		false //worldSpace
		);
	assert(status == MStatus::kSuccess);
	int countMainLines = mainLines.length();
	int countLeafLines = leafLines.length();
	int countFlowerLines = flowerLines.length();

	// create rpr curves
	m_Curves.push_back(ProcessCurvesBatch(mainLines, Context()));

	// clean up
	mainLines.deleteArray();
	leafLines.deleteArray();
	flowerLines.deleteArray();

	// apply transform to curves
	ApplyTransform();

	// apply material to curves
	ApplyMaterial();

	return (m_Curves.size() > 0);
}

static void HairShaderDirtyCallback(MObject& node, void* clientData)
{
	DebugPrint("CALLBACK > HairShaderDirtyCallback(%s)", node.apiTypeStr());
	if (auto self = static_cast<FireRenderHair*>(clientData))
	{
		assert(node != self->Object());
		self->OnShaderDirty();
	}
}

void FireRenderHair::OnShaderDirty()
{
	setDirty();
}

void FireRenderHair::RegisterCallbacks()
{
	FireRenderNode::RegisterCallbacks();

	if (m_Curves.empty())
		return;

	MObject node = Object();
	MDagPath path = MDagPath::getAPathTo(node);

	// get hair shader node
	MObjectArray shdrs = getConnectedShaders(path);
	if (shdrs.length() == 0)
		return;

	MObject surfaceShader = shdrs[0];
	if (!surfaceShader.isNull())
	{
		AddCallback(MNodeMessage::addNodeDirtyCallback(surfaceShader, HairShaderDirtyCallback, this));
	}
}

void FireRenderHairNHair::RegisterCallbacks()
{
	FireRenderNode::RegisterCallbacks();

	if (m_Curves.empty())
		return;

	MObject node = Object();
	MDagPath path = MDagPath::getAPathTo(node);

	MObject shader = TryFindUber(path);
	if (shader.isNull())
		return;

	AddCallback(MNodeMessage::addNodeDirtyCallback(shader, HairShaderDirtyCallback, this));
}

MObject GetHairSystemFromHairShape(MObject hairObject)
{
	// ensure correct input
	if (hairObject.isNull())
	{
		return MObject::kNullObj;
	}

	// traverse graph to hairSystemShape node
	// - get connection attribute
	MFnDagNode mfn_hairObject(hairObject);
	MObject attr = mfn_hairObject.attribute("renderHairs");
	if (attr.isNull())
		return MObject::kNullObj; // attribute not found

	MPlug renderHairsPlug = mfn_hairObject.findPlug(attr);
	// - get connected object
	MPlugArray connections;
	renderHairsPlug.connectedTo(connections, true, false);
	for (auto connection : connections)
	{
		// assuming that first found connection is node that we need
		MObject connNode = connection.node();
		return connNode;
	}

	// failed to find hairSystemShape node
	return MObject::kNullObj;
}

frw::Shader FireRenderHairNHair::ParseNodeAttributes(MObject hairObject, const FireMaya::Scope& scope)
{
	// get hairSystemShape object from pfxHairShape object
	MObject hairSystemShapeObj = GetHairSystemFromHairShape(hairObject);
	if (hairSystemShapeObj.isNull())
		return frw::Shader(); // failed to find hairSystemShape node

	// get attribute values from the node
	MFnDagNode dagHairSystemShapeObj(hairSystemShapeObj);

	// - get opacity
	MObject opacityAttr = dagHairSystemShapeObj.attribute("opacity");
	if (opacityAttr.isNull())
		return frw::Shader();

	float opacity = findPlugTryGetValue(dagHairSystemShapeObj, opacityAttr, 1.0f);

	// - get hair color
	MObject hairColorAttr = dagHairSystemShapeObj.attribute("hairColor");
	if (hairColorAttr.isNull())
		return frw::Shader();

	MColor hairColor = getColorAttribute(dagHairSystemShapeObj, hairColorAttr);

	// - get specularColor
	MObject specularColorAttr = dagHairSystemShapeObj.attribute("specularColor");
	if (specularColorAttr.isNull())
		return frw::Shader();

	MColor specularColor = getColorAttribute(dagHairSystemShapeObj, specularColorAttr);

	// - get specularPower
	MObject specularPowerAttr = dagHairSystemShapeObj.attribute("specularPower");
	if (specularPowerAttr.isNull())
		return frw::Shader();

	float specularPower = findPlugTryGetValue(dagHairSystemShapeObj, specularPowerAttr, 1.0f);

	// - get castShadows
	MObject castShadowsAttr = dagHairSystemShapeObj.attribute("castShadows");
	if (castShadowsAttr.isNull())
		return frw::Shader();

	bool castShadows = findPlugTryGetValue(dagHairSystemShapeObj, castShadowsAttr, 1) > 0;

	// - get hairColorScale (this is ramp)
	MObject hairColorScaleAttr = dagHairSystemShapeObj.attribute("hairColorScale");
	MPlug hairColorScalePlug = dagHairSystemShapeObj.findPlug(hairColorScaleAttr);
	if (hairColorScalePlug.isNull())
		return frw::Shader();

	std::vector<RampCtrlPoint<MColor>> outRampCtrlPoints;
	bool isRampParced = GetRampValues<MColorArray>(hairColorScalePlug, outRampCtrlPoints);
	if (!isRampParced)
		return frw::Shader();

	const unsigned int bufferSize = 256; // same as in Blender
	frw::BufferNode rampNode = CreateRPRRampNode(outRampCtrlPoints, scope, bufferSize);
	frw::LookupNode lookupNode(scope.MaterialSystem(), frw::LookupTypeUV0);
	frw::ArithmeticNode bufferLookupMulNode(scope.MaterialSystem(), frw::OperatorMultiply, lookupNode, frw::Value(bufferSize, bufferSize, bufferSize));
	frw::ArithmeticNode selectZ(scope.MaterialSystem(), frw::OperatorSelectZ, bufferLookupMulNode);
	rampNode.SetUV(selectZ);
	frw::ArithmeticNode coloredRamp(scope.MaterialSystem(), frw::OperatorMultiply, rampNode, frw::Value(hairColor.r, hairColor.g, hairColor.b));

	// create uber from read values
	frw::Shader translatedHairShader = frw::Shader(context()->GetMaterialSystem(), frw::ShaderType::ShaderTypeStandard);
	translatedHairShader.xSetValue(RPR_MATERIAL_INPUT_UBER_DIFFUSE_COLOR, coloredRamp);
	translatedHairShader.xSetValue(RPR_MATERIAL_INPUT_UBER_DIFFUSE_WEIGHT, { 1.0f });
	translatedHairShader.xSetValue(RPR_MATERIAL_INPUT_UBER_TRANSPARENCY, { 1 - opacity });
	translatedHairShader.xSetValue(RPR_MATERIAL_INPUT_UBER_REFLECTION_COLOR, { specularColor.r, specularColor.g, specularColor.b });
	translatedHairShader.xSetValue(RPR_MATERIAL_INPUT_UBER_REFLECTION_WEIGHT, { 0.5f });
	translatedHairShader.xSetValue(RPR_MATERIAL_INPUT_UBER_REFRACTION_WEIGHT, { 0.05*specularPower });

	return translatedHairShader;
}

void SetHairPrimaryVisibility(FireRenderHair* pHair, MDagPath& dagPath)
{
	assert(pHair != nullptr);
	assert(dagPath.isValid());

	MFnDependencyNode depNode(dagPath.node());

	MPlug primaryVisibilityPlug = depNode.findPlug("primaryVisibility");
	if (!primaryVisibilityPlug.isNull())
	{
		bool primaryVisibility = true;
		primaryVisibilityPlug.getValue(primaryVisibility);
		pHair->setPrimaryVisibility(primaryVisibility);
	}
}

void FireRenderHairXGenGrooming::setRenderStats(MDagPath dagPath)
{
	if (!dagPath.isValid())
		return;

	SetHairPrimaryVisibility(this, dagPath);

	FireRenderHair::setRenderStats(dagPath);
}

void FireRenderHairOrnatrix::setRenderStats(MDagPath dagPath)
{
	if (!dagPath.isValid())
		return;

	SetHairPrimaryVisibility(this, dagPath);

	FireRenderHair::setRenderStats(dagPath);
}

void FireRenderHairNHair::setRenderStats(MDagPath dagPath)
{
	if (!dagPath.isValid())
		return;

	// in nhair we have to check pfxHairShape object
	MObject hairObject = dagPath.node();

	// get hairSystemShape object from pfxHairShape object
	MObject hairSystemShapeObj = GetHairSystemFromHairShape(hairObject);
	if (hairSystemShapeObj.isNull())
		return;

	MFnDagNode dagHairSystemShapeObj(hairSystemShapeObj);
	MDagPath hairSystemShapePath;
	MStatus status = dagHairSystemShapeObj.getPath(hairSystemShapePath);
	assert(status == MStatus::kSuccess);

	MFnDependencyNode depNode(hairSystemShapePath.node());

	MPlug primaryVisibilityPlug = depNode.findPlug("nhairIsVisible");
	if (!primaryVisibilityPlug.isNull())
	{
		bool primaryVisibility = true;
		primaryVisibilityPlug.getValue(primaryVisibility);
		setPrimaryVisibility(primaryVisibility);
	}

	MPlug visibleInReflectionsPlug = depNode.findPlug("nhairVisibleInReflections");
	if (!visibleInReflectionsPlug.isNull())
	{
		bool visibleInReflections = false;
		visibleInReflectionsPlug.getValue(visibleInReflections);
		setReflectionVisibility(visibleInReflections);
	}

	MPlug visibleInRefractionsPlug = depNode.findPlug("nhairVisibleInRefractions");
	if (!visibleInRefractionsPlug.isNull())
	{
		bool visibleInRefractions = false;
		visibleInRefractionsPlug.getValue(visibleInRefractions);
		setRefractionVisibility(visibleInRefractions);
	}

	MPlug castsShadowsPlug = depNode.findPlug("castsShadows");
	if (!castsShadowsPlug.isNull())
	{
		bool castsShadows = false;
		castsShadowsPlug.getValue(castsShadows);
		setCastShadows(castsShadows);
	}

	MPlug receivesShadowsPlug = depNode.findPlug("receiveShadows");
	if (!receivesShadowsPlug.isNull())
	{
		bool recievesShadows = false;
		receivesShadowsPlug.getValue(recievesShadows);
		setReceiveShadows(recievesShadows);
	}
}


