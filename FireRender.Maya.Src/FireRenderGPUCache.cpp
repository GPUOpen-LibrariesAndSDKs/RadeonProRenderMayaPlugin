
#include "FireRenderGPUCache.h"
#include "Context/FireRenderContext.h"
#include "FireRenderUtils.h"

#include <array>
#include <algorithm>
#include <vector>
#include <iterator>
#include <istream>
#include <ostream>
#include <sstream>

#include <maya/MFnDependencyNode.h>
#include <maya/MPlug.h>
#include <maya/MMatrix.h>
#include <maya/MDagPath.h>
#include <maya/MSelectionList.h>

#include <Alembic/Abc/All.h>
#include <Alembic/AbcCoreOgawa/All.h>
#include <Alembic/AbcGeom/All.h>

using namespace Alembic::Abc;
using namespace Alembic::AbcGeom;

FireRenderGPUCache::FireRenderGPUCache(FireRenderContext* context, const MDagPath& dagPath) 
	: 	m_changedFile(true)
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
void FireRenderGPUCache::Freshen()
{
	Rebuild();
	FireRenderNode::Freshen();
}

void FireRenderGPUCache::ReadAlembicFile()
{
	MStatus res;

	// get name of alembic file from Maya node
	const MObject& node = Object();
	MFnDependencyNode nodeFn(node);
	MPlug plug = nodeFn.findPlug("cacheFileName", &res);
	CHECK_MSTATUS(res);

	std::string cacheFilePath = ProcessEnvVarsInFilePath(plug.asString(&res));
	CHECK_MSTATUS(res);

	try
	{
		m_archive = IArchive(Alembic::AbcCoreOgawa::ReadArchive(), cacheFilePath);
	}
	catch (std::exception &e)
	{
		char error[100];
		sprintf(error, "open alembic error: %s\n", e.what());
		MGlobal::displayError(error);
		return;
	}

	if (!m_archive.valid())
		return;

	uint32_t getNumTimeSamplings = m_archive.getNumTimeSamplings();

	std::string errorMessage;
	if (m_storage.open(cacheFilePath, errorMessage) == false)
	{
		errorMessage = "AlembicStorage::open error: " + errorMessage;
		MGlobal::displayError(errorMessage.c_str());
		return;
	}

	static int sampleIdx = 0;
	m_scene = m_storage.read(sampleIdx, errorMessage);
	if (!m_scene)
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

	MMatrix scaleM;
	scaleM.setToIdentity();
	scaleM[0][0] = scaleM[1][1] = scaleM[2][2] = 0.01;
	matrix *= scaleM;

	for (auto& element : m.elements)
	{
		if (element.shape)
		{
			float(*f)[4][4] = reinterpret_cast<float(*)[4][4]>(element.TM.data());
			MMatrix elementTransform(*f);

			MMatrix mayaObjMatr = GetSelfTransform();

			elementTransform *= mayaObjMatr;
			elementTransform *= scaleM;

			float mfloats[4][4];
			elementTransform.get(mfloats);

			element.shape.SetTransform(&mfloats[0][0]);
		}
	}
}

void FireRenderGPUCache::ProcessShaders()
{
	FireRenderContext* context = this->context();

	for (int i = 0; i < m.elements.size(); i++)
	{
		auto& element = m.elements[i];
		element.shader = context->GetShader(getSurfaceShader(element.shadingEngine), this);

		if (element.shape)
		{
			element.shape.SetShader(element.shader);

			frw::ShaderType shType = element.shader.GetShaderType();
			if (shType == frw::ShaderTypeEmissive)
				m.isEmissive = true;

			if ((shType == frw::ShaderTypeRprx) && (IsUberEmissive(element.shader)))
				m.isEmissive = true;
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
		ReadAlembicFile();
		ReloadMesh(meshPath);
	}

	RebuildTransforms();
	setRenderStats(meshPath);

	MFnDagNode meshFn(Object());
	MObjectArray shadingEngines = GetShadingEngines(meshFn, Instance());
	AssignShadingEngines(shadingEngines);

	RegisterCallbacks();

	ProcessShaders();

	// if (IsMeshVisible())
	attachToScene();

	m.changed.mesh = false;
	m.changed.transform = false;
	m.changed.shader = false;
	m_changedFile = false;
}

void FireRenderGPUCache::ReloadMesh(const MDagPath& meshPath)
{
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
	outShapes.clear();
	frw::Context ctx = context()->GetContext();
	assert(ctx.IsValid());

	// ensure correct input
	if (!m_scene)
		return;

	// translate alembic data into RPR shapes (to be decomposed...)
	for (auto alembicObj : m_scene->objects)
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

	for (auto& it : m.elements)
	{
		if (!it.shadingEngine.isNull())
		{
			MObject shaderOb = getSurfaceShader(it.shadingEngine);
			if (!shaderOb.isNull())
			{
				AddCallback(MNodeMessage::addNodeDirtyCallback(shaderOb, ShaderDirtyCallback, this));
			}
		}
	}
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

