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

#if defined(MAYA2018)
#include <XGen/XgSplineAPI.h>
#else
#include <Xgen/src/xgsculptcore/api/XgSplineAPI.h>
#endif

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

bool FireRenderHair::ApplyMaterial(void)
{
	if (m_Curves.empty())
		return false;

	auto node = Object();
	MDagPath path = MDagPath::getAPathTo(node);

	// get hair shader node
	MObjectArray shdrs = getConnectedShaders(path);

	if (shdrs.length() == 0)
		return false;

	MObject surfaceShader = shdrs[0];
	MFnDependencyNode shaderNode(surfaceShader);

	MString shaderName = shaderNode.name();
	MString shaderType = shaderNode.typeName();

	MObject uberMObj;
	bool uberMaterrialFound = false;
	std::tie(uberMaterrialFound, uberMObj) = GetUberMaterialFromHairShader(surfaceShader);
	if (!uberMaterrialFound)
		return false;

	frw::Shader shader = m.context->GetShader(uberMObj);
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
	scaleM[0][0] = scaleM[1][1] = scaleM[2][2] = 0.01;
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

void ProcessHairWidth(
	std::vector<float>& outRadiuses,
	const float* width,
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
		float radius = width[controlPointIdx] * 0.5f;
		outRadiuses.push_back(radius);

		// top circle
		controlPointIdx = curveIndicesData[idx * PointsPerSegment + (PointsPerSegment - 1)];
		radius = width[controlPointIdx] * 0.5f;
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

void FireRenderHair::Freshen()
{
	clear();

	auto node = Object();
	MFnDagNode fnDagNode(node);
	MString name = fnDagNode.name();

	bool haveCurves = CreateCurves();

	if (haveCurves)
	{
		MDagPath path = MDagPath::getAPathTo(node);
		if (path.isVisible())
			attachToScene();
	}

	FireRenderNode::Freshen();
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

	Ephere::Ornatrix::IHair::CoordinateSpace corrdSpace = sourceHair->GetCoordinateSpace();
	if (corrdSpace != Ephere::Ornatrix::IHair::Strand)
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

	// RPR supports only 1 channel!
	/*if (countTextureChannels > 1)
		display some warning; */

	int channel = 0;
	Ephere::Ornatrix::IHair::StrandDataType strandDataType = sourceHair->GetTextureCoordinateDataType(channel);
	if (strandDataType == Ephere::Ornatrix::IHair::PerVertex)
	{
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
		batchData.m_uvCoord.push_back(coords[1].x());
		batchData.m_uvCoord.push_back(coords[1].y());
	}
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
