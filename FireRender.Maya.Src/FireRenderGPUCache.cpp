
#include "FireRenderGPUCache.h"
#include "Context/FireRenderContext.h"
#include "FireRenderUtils.h"
#include "Context/TahoeContext.h"

#include <array>
#include <algorithm>
#include <vector>
#include <iterator>
#include <istream>
#include <ostream>
#include <sstream>
#include <iostream>  
#include <fstream>

#include <maya/MFnDependencyNode.h>
#include <maya/MPlug.h>
#include <maya/MMatrix.h>
#include <maya/MDagPath.h>
#include <maya/MSelectionList.h>
#include <maya/MAnimControl.h>
#include <maya/MEventMessage.h>

#include <Alembic/Abc/All.h>
#include <Alembic/AbcCoreOgawa/All.h>
#include <Alembic/AbcGeom/All.h>

using namespace Alembic::Abc;
using namespace Alembic::AbcGeom;

FireRenderGPUCache::FireRenderGPUCache(FireRenderContext* context, const MDagPath& dagPath) 
	: 	m_changedFile(true)
	,	m_curr_frameNumber(0)
	,	FireRenderMeshCommon(context, dagPath)
{}

FireRenderGPUCache::~FireRenderGPUCache()
{
	FireRenderGPUCache::clear();
}

bool FireRenderGPUCache::IsSelected(const MDagPath& dagPath) const
{
	MObject transformObject = dagPath.transform();
	bool isSelected = false;

	// get a list of the currently selected items 
	MSelectionList selected;
	MGlobal::getActiveSelectionList(selected);

	// iterate through the list of items returned
	for (unsigned int i = 0; i < selected.length(); ++i)
	{
		MObject obj;

		// returns the i'th selected dependency node
		selected.getDependNode(i, obj);

		if (obj == transformObject)
		{
			isSelected = true;
			break;
		}
	}

	return isSelected;
}

bool FireRenderGPUCache::IsMeshVisible(const MDagPath& meshPath, const FireRenderContext* context) const
{
	bool isVisible = meshPath.isVisible();

	return isVisible;
}

void FireRenderGPUCache::clear()
{
	m.elements.clear();
	FireRenderObject::clear();
}

// this function is identical to one in FireRenderMesh! 
// TODO: move it to common parent class!
void FireRenderGPUCache::Freshen(bool shouldCalculateHash)
{
	MDagPath meshPath = DagPath();
	MMatrix mMtx = meshPath.inclusiveMatrix();

	Rebuild();
	FireRenderNode::Freshen(shouldCalculateHash);
}

void FireRenderGPUCache::ReadAlembicFile(uint32_t frame /*= 0*/)
{
	MStatus res;
	
	// get name of alembic file from Maya node
	const MObject& node = Object();
	MFnDependencyNode nodeFn(node);
	MPlug plug = nodeFn.findPlug("cacheFileName", &res);
	CHECK_MSTATUS(res);

	std::string cacheFilePath = ProcessEnvVarsInFilePath<std::string, char>(plug.asString(&res).asChar());
	CHECK_MSTATUS(res);

	// ensure that file with such name exists
	const std::ifstream abcFile(cacheFilePath.c_str(), std::ios::in);
	if (!abcFile.good())
		return;

	// get Maya frame rate
	MTime::Unit timeUnit = MTime::uiUnit();
	MTime frameRate;
	frameRate.setUnit(timeUnit);
	double fFrameRate = frameRate.as(MTime::kSeconds);

	// file already read => load it from cache
	m_file = abcCache.find(cacheFilePath + std::to_string(frame) + std::to_string(fFrameRate));
	if (m_file != abcCache.end())
	{
		return;
	}

	// proceed reading file
	abcCache[cacheFilePath + std::to_string(frame) + std::to_string(fFrameRate)] = RPRAlembicWrapperCacheEntry();
	m_file = abcCache.find(cacheFilePath + std::to_string(frame) + std::to_string(fFrameRate));
	assert(m_file != abcCache.end());

	try
	{
		m_file->second.m_archive = IArchive(Alembic::AbcCoreOgawa::ReadArchive(), cacheFilePath);
	}
	catch (std::exception& e)
	{
		char error[100];
		sprintf(error, "open alembic error: %s\n", e.what());
		MGlobal::displayError(error);
		return;
	}

	if (!m_file->second.m_archive.valid())
		return;

	// get alembic time entries
	double oStartTime;
	double oEndTime;
	GetArchiveStartAndEndTime(m_file->second.m_archive, oStartTime, oEndTime);

	uint32_t getNumTimeSamplings = m_file->second.m_archive.getNumTimeSamplings();

	// get Alembic frame entry
	uint32_t abcFirstFrame = oStartTime / fFrameRate; // <= frame in Maya playback that corresponds to zero index of alembic animation record
	uint32_t abcLastFrame = oEndTime / fFrameRate; // <= frame in Maya playback that corresponds to last index of alembic animation record

	uint32_t sampleIdx = frame - abcFirstFrame;

	if (frame <= abcFirstFrame)
		sampleIdx = 0;

	if (frame > abcLastFrame)
		sampleIdx = abcLastFrame - abcFirstFrame;

	std::string errorMessage;
	if (m_file->second.m_storage.open(cacheFilePath, errorMessage) == false)
	{
		errorMessage = "AlembicStorage::open error: " + errorMessage;
		MGlobal::displayError(errorMessage.c_str());
		return;
	}

	m_file->second.m_scene = m_file->second.m_storage.read(sampleIdx, errorMessage);
	if (!m_file->second.m_scene)
	{
		errorMessage = "sample error: " + errorMessage;
		MGlobal::displayError(errorMessage.c_str());
		return;
	}
}

frw::Shader FireRenderGPUCache::GetAlembicShadingEngines(MObject gpucacheNode)
{
	// this is implementation that returns default shader
	frw::Shader placeholderShader = Scope().GetCachedShader(std::string("DefaultShaderForAlembic"));
	if (!placeholderShader)
	{
		placeholderShader = frw::Shader(context()->GetMaterialSystem(), frw::ShaderType::ShaderTypeStandard);
		placeholderShader.xSetValue(RPR_MATERIAL_INPUT_UBER_DIFFUSE_COLOR, {1.0f, 1.0f, 1.0f});
		placeholderShader.xSetValue(RPR_MATERIAL_INPUT_UBER_DIFFUSE_WEIGHT, {1.0f, 1.0f, 1.0f});

		Scope().SetCachedShader(std::string("DefaultShaderForAlembic"), placeholderShader);
	}

	return placeholderShader;
}

void FireRenderGPUCache::RebuildTransforms()
{
	MObject node = Object();
	MMatrix matrix = GetSelfTransform(); 
	MFnDagNode meshFn(node);

	MMatrix scaleM;
	scaleM.setToIdentity();
	scaleM[0][0] = scaleM[1][1] = scaleM[2][2] = GetSceneUnitsConversionCoefficient();
	matrix *= scaleM;

	for (auto& element : m.elements)
	{
		assert(element.shape);
		if (!element.shape)
			continue;

		// transform
		float(*f)[4][4] = reinterpret_cast<float(*)[4][4]>(element.TM.data());
		MMatrix elementTransform(*f);

		MMatrix mayaObjMatr = GetSelfTransform();

		elementTransform *= mayaObjMatr;
		elementTransform *= scaleM;

		float mfloats[4][4];
		elementTransform.get(mfloats);

		element.shape.SetTransform(&mfloats[0][0]);
	}

	// motion blur
	ProcessMotionBlur(meshFn);
}

void FireRenderGPUCache::ProcessShaders()
{
	FireRenderContext* context = this->context();

	for (auto& element : m.elements)
	{
		assert(element.shape);
		if (!element.shape)
			continue;

		element.shape.SetShader(nullptr);

		for (unsigned int shaderIdx = 0; shaderIdx < element.shadingEngines.size(); ++shaderIdx)
		{
			MObject& shadingEngine = element.shadingEngines[shaderIdx];
			element.shaders.push_back(context->GetShader(getSurfaceShader(shadingEngine), shadingEngine, this));

			std::vector<int>& faceMaterialIndices = m.faceMaterialIndices;
			std::vector<int> face_ids;
			face_ids.reserve(faceMaterialIndices.size());
			for (int faceIdx = 0; faceIdx < faceMaterialIndices.size(); ++faceIdx)
			{
				if (faceMaterialIndices[faceIdx] == shaderIdx)
					face_ids.push_back(faceIdx);
			}

			element.shape.SetPerFaceShader(element.shaders.back(), face_ids);

			frw::ShaderType shType = element.shaders.back().GetShaderType();
			if (shType == frw::ShaderTypeEmissive)
				m.isEmissive = true;

			if (element.shaders.back().IsShadowCatcher() || element.shaders.back().IsReflectionCatcher())
				continue;

			if ((shType == frw::ShaderTypeRprx) && (IsUberEmissive(element.shaders.back())))
			{
				m.isEmissive = true;
			}
		}
	}
}

void FireRenderGPUCache::Rebuild()
{
	MDagPath meshPath = DagPath();
	//*********************************
	// this is called every time alembic node is moved or params changed
	// optimizations will be added so that we reload file and rebuild mesh only when its necessary
	//*********************************
	// read alembic file
	bool needReadFile = m_changedFile;
	if (needReadFile)
	{
		MTime currTime = MAnimControl::currentTime();
		uint32_t currFrame = (uint32_t)currTime.as(MTime::uiUnit());
		ReadAlembicFile(currFrame);
		m_curr_frameNumber = currFrame;
		ReloadMesh(meshPath);
	}

	RebuildTransforms();
	setRenderStats(meshPath);

	MFnDagNode meshFn(Object());
	MObjectArray shadingEngines = GetShadingEngines(meshFn, Instance());
	AssignShadingEngines(shadingEngines);

	RegisterCallbacks();

	ProcessShaders();

	attachToScene();

	m.changed.mesh = false;
	m.changed.transform = false;
	m.changed.shader = false;
	m_changedFile = false;
}

void FireRenderGPUCache::ReloadMesh(const MDagPath& meshPath)
{
	MMatrix mMtx = meshPath.inclusiveMatrix();

	setVisibility(false);
	m.elements.clear();

	// node is not visible => skip
	if (!IsMeshVisible(meshPath, this->context()))
		return;

	std::vector<frw::Shape> shapes;
	std::vector<std::array<float, 16>> tmMatrs;
	GetShapes(shapes, tmMatrs);

	m.elements.resize(shapes.size());
	for (unsigned int i = 0; i < shapes.size(); i++)
	{
		m.elements[i].shape = shapes[i];
		m.elements[i].TM = tmMatrs[i];
	}
}

void GenerateIndicesByVtx(std::vector<int>& out, bool isTriangleMesh, const RPRAlembicWrapper::PolygonMeshObject* mesh)
{
	if (isTriangleMesh)
	{
		for (size_t idx = 0; idx < mesh->indices.size(); idx += 3)
		{
			out[idx] = mesh->indices[idx + 2];
			out[idx + 1] = mesh->indices[idx + 1];
			out[idx + 2] = mesh->indices[idx];
		}

		return;
	}

	uint32_t idx = 0;
	size_t countIndices = mesh->indices.size();

	for (uint32_t faceCount : mesh->faceCounts)
	{
		uint32_t currIdx = idx;

		for (uint32_t idxInPolygon = 1; idxInPolygon <= faceCount; idxInPolygon++)
		{
			out[idx] = mesh->indices[currIdx + faceCount - idxInPolygon];
			idx++;
		}
	}
}

void GenerateIndicesByFvr(std::vector<int>& out, const RPRAlembicWrapper::PolygonMeshObject* mesh)
{
	uint32_t idx = 0;
	for (uint32_t faceCount : mesh->faceCounts)
	{
		uint32_t currIdx = idx;

		for (uint32_t idxInPolygon = 0; idxInPolygon < faceCount; ++idxInPolygon)
		{
			out.push_back(idx++);
		}

		std::reverse(out.end() - faceCount, out.end());
	}
}

void GenerateIndicesArray(std::vector<int>& out, const std::string& key, const RPRAlembicWrapper::PolygonMeshObject* mesh, bool isTriangleMesh)
{
	if (key == "vtx")
	{
		GenerateIndicesByVtx(out, isTriangleMesh, mesh);
	}
	else if (key == "fvr")
	{
		GenerateIndicesByFvr(out, mesh);
	}
	else
	{
		assert(false); // NOT IMPLEMENTED!
	}
}

frw::Shape TranslateAlembicMesh(const RPRAlembicWrapper::PolygonMeshObject* mesh, frw::Context& context)
{
	// ensure RPR can process mesh
	for (uint32_t faceCount : mesh->faceCounts)
	{
		if (faceCount != 3 && faceCount != 4)
			return frw::Shape();
	}

	// get indices
	std::vector<int> vertexIndices(mesh->indices.size(), 0); // output indices of vertexes (3 for triangle and 4 for quad)

	// mesh have only triangles => simplified mesh processing
	bool isTriangleMesh = std::all_of(mesh->faceCounts.begin(), mesh->faceCounts.end(), [](int32_t f) {
		return f == 3;
	});

	// in alembic indexes could be stored in file ("vtx" tag) and could be expected to be simply ascending order ("fvr" tag)
	const std::shared_ptr<std::vector<std::pair<std::string, std::string>>>& keyScopeTags = mesh->keyScopeTag;
	assert(keyScopeTags);
	auto pointsIt = find_if(keyScopeTags->begin(), keyScopeTags->end(), [](const auto& pair) 
		{ return pair.first == "P"; });

	// in alembic indexes are reversed compared to what RPR expects
	assert(pointsIt != keyScopeTags->end());
	std::string pointsTag = pointsIt->second;

	GenerateIndicesArray(vertexIndices, pointsTag, mesh, isTriangleMesh);

	std::vector<int> normalIndices;
	if (mesh->N.data() != nullptr)
	{
		auto normalsIt = find_if(keyScopeTags->begin(), keyScopeTags->end(), [](const auto& pair)
			{ return pair.first == "N"; });

		assert(normalsIt != keyScopeTags->end());
		std::string normalsTag = normalsIt->second;

		if (normalsTag == pointsTag)
		{
			normalIndices = vertexIndices;
		}
		else
		{
			GenerateIndicesArray(normalIndices, normalsTag, mesh, isTriangleMesh);
		}
	}

	std::vector<int> uvIndices;
	if (mesh->UV.data() != nullptr)
	{
		auto uvsIt = find_if(keyScopeTags->begin(), keyScopeTags->end(), [](const auto& pair)
		{ return pair.first == "uv"; });

		assert(uvsIt != keyScopeTags->end());
		std::string uvsTag = uvsIt->second;

		if (uvsTag == pointsTag)
		{
			uvIndices = vertexIndices;
		}
		else
		{
			GenerateIndicesArray(uvIndices, uvsTag, mesh, isTriangleMesh);
		}
	}

	// data structures necessary for passing data to RPR
	const std::vector<RPRAlembicWrapper::Vector3f>& points = mesh->P;
	const std::vector<RPRAlembicWrapper::Vector3f>& normals = mesh->N;
	const std::vector<RPRAlembicWrapper::Vector2f>& uvs = mesh->UV;

	unsigned int uvSetCount = 1; // 1 uv set
	std::vector<const float*> output_submeshUVCoords;
	output_submeshUVCoords.reserve(uvSetCount);
	std::vector<size_t> output_submeshSizeCoords;
	output_submeshSizeCoords.reserve(uvSetCount);
	std::vector<const rpr_int*>	puvIndices;
	puvIndices.reserve(uvSetCount);

	// - this should be done for each uv set, but we support only one uv set in object loaded from alembic file for now
	output_submeshUVCoords.push_back((const float*)uvs.data());
	output_submeshSizeCoords.push_back(uvs.size());

	puvIndices.push_back(uvIndices.size() > 0 ?
		uvIndices.data() :
		nullptr);
	
	std::vector<int> texIndexStride(uvSetCount, sizeof(int));
	std::vector<int> multiUV_texcoord_strides(uvSetCount, sizeof(Float2));

	// pass data to RPR
	frw::Shape out = context.CreateMeshEx(
		(const float*)points.data(), points.size(), sizeof(RPRAlembicWrapper::Vector3f),
		(const float*)normals.data(), normals.size(), sizeof(RPRAlembicWrapper::Vector3f),
		nullptr, 0, 0,
		uvSetCount, output_submeshUVCoords.data(), output_submeshSizeCoords.data(), multiUV_texcoord_strides.data(),
		(const int*)vertexIndices.data(), sizeof(int),
		(const int*)normalIndices.data(), normalIndices.size() != 0 ? sizeof(int) : 0,
		puvIndices.data(), texIndexStride.data(),
		(const int*)mesh->faceCounts.data(), mesh->faceCounts.size()
	);

	return out;
}

void FireRenderGPUCache::GetShapes(std::vector<frw::Shape>& outShapes, std::vector<std::array<float, 16>>& tmMatrs)
{
	assert(m_file != abcCache.end());

	outShapes.clear();
	frw::Context ctx = context()->GetContext();
	assert(ctx.IsValid());

	const FireRenderMeshCommon* pMainMesh = this->context()->GetMainMesh(uuid() + std::to_string(m_curr_frameNumber));
	const FireRenderGPUCache* mainMesh = dynamic_cast<const FireRenderGPUCache*>(pMainMesh);

	if (mainMesh != nullptr)
	{
		const std::vector<FrElement>& elements = mainMesh->Elements();

		outShapes.reserve(elements.size());

		for (const FrElement& element : elements)
		{
			outShapes.push_back(element.shape.CreateInstance(Context()));
			tmMatrs.push_back(element.TM);
		}

		m.isMainInstance = false;
	}

	if (mainMesh == nullptr)
	{
		// ensure correct input
		if (!m_file->second.m_scene)
			return;

		// translate alembic data into RPR shapes (to be decomposed...)
		for (auto alembicObj : m_file->second.m_scene->objects)
		{
			if (alembicObj->visible == false)
				continue;

			if (RPRAlembicWrapper::PolygonMeshObject* mesh = alembicObj.as_polygonMesh())
			{
				outShapes.emplace_back();
				outShapes.back() = TranslateAlembicMesh(mesh, ctx);

				// - transformation matrix
				tmMatrs.emplace_back(mesh->combinedXforms.m_value);
			}
		}

		m.isMainInstance = true;
		context()->AddMainMesh(this, std::to_string(m_curr_frameNumber));
	}

	MDagPath dagPath = DagPath();
	for (int i = 0; i < outShapes.size(); i++)
	{
		MString fullPathName = dagPath.fullPathName();
		std::string shapeName = std::string(fullPathName.asChar()) + "_" + std::to_string(i);
		outShapes[i].SetName(shapeName.c_str());
	}
}

void FireRenderGPUCache::OnNodeDirty()
{
	m.changed.mesh = true;
	setDirty();
}

void FireRenderGPUCache::OnShaderDirty()
{
	m.changed.shader = true;
	setDirty();
}

void FireRenderGPUCache::attributeChanged(MNodeMessage::AttributeMessage msg, MPlug &plug, MPlug &otherPlug)
{
	std::string name = plug.name().asChar();

	// check if file name was changed
	if (name.find("cacheFileName") != std::string::npos &&
		((msg | MNodeMessage::AttributeMessage::kConnectionMade) ||
		(msg | MNodeMessage::AttributeMessage::kConnectionBroken)))
	{
		m_changedFile = true;
		OnNodeDirty();
	}
}

void FireRenderGPUCache::RegisterCallbacks()
{
	FireRenderNode::RegisterCallbacks();
	if (context()->getCallbackCreationDisabled())
		return;
	for (auto& element : m.elements)
	{
		for (auto& shadingEngine : element.shadingEngines)
		{
			if (shadingEngine.isNull())
				continue;

			MObject shaderOb = getSurfaceShader(shadingEngine);

			if (shaderOb.isNull())
				continue;

			AddCallback(MNodeMessage::addNodeDirtyCallback(shaderOb, ShaderDirtyCallback, this));
		}
	}

	AddCallback(MEventMessage::addEventCallback("timeChanged", TimeChangedCallback, this));
}

void FireRenderGPUCache::ShaderDirtyCallback(MObject& node, void* clientData)
{
	DebugPrint("CALLBACK > ShaderDirtyCallback(%s)", node.apiTypeStr());
	if (auto self = static_cast<FireRenderGPUCache*>(clientData))
	{
		assert(node != self->Object());
		self->OnShaderDirty();
	}
}

void FireRenderGPUCache::TimeChangedCallback(void* clientData)
{
	MGlobal::displayInfo("FireRenderGPUCache::TimeChangedCallback FireRenderGPUCache::TimeChangedCallback FireRenderGPUCache::TimeChangedCallback");

	auto self = static_cast<FireRenderGPUCache*>(clientData);
	if (!self)
		return;

	self->m_changedFile = true;

	self->OnNodeDirty();
}



