#include "FireRenderObjects.h"
#include "FireRenderContext.h"
#include "FireRenderUtils.h"
#include "base_mesh.h"
#include "FireRenderDisplacement.h"
#include "SkyBuilder.h"

#include <float.h>

#include <maya/MFnLight.h>
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

#ifndef MAYA2015
#include <maya/MUuid.h>
#endif

#ifndef PI
#define PI 3.14159265358979323846
#endif

FireRenderObject::FireRenderObject(FireRenderContext* context, const MObject& ob)
{
	m.context = context;
	// SetObject(object); <- don't call this from base constructor!!!!
	m.object = ob;
	m.uuid = ob.isNull() ? "" : getNodeUUid(ob);
	if (!ob.isNull())
		context->setDirtyObject(this);
	m_isPortal_IBL = false;
	m_isPortal_SKY = false;
}

FireRenderObject::~FireRenderObject()
{
	FireRenderObject::clear();
}

std::string FireRenderObject::uuid()
{
	return m.uuid;
}

void FireRenderObject::setDirty()
{
	context()->setDirtyObject(this);
}

void FireRenderObject::Dump(const MObject& ob, int depth, int maxDepth)
{
	if (depth > maxDepth)
		return;

	std::string prefix(depth + 1, '\t');
	if (!ob.isNull() && ob.hasFn(MFn::kDependencyNode))
	{
		MFnDependencyNode depNode(ob);
		DebugPrint("%s%s %s", prefix.c_str(), depNode.typeName().asUTF8(), depNode.name().asUTF8());

		for (unsigned int i = 0; i < depNode.attributeCount(); i++)
		{
			MFnAttribute attr(depNode.attribute(i));
			auto plug = depNode.findPlug(attr.object());

			DebugPrint("%s-\t%s / %s", prefix.c_str(), attr.shortName().asUTF8(), attr.name().asUTF8());

			auto node = FireMaya::GetConnectedNode(plug);
			if (!node.isNull())
				Dump(node, depth + 1, maxDepth);
		}
	}
	else
	{
		DebugPrint("%s?", prefix.c_str());
	}
}

HashValue FireRenderObject::GetHash(const MObject& ob)
{
	HashValue hash;
	hash << ob;
	if (!ob.isNull() && ob.hasFn(MFn::kDependencyNode))
	{
		MFnDependencyNode depNode(ob);
		for (unsigned int i = 0; i < depNode.attributeCount(); i++)
		{
			auto oAttr = depNode.attribute(i);
			MFnAttribute attr(oAttr);
			auto plug = depNode.findPlug(attr.object());
			if (!plug.isNull())
			{
				hash << plug.node();
			}
		}
	}
	return hash;
}

HashValue FireRenderObject::CalculateHash()
{
	return GetHash(Object());
}

HashValue FireRenderNode::CalculateHash()
{
	auto hash = FireRenderObject::CalculateHash();
	auto dagPath = DagPath();
	if (dagPath.isValid())
	{
		hash << dagPath.isVisible();
		hash << dagPath.inclusiveMatrix();
	}
	return hash;
}

HashValue GetHashValue(const MPlug& plug)
{
	HashValue hash;
	MFnAttribute attr(plug.attribute());

	if (!attr.isWritable() || attr.isDynamic())
		return hash;

	if (plug.isArray())
		return hash;

	if (plug.isIgnoredWhenRendering())
		return hash;

	if (!plug.isKeyable())
		return hash;

	auto data = plug.asMDataHandle();

	auto type = data.type();
	hash << type;

	switch (type)
	{
	case MFnData::kNumeric:
	{
		switch (data.numericType())
		{
		case MFnNumericData::kBoolean:
			hash << data.asBool();
			break;
		case MFnNumericData::kInt:
			hash << data.asInt();
			break;
		case MFnNumericData::kChar:
			hash << data.asChar();
			break;
		case MFnNumericData::kShort:
			hash << data.asShort();
			break;
		case MFnNumericData::kByte:
			hash << data.asInt();
			break;
		case MFnNumericData::k2Short:
			hash << data.asShort2();
			break;
		case MFnNumericData::k3Short:
			hash << data.asShort3();
			break;
		case MFnNumericData::k2Int:
			hash << data.asInt2();
			break;
		case MFnNumericData::k3Int:
			hash << data.asInt3();
			break;
		case MFnNumericData::kFloat:
			hash << data.asFloat();
			break;
		case MFnNumericData::k2Float:
			hash << data.asFloat2();
			break;
		case MFnNumericData::k3Float:
			hash << data.asFloat3();
			break;
		case MFnNumericData::kDouble:
			hash << data.asDouble();
			break;
		case MFnNumericData::k2Double:
			hash << data.asDouble2();
			break;
		case MFnNumericData::k3Double:
			hash << data.asDouble3();
			break;
#ifndef MAYA2015
		case MFnNumericData::kInt64:
			hash << data.asInt64();
			break;
#endif
		default: break;
		}
	} break;

	case MFnData::kMatrix:
	{
		hash << data.asMatrix();
	}	break;
	case MFnData::kDoubleArray:
	case MFnData::kFloatArray:
	case MFnData::kIntArray:
	case MFnData::kPointArray:
	case MFnData::kVectorArray:

	default:
		break;
	}


	return hash;
}

void FireRenderObject::Freshen()
{
	if (m.callbackId.empty())
		RegisterCallbacks();
	m.hash = CalculateHash();
}

void FireRenderObject::clear()
{
	ClearCallbacks();
}

frw::Scene FireRenderObject::Scene()
{
	return context()->GetScene();
}

frw::Context FireRenderObject::Context()
{
	return context()->GetContext();
}

FireMaya::Scope FireRenderObject::Scope()
{
	return context()->GetScope();
}

void FireRenderObject::SetObject(const MObject& ob)
{
	if (m.object != ob)
	{
		ClearCallbacks();
		m.object = ob;
		m.uuid = ob.isNull() ? "" : getNodeUUid(ob);
	}
	setDirty();
}

void FireRenderObject::AddCallback(MCallbackId id)
{
	m.callbackId.push_back(id);
}

void FireRenderObject::RegisterCallbacks()
{
	ClearCallbacks();
	if (context()->getCallbackCreationDisabled())
		return;

	if (!m.object.isNull())
	{
		AddCallback(MNodeMessage::addNodeDirtyCallback(m.object, NodeDirtyCallback, this));
		AddCallback(MNodeMessage::addNodeDirtyPlugCallback(m.object, plugDirty_callback, this));
		AddCallback(MNodeMessage::addAttributeChangedCallback(m.object, attributeChanged_callback, this));
		AddCallback(MNodeMessage::addAttributeAddedOrRemovedCallback(m.object, attributeAddedOrRemoved_callback, this));
	}
}

void FireRenderObject::ClearCallbacks()
{
	if (m.callbackId.empty())
		return;

	for (auto it : m.callbackId)
		MNodeMessage::removeCallback(it);

	m.callbackId.clear();
}

void FireRenderObject::OnNodeDirty()
{
	setDirty();
}

void FireRenderObject::NodeDirtyCallback(MObject& node, void* clientData)
{
	DebugPrint("CALLBACK > NodeDirtyCallback(%s)", node.apiTypeStr());

	if (auto self = static_cast<FireRenderObject*>(clientData))
		self->OnNodeDirty();
}

void FireRenderObject::attributeChanged_callback(MNodeMessage::AttributeMessage msg, MPlug& plug, MPlug& otherPlug, void* clientData)
{
	if (auto self = static_cast<FireRenderObject*>(clientData))
		self->attributeChanged(msg, plug, otherPlug);
}

void FireRenderObject::attributeAddedOrRemoved_callback(MNodeMessage::AttributeMessage msg, MPlug& plug, void* clientData)
{
	if (auto self = static_cast<FireRenderObject*>(clientData))
		self->setDirty();
}

void FireRenderObject::plugDirty_callback(MObject& node, MPlug& plug, void* clientData)
{
	DebugPrint("CALLBACK > OnPlugDirty(%s, %s)", node.apiTypeStr(), plug.name().asUTF8());

	if (auto self = static_cast<FireRenderObject*>(clientData))
	{
		self->OnPlugDirty(node, plug);
	}

}

void FireRenderNode::WorldMatrixChangedCallback(MObject& transformNode, MDagMessage::MatrixModifiedFlags& modified, void* clientData)
{
	DebugPrint("CALLBACK > WorldMatrixChanged(%s, 0x%x)", transformNode.apiTypeStr(), modified);

	if (auto self = static_cast<FireRenderNode*>(clientData))
		self->OnWorldMatrixChanged();
}

FireRenderNode::FireRenderNode(FireRenderContext* context, const MDagPath& dagPath)
	: FireRenderObject(context, dagPath.node())
{
	m.instance = dagPath.instanceNumber();
	m.uuid = getNodeUUid(dagPath);
}

FireRenderNode::~FireRenderNode()
{
	RestorePortalStates();
}

std::vector<frw::Shape> FireRenderNode::GetVisiblePortals()
{
	std::vector<frw::Shape> result;
	MStatus mstatus;
	MFnDagNode dagNode(Object(), &mstatus);
	if (mstatus == MStatus::kSuccess)
	{
		RestorePortalStates();

		MPlug portalInPlug = dagNode.findPlug("portal");
		if (!portalInPlug.isNull())
		{
			MPlugArray portalConnections;
			portalInPlug.connectedTo(portalConnections, true, false);

			for (auto connection : portalConnections)
			{
				RecordPortalState(connection.node());
				if (auto ob = context()->getRenderObject<FireRenderMesh>(connection.node()))
				{
					ob->Freshen();	// make sure we have shapes to attach
					if (ob->IsVisible())
					{
						for (auto& element : ob->Elements())
						{
							if (element.shape)
								result.push_back(element.shape);
						}
					}
				}
			}
		}
	}
	return result;
}

void FireRenderNode::RecordPortalState(MObject node)
{
	MFnDependencyNode dn(node);
	MPlug plug = dn.findPlug("castsShadows");

	bool castsShadows;
	plug.getValue(castsShadows);
	plug.setValue(false);

	m_visiblePortals.push_back(std::make_pair(node, castsShadows));
}

void FireRenderNode::RestorePortalStates()
{
	for (auto portal : m_visiblePortals)
	{
		MObject& node = portal.first;
		bool castsShadows = portal.second;

		MFnDependencyNode dn(node);
		MPlug plug = dn.findPlug("castsShadows");
		plug.setValue(castsShadows);
	}

	m_visiblePortals.clear();
}

void FireRenderNode::RecordPortalState(MObject node, bool iblPortals)
{
	MFnDependencyNode dn(node);
	MPlug plug = dn.findPlug("castsShadows");

	bool castsShadows;
	plug.getValue(castsShadows);
	plug.setValue(false);

	if (iblPortals)
	{
		m_visiblePortals_IBL.push_back(std::make_pair(node, castsShadows));
	}
	else
	{
		m_visiblePortals_SKY.push_back(std::make_pair(node, castsShadows));
	}
}

void FireRenderNode::RestorePortalStates(bool iblPortals)
{

	for (auto portal : (iblPortals) ? m_visiblePortals_IBL : m_visiblePortals_SKY)
	{
		MObject& node = portal.first;
		bool castsShadows = portal.second;

		MFnDependencyNode dn(node);
		MPlug plug = dn.findPlug("castsShadows");
		plug.setValue(castsShadows);
	}

	if (iblPortals)
		m_visiblePortals_IBL.clear();
	else
		m_visiblePortals_SKY.clear();
}

MDagPath FireRenderNode::DagPath()
{
	MFnDagNode dagNode(Object());
	MDagPathArray pathArray;
	dagNode.getAllPaths(pathArray);
	if (m.instance < pathArray.length())
		return pathArray[m.instance];

	return MDagPath();
}

//===================
// Mesh
//===================
FireRenderMesh::FireRenderMesh(FireRenderContext* context, const MDagPath& dagPath) :
	FireRenderNode(context, dagPath)
{
}

FireRenderMesh::~FireRenderMesh()
{
	FireRenderMesh::clear();
}


void FireRenderMesh::clear()
{
	m.elements.clear();
	FireRenderObject::clear();
}

void FireRenderMesh::detachFromScene()
{
	if (!m_isVisible)
		return;

	if (auto scene = context()->GetScene())
	{
		for (auto element : m.elements)
		{
			if (auto shape = element.shape)
				scene.Detach(shape);
		}
	}
	m_isVisible = false;
}

void FireRenderMesh::attachToScene()
{
	if (m_isVisible)
		return;

	if (auto scene = context()->GetScene())
	{
		for (auto element : m.elements)
		{
			if (auto shape = element.shape)
				scene.Attach(shape);
		}
		m_isVisible = true;
	}
}

void FireRenderMesh::RegisterCallbacks()
{
	FireRenderNode::RegisterCallbacks();
	if (context()->getCallbackCreationDisabled())
		return;

	for (auto& it : m.elements)
	{
		if (!it.shadingEngine.isNull())
		{
			MObject shaderOb = getSurfaceShader(it.shadingEngine);
			if (!shaderOb.isNull())
			{
				AddCallback(MNodeMessage::addNodeDirtyCallback(shaderOb, ShaderDirtyCallback, this));
			}

			MObject shaderDi = getDisplacementShader(it.shadingEngine);
			if (!shaderDi.isNull())
			{
				AddCallback(MNodeMessage::addNodeDirtyCallback(shaderDi, ShaderDirtyCallback, this));
			}

			MObject shaderVolume = getVolumeShader(it.shadingEngine);
			if (!shaderVolume.isNull())
			{
				AddCallback(MNodeMessage::addNodeDirtyCallback(shaderVolume, ShaderDirtyCallback, this));
			}
		}
	}
}

void FireRenderMesh::attributeChanged(MNodeMessage::AttributeMessage msg, MPlug &plug, MPlug &otherPlug)
{
	std::string name = plug.name().asChar();

	// check if change is connected with shaderEngines
	if (name.find("instObjGroups") != std::string::npos &&
		((msg | MNodeMessage::AttributeMessage::kConnectionMade) ||
			(msg | MNodeMessage::AttributeMessage::kConnectionBroken)))
	{
		OnShaderDirty();
	}
}

void FireRenderMesh::buildSphere()
{
	if (auto sphere = context()->GetScope().Context().CreateMesh(
		vertices, 382, sizeof(float) * 3,
		normals, 382, sizeof(float) * 3,
		texcoords, 439, sizeof(float) * 2,
		vertexIndices, sizeof(int),
		normalIndices, sizeof(int),
		texcoord_indices, sizeof(int),
		polyCountArray, 400))
	{
		m.elements.push_back(FrElement{ sphere });
	}
}

void FireRenderMesh::setRenderStats(MDagPath dagPath)
{
	MFnDependencyNode depNode(dagPath.node());

	MPlug visibleInReflectionsPlug = depNode.findPlug("visibleInReflections");
	bool visibleInReflections;
	visibleInReflectionsPlug.getValue(visibleInReflections);

	MPlug castsShadowsPlug = depNode.findPlug("castsShadows");
	bool castsShadows;
	castsShadowsPlug.getValue(castsShadows);

	MPlug primaryVisibilityPlug = depNode.findPlug("primaryVisibility");
	bool primaryVisibility;
	primaryVisibilityPlug.getValue(primaryVisibility);

	bool selectionCheck = !context()->renderSelectedObjectsOnly() || IsSelected(dagPath);

	setVisibility(dagPath.isVisible() && selectionCheck);
	setPrimaryVisibility(primaryVisibility);

	//TODO: doesn't seem to be working just right
	//see also AMDMAX-617 and AMDMAX-588
	//setReflectionVisibility(visibleInReflections);

	setCastShadows(castsShadows);
}

bool FireRenderMesh::IsSelected(const MDagPath& dagPath) const
{
	MObject transformObject = dagPath.transform();
	bool isSelected = false;

	// get a list of the currently selected items 
	MSelectionList selected;
	MGlobal::getActiveSelectionList(selected);

	// iterate through the list of items returned
	for (unsigned int i = 0; i<selected.length(); ++i)
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

void FireRenderMesh::setVisibility(bool visibility)
{
	if (visibility)
		attachToScene();
	else
		detachFromScene();
}

void FireRenderMesh::setReflectionVisibility(bool reflectionVisibility)
{
	for (auto element : m.elements)
	{
		if (auto shape = element.shape)
			shape.SetReflectionVisibility(reflectionVisibility);
	}
}
void FireRenderMesh::setCastShadows(bool castShadow)
{
	for (auto element : m.elements)
	{
		if (auto shape = element.shape)
			shape.SetShadowFlag(castShadow);
	}
}

void FireRenderMesh::setPrimaryVisibility(bool primaryVisibility)
{
	for (auto element : m.elements)
	{
		if (auto shape = element.shape)
			shape.SetPrimaryVisibility(primaryVisibility);
	}
}

void FireRenderNode::RegisterCallbacks()
{
	FireRenderObject::RegisterCallbacks();
	if (context()->getCallbackCreationDisabled())
		return;

	if (Object().isNull())
		return;

	auto dagPath = DagPath();

	auto transform = dagPath.transform();
	if (transform.isNull())
		return;

	AddCallback(MNodeMessage::addNodeDirtyPlugCallback(transform, plugDirty_callback, this));
	AddCallback(MDagMessage::addWorldMatrixModifiedCallback(dagPath, WorldMatrixChangedCallback, this));
}

void FireRenderMesh::setupDisplacement(MObject shadingEngine, frw::Shape shape)
{
	if (!shape)
		return;

	bool haveDisplacement = false;

	if (shape.IsUVCoordinatesSet())
	{
		FireMaya::Displacement *displacement = nullptr;

		// Check displacement shader connection
		MObject displacementShader = getDisplacementShader(shadingEngine);
		if (!displacementShader.isNull())
		{
			MFnDependencyNode shaderNode(displacementShader);
			displacement = dynamic_cast<FireMaya::Displacement*>(shaderNode.userNode());
		}

		if (!displacement)
		{
			// Check surface shader connection, look for shader with displacement map input
			MObject surfaceShader = getSurfaceShader(shadingEngine);
			if (!surfaceShader.isNull())
			{
				MFnDependencyNode shaderNode(surfaceShader);
				FireMaya::ShaderNode* shader = dynamic_cast<FireMaya::ShaderNode*>(shaderNode.userNode());
				if (shader)
				{
					displacementShader = shader->GetDisplacementNode();
					if (!displacementShader.isNull())
					{
						MFnDependencyNode shaderNodeDS(displacementShader);
						displacement = dynamic_cast<FireMaya::Displacement*>(shaderNodeDS.userNode());
					}
				}
			}
		}

		if (displacement)
		{
			float minHeight = 0;
			float maxHeight = 0;
			int subdivision = 0;
			float creaseWeight = 0;
			int boundary = 0;
			frw::Value mapValue;

			auto scope = Scope();
			haveDisplacement = displacement->getValues(mapValue, scope, minHeight, maxHeight, subdivision, creaseWeight, boundary);

			if (haveDisplacement)
			{
				shape.SetDisplacement(mapValue, minHeight, maxHeight);
				shape.SetSubdivisionFactor(subdivision);
				shape.SetSubdivisionCreaseWeight(creaseWeight);
				shape.SetSubdivisionBoundaryInterop(boundary);
			}
		}
	}

	if (!haveDisplacement)
	{
		shape.RemoveDisplacement();
	}
}

void FireRenderMesh::Rebuild()
{
	auto node = Object();
	MFnDagNode meshFn(node);
	MString name = meshFn.name();
	auto meshPath = DagPath();

	FireRenderContext *context = this->context();

	auto shadingEngines = GetShadingEngines(meshFn, Instance());

	m.isEmissive = false;

	// If there is just one shader and the number of shader is not changed then just update the shader
	if (m.changed.mesh || (shadingEngines.length() != 1 || m.elements.size() != 1))
	{
		// the number of shader is changed so reload the mesh

		MMatrix mMtx = meshPath.inclusiveMatrix();
		setVisibility(false);
		m.elements.clear();

		MObjectArray shaderObjs;
		std::vector<frw::Shape> shapes;

		GetShapes(node, shapes);
				
		m.elements.resize(shapes.size());
		for (unsigned int i = 0; i < shapes.size(); i++)
		{
			m.elements[i].shape = shapes[i];
			m.elements[i].shadingEngine = shadingEngines[i < shadingEngines.length() ? i : 0];
		}
	}

	if (meshPath.isValid())
	{
		MFnDependencyNode nodeFn(node);

		for (int i = 0; i < m.elements.size(); i++)
		{
			auto& element = m.elements[i];
			element.shadingEngine = shadingEngines[i];
			element.shader = context->GetShader(getSurfaceShader(element.shadingEngine));
			element.volumeShader = context->GetVolumeShader(getVolumeShader(element.shadingEngine));

			setupDisplacement(element.shadingEngine, element.shape);

			if (!element.volumeShader)
				element.volumeShader = context->GetVolumeShader(getSurfaceShader(element.shadingEngine));

			// if no valid surface shader, we should set to transparent in case of volumes present
			if (element.volumeShader)
			{
				if (!element.shader || element.shader == element.volumeShader)	// also catch case where volume assigned to surface
					element.shader = frw::TransparentShader(context->GetMaterialSystem());
			}

			if (element.shape)
			{
				element.shape.SetShader(element.shader);
				element.shape.SetVolumeShader(element.volumeShader);
				if (element.shader.GetShaderType() == frw::ShaderTypeEmissive)
					m.isEmissive = true;
			}
		}
		RebuildTransforms();
		setRenderStats(meshPath);
	}

	if (context->iblLight) {
		MObject temp = context->iblTransformObject;
		MFnTransform iblLightTransform(temp);

		if (iblLightTransform.isParentOf(node))
		{
			if (!m_isPortal_IBL)
			{
				m_isPortal_IBL = false;
				context->iblLight->setDirty();
			}
		}
		else {
			if (m_isPortal_IBL)
			{
				if (context->skyLight)
				{
					// in case we move object from IBL to skylight
					MObject temp2 = context->skyTransformObject;
					MFnTransform skyLightTransform(temp2);

					if (skyLightTransform.isParentOf(node))
					{
						if (!m_isPortal_SKY)
						{
							m_isPortal_SKY = false;
							context->skyLight->setDirty();
						}
					}
				}


				m_isPortal_IBL = false;
				context->iblLight->setDirty();
			}
		}
	}

	if (context->skyLight)
	{
		MObject temp = context->skyTransformObject;
		MFnTransform skyLightTransform(temp);

		if (skyLightTransform.isParentOf(node))
		{
			if (!m_isPortal_SKY)
			{
				m_isPortal_SKY = false;
				context->skyLight->setDirty();
			}
		}
		else {
			if (m_isPortal_SKY)
			{
				m_isPortal_SKY = false;
				context->skyLight->setDirty();
			}
		}
	}

	m.changed.mesh = false;
	m.changed.transform = false;
	m.changed.shader = false;

	RegisterCallbacks();	// we need to do this in case the shaders change (ie we will need to attach new callbacks)
}

void FireRenderMesh::GetShapes(const MFnDagNode& meshNode, std::vector<frw::Shape>& outShapes)
{
	FireRenderContext* context = this->context();

	FireRenderMesh* mainMesh = nullptr;
	if (meshNode.isInstanced())
	{
		MDagPathArray pathArray;
		meshNode.getAllPaths(pathArray);

		for (const MDagPath& path : pathArray)
		{
			FireRenderMesh* mesh = context->getRenderObject<FireRenderMesh>(path);
			if (mesh->IsMainInstance())
			{
				mainMesh = mesh;
				break;
			}
		}

		if (mainMesh != nullptr)
		{
			const std::vector<FrElement>& elements = mainMesh->Elements();

			for (const FrElement& element : elements)
			{
				outShapes.push_back(element.shape.CreateInstance(Context()));
			}

			m.isMainInstance = false;
		}
	}

	if (mainMesh == nullptr)
	{
		outShapes = FireMaya::MeshTranslator::TranslateMesh(Context(), meshNode.object());
		m.isMainInstance = true;
	}
}

void FireRenderMesh::RebuildTransforms()
{
	auto node = Object();
	MFnDagNode meshFn(node);
	auto meshPath = DagPath();

	MMatrix matrix = meshPath.inclusiveMatrix();
	MMatrix nextFrameMatrix = matrix;

	// convert Maya mesh in cm to m
	MMatrix scaleM;
	scaleM.setToIdentity();
	scaleM[0][0] = scaleM[1][1] = scaleM[2][2] = 0.01;
	matrix *= scaleM;
	float mfloats[4][4];
	matrix.get(mfloats);

	MVector linearMotion(0, 0, 0);
	MVector rotationAxis(1, 0, 0);
	double rotationAngle = 0.0;

	MTime nextTime = MAnimControl::currentTime();
	MTime minTime = MAnimControl::minTime();

	if (context()->motionBlur() && (nextTime != minTime))
	{
		nextTime--;
		MDGContext dgcontext(nextTime);
		MObject val;
		MFnDependencyNode nodeFn(meshPath.node());
		MPlug matrixPlug = nodeFn.findPlug("worldMatrix");
		matrixPlug = matrixPlug.elementByLogicalIndex(0);
		matrixPlug.getValue(val, dgcontext);
		nextFrameMatrix = MFnMatrixData(val).matrix();
		if (nextFrameMatrix != matrix)
		{
			float timeMultiplier = context()->motionBlurScale();
			MTime time = MAnimControl::currentTime();
			MTime t2 = MTime(1.0, time.unit());
			timeMultiplier = (float) (timeMultiplier / t2.asUnits(MTime::kSeconds));

			MMatrix nextMatrix = nextFrameMatrix;
			nextMatrix *= scaleM;

			// get linear motion
			linearMotion = MVector(nextMatrix[3][0] - matrix[3][0], nextMatrix[3][1] - matrix[3][1], nextMatrix[3][2] - matrix[3][2]);
			linearMotion *= timeMultiplier;

			MTransformationMatrix transformationMatrix(matrix);
			MQuaternion currentRotation = transformationMatrix.rotation();

			MTransformationMatrix transformationMatrixNext(nextMatrix);
			MQuaternion nextRotation = transformationMatrixNext.rotation();

			MQuaternion dispRotation = nextRotation * currentRotation.inverse();

			dispRotation.getAxisAngle(rotationAxis, rotationAngle);
			if (rotationAngle > PI)
				rotationAngle -= 2 * PI;
			rotationAngle *= timeMultiplier;
		}
	}

	for (auto& element : m.elements)
	{
		if (element.shape)
		{
			element.shape.SetTransform(&mfloats[0][0]);
			element.shape.SetLinearMotion(float(linearMotion.x), float(linearMotion.y), float(linearMotion.z));
			element.shape.SetAngularMotion(float(rotationAxis.x), float(rotationAxis.y), float(rotationAxis.z), float(rotationAngle));
		}
	}
}

void FireRenderMesh::OnNodeDirty()
{
	m.changed.mesh = true;
	setDirty();
}

void FireRenderNode::OnWorldMatrixChanged()
{
	setDirty();
}

void FireRenderMesh::OnShaderDirty()
{
	m.changed.shader = true;
	setDirty();
}

void FireRenderMesh::ShaderDirtyCallback(MObject& node, void* clientData)
{
	DebugPrint("CALLBACK > ShaderDirtyCallback(%s)", node.apiTypeStr());
	if (auto self = static_cast<FireRenderMesh*>(clientData))
	{
		assert(node != self->Object());
		self->OnShaderDirty();
	}
}

void FireRenderNode::OnPlugDirty(MObject& node, MPlug &plug)
{
	MString partialShortName = plug.partialName();

	if (partialShortName == "fruuid")
		return;

	setDirty();
}

void FireRenderMesh::Freshen()
{
	Rebuild();

	FireRenderNode::Freshen();
}

HashValue FireRenderMesh::CalculateHash()
{
	auto hash = FireRenderNode::CalculateHash();
	for (auto& e : m.elements)
	{
		hash << e.shadingEngine;
	}
	return hash;
}

//===================
// Light
//===================
FireRenderLight::FireRenderLight(FireRenderContext* context, const MDagPath& dagPath) :
	FireRenderNode(context, dagPath),
	m_light(FrLight()),
	m_portal(false)
{}

FireRenderLight::~FireRenderLight()
{
	clear();
}

void FireRenderLight::clear()
{
	m_matrix = MMatrix();
	m_light = FrLight();

	FireRenderObject::clear();
}

void FireRenderLight::detachFromScene()
{
	if (!m_isVisible)
		return;

	if (auto scene = Scene())
	{
		if (m_light.isAreaLight && m_light.areaLight)
			scene.Detach(m_light.areaLight);
		if ((!m_light.isAreaLight) && m_light.light)
			scene.Detach(m_light.light);
	}
	m_isVisible = false;
}

void FireRenderLight::attachToScene()
{
	if (m_isVisible)
		return;
	if (auto scene = Scene())
	{
		if (m_light.isAreaLight && m_light.areaLight)
			scene.Attach(m_light.areaLight);
		if ((!m_light.isAreaLight) && m_light.light)
			scene.Attach(m_light.light);
		m_isVisible = true;
	}
}

void FireRenderLight::Freshen()
{
	detachFromScene();
	m_light = FrLight();
	auto node = Object();
	auto dagPath = DagPath();
	if (dagPath.isValid())
	{
		auto mMtx = dagPath.inclusiveMatrix();

		MFnDependencyNode depNode(node);
		if (depNode.typeId() == FireMaya::TypeId::FireRenderPhysicalLightLocator)
			FireMaya::translateLight(m_light, context()->GetScope(), Context(), node, mMtx);
		else if (node.hasFn(MFn::kPluginLocatorNode) || node.hasFn(MFn::kPluginTransformNode))
			FireMaya::translateVrayLight(m_light, context()->GetMaterialSystem(), Context(), node, mMtx);
		else
			FireMaya::translateLight(m_light, context()->GetScope(), Context(), node, mMtx);

		if (dagPath.isVisible())
			attachToScene();
	}

	FireRenderNode::Freshen();
}

void FireRenderLight::buildSwatchLight()
{
	m_light.light = Context().CreateDirectionalLight();
	auto frstatus = rprDirectionalLightSetRadiantPower3f(m_light.light.Handle(), LIGHT_SCALE, LIGHT_SCALE, LIGHT_SCALE);
	checkStatus(frstatus);

	rpr_float mfloats[4][4] = {
		0.9487038463825838f,
		-0.012954754457450961f,
		0.31590059543444676f,
		0.0f,
		0.13385030546109988f,
		0.9216602553518096f,
		-0.3641791721567935f,
		0.0f,
		-0.28643517170009186f,
		0.3877815725918193f,
		0.8761166271515195f,
		0.0f,
		0.0f,
		0.0f,
		0.0f,
		1.0f
	};
	m_light.light.SetTransform(mfloats[0]);
}

void FireRenderLight::setPortal(bool value)
{
	m_portal = value;
}

bool FireRenderLight::portal()
{
	return m_portal;
}

//===================
// Env Light
//===================
FireRenderEnvLight::FireRenderEnvLight(FireRenderContext* context, const MDagPath& dagPath) :
	FireRenderNode(context, dagPath)
{
}

FireRenderEnvLight::~FireRenderEnvLight()
{
	clear();
}

const frw::Light& FireRenderEnvLight::data()
{
	return m.light;
}


void FireRenderEnvLight::clear()
{
	if (m.light)
	{
		context()->iblLight = nullptr;
	}
	m_matrix = MMatrix();
	m.light.Reset();
	m.image.Reset();
	m.bgOverride.Reset();

	FireRenderObject::clear();
}

void FireRenderEnvLight::detachFromScene()
{
	if (!m_isVisible)
		return;

	if (auto scene = Scene())
	{
		scene.SetEnvironmentOverride(frw::EnvironmentOverrideBackground, frw::EnvironmentLight());
		scene.Detach(m.light);
	}

	m_isVisible = false;
}

void FireRenderEnvLight::attachToScene()
{
	if (m_isVisible)
		return;

	if (auto scene = Scene())
	{
		scene.Attach(m.light);
		Scene().SetEnvironmentOverride(frw::EnvironmentOverrideBackground, m.bgOverride);

		m_isVisible = true;
	}
}

void setPortal_IBL(MObject transformObject, FireRenderEnvLight *light) {
	MFnTransform transform(transformObject);

	auto childCount = transform.childCount();
	for (auto i = 0u; i < childCount; i++)
	{
		MObject child = transform.child(i);
		MFnTransform childTransform(child);
		auto childChildCount = childTransform.childCount();

		for (auto j = 0u; j < childChildCount; j++)
		{
			MObject portal = childTransform.child(j);

			if (auto ob = light->context()->getRenderObject<FireRenderMesh>(portal))
			{
				ob->m_isPortal_IBL = true;

				light->RecordPortalState(portal, true);

				ob->Freshen();	// make sure we have shapes to attach

				if (ob->IsVisible())
				{
					for (auto& element : ob->Elements())
					{
						if (element.shape)
						{
							light->m.light.AttachPortal(element.shape);
						}
					}
				}
			}
		}

		if (child.hasFn(MFn::kTransform))
		{
			setPortal_IBL(child, light);
		}
	}
}

void FireRenderEnvLight::Freshen()
{
	RestorePortalStates(true);

	detachFromScene();
	m.light.Reset();
	m.image.Reset();
	m.bgOverride.Reset();

	context()->iblLight = nullptr;
	context()->iblTransformObject = MObject();

	auto node = Object();
	auto dagPath = DagPath();
	MFnDagNode dagNode(node);

	// Check node visibility without checking current render layer.
	// We should render env light on all render layers.
	if (dagPath.isValid() && isVisible(dagNode, MFn::kInvalid))
	{
		MPlug filePathPlug = dagNode.findPlug("filePath");
		if (!filePathPlug.isNull())
		{
			MString filePath;
			filePathPlug.getValue(filePath);
			if (filePath != "")
			{
				MString colorSpace;
				auto colorSpacePlug = dagNode.findPlug("colorSpace");
				if (!colorSpacePlug.isNull())
					colorSpace = colorSpacePlug.asString();

				// Initially we have IBL flipped, due to fact that texture is being placed 
				// from external side of the sphere, but we look from inside sphere
				// That's why pass true if IBL flip parameter is false and vice versa
				m.image = context()->GetScope().GetImage(filePath, colorSpace, !IsFlipIBL());
			}
		}

		bool ambLightChecker = false;
		auto scope = this->Scope();
		FireMaya::translateEnvLight(m.light, m.image, Context(), scope, node, dagPath.inclusiveMatrix(), ambLightChecker);
		if (m.light)
		{
			setPortal_IBL(dagPath.transform(), this);

			//
			context()->iblLight = this;
			context()->iblTransformObject = dagPath.transform();
			//

			if (!GetPlugValue("display", true) || m.light.IsAmbientLight())
			{
				m.bgOverride = Context().CreateEnvironmentLight();
				m.bgOverride.SetImage(frw::Image(Context(), 0, 0, 0));
			}

			attachToScene();	// normal!
		}
	}

	FireRenderNode::Freshen();
}

//===================
// Camera
//===================
FireRenderCamera::FireRenderCamera(FireRenderContext* context, const MDagPath& dagPath) :
	FireRenderNode(context, dagPath)
{
	m_alphaMask = true;
}

FireRenderCamera::~FireRenderCamera()
{
	clear();
}

const frw::Camera& FireRenderCamera::data()
{
	return m_camera;
}

void FireRenderCamera::clear()
{
	m_matrix = MMatrix();
	m_camera.Reset();

	FireRenderObject::clear();
}

void FireRenderCamera::Freshen()
{
	auto node = Object();
	auto dagPath = DagPath();
	MFnDagNode dagNode(node);
	MFnCamera fnCamera(node);

	if (!m_camera)
	{
		m_camera = Context().CreateCamera();
		Scene().SetCamera(m_camera);
	}

	if (dagPath.isValid())
	{
		MMatrix camMtx = dagPath.inclusiveMatrix();
		int viewWidth = context()->width();
		int viewHeight = context()->height();

		FireMaya::translateCamera(m_camera, node, camMtx, context()->isRenderView(), float(viewWidth) / float(viewHeight), true, m_type);

		frw::Image image;

		auto imagePlanePlug = dagNode.findPlug("imagePlane");
		if (imagePlanePlug.isArray())
		{
			if (int n = imagePlanePlug.numElements())
				imagePlanePlug = imagePlanePlug.elementByPhysicalIndex(0);
		}

		auto maskPlug = dagNode.findPlug("mask");
		if (!maskPlug.isNull())
		{
			m_alphaMask = maskPlug.asBool();
		}

		m_imagePlane = FireMaya::GetConnectedNode(imagePlanePlug);
		if (!m_imagePlane.isNull())
		{
			MFnDagNode dagImage(m_imagePlane);
			MDagPath imagePath;
			dagImage.getPath(imagePath);

			if (imagePath.isValid() && imagePath.isVisible())
			{
				switch (GetPlugValue(m_imagePlane, "type", -1))
				{
				case 0:	// image
				{
					MFnDependencyNode imNode(m_imagePlane);
					auto name = GetPlugValue(m_imagePlane, "imageName", MString());
					auto contrast = Scope().GetValue(imNode.findPlug("colorGain"));
					auto brightness = Scope().GetValue(imNode.findPlug("colorOffset"));

					double overScan = 0, imgSizeX = 0, imgSizeY = 0, apertureSizeX = 0, apertureSizeY = 0, offsetX = 0, offsetY = 0;

					MPlug plug;
					if (context()->isRenderView())
					{
						overScan = 1;
					}
					else
					{
						plug = dagNode.findPlug("overscan");
						if (!plug.isNull())
							plug.getValue(overScan);
					}

					plug = imNode.findPlug("sizeX");
					if (!plug.isNull())
						plug.getValue(imgSizeX);
					plug = imNode.findPlug("sizeY");
					if (!plug.isNull())
						plug.getValue(imgSizeY);

					plug = dagNode.findPlug("horizontalFilmAperture");
					if (!plug.isNull())
						plug.getValue(apertureSizeX);
					plug = dagNode.findPlug("verticalFilmAperture");
					if (!plug.isNull())
						plug.getValue(apertureSizeY);

					plug = imNode.findPlug("offsetX");
					if (!plug.isNull())
						plug.getValue(offsetX);
					plug = imNode.findPlug("offsetY");
					if (!plug.isNull())
						plug.getValue(offsetY);

					int filmFitRaw = GetPlugValue(node, "filmFit", -1);
					FireMaya::FitType filmFit;
					switch (filmFitRaw)
					{
					case 0:
						filmFit = (viewWidth >= viewHeight) ? FireMaya::FitHorizontal : FireMaya::FitVertical;
						break;
					case 1:
						filmFit = FireMaya::FitHorizontal;
						break;
					case 2:
						filmFit = FireMaya::FitVertical;
						break;
					case 3:
						filmFit = (viewWidth < viewHeight) ? FireMaya::FitHorizontal : FireMaya::FitVertical;
						break;
					default:
						filmFit = FireMaya::FitHorizontal;
						break;
					}

					bool ignoreColorSpaceFileRules = false;
					MString colorSpace;
#ifdef MAYA2017
					plug = imNode.findPlug("ignoreColorSpaceFileRules");
					if (!plug.isNull())
						plug.getValue(ignoreColorSpaceFileRules);
					plug = imNode.findPlug("colorSpace");
					if (!plug.isNull())
						plug.getValue(colorSpace);
#endif
					image = Scope().GetAdjustedImage(name,
						viewWidth, viewHeight,
						FireMaya::FitType(GetPlugValue(m_imagePlane, "fit", -1)), contrast.GetX(1), brightness.GetX(0),
						filmFit, overScan,
						imgSizeX, imgSizeY,
						apertureSizeX, apertureSizeY,
						offsetX, offsetY,
						ignoreColorSpaceFileRules, colorSpace);
				} break;
				case 1: // texture
				{
					// TODO
				} break;
				}
			}
		}

		Scene().SetBackgroundImage(image);

		// Set exposure from motion blur block parameters if we didn't add fireRenderExposure attribute to the camera before
		if (context()->motionBlur() && fnCamera.findPlug("fireRenderExposure").isNull())
		{
			rprCameraSetExposure(m_camera.Handle(), context()->motionBlurCameraExposure());
		}
	}
	else
	{
		rprCameraSetExposure(m_camera.Handle(), 1.f);
	}

	RegisterCallbacks();
	FireRenderNode::Freshen();
}

void FireRenderCamera::RegisterCallbacks()
{
	FireRenderNode::RegisterCallbacks();
	if (context()->getCallbackCreationDisabled())
		return;

	if (!m_imagePlane.isNull())
	{
		AddCallback(MNodeMessage::addNodeDirtyCallback(m_imagePlane, NodeDirtyCallback, this));
		AddCallback(MNodeMessage::addNodeDirtyPlugCallback(m_imagePlane, plugDirty_callback, this));
	}
}

void FireRenderCamera::buildSwatchCamera()
{
	m_camera = Context().CreateCamera();
	Scene().SetCamera(m_camera);

	auto frcamera = m_camera.Handle();

	auto frstatus = rprCameraSetMode(frcamera, RPR_CAMERA_MODE_PERSPECTIVE);
	checkStatus(frstatus);

	frstatus = rprCameraSetFocalLength(frcamera, 100.0f);
	checkStatus(frstatus);

	frstatus = rprCameraSetFocusDistance(frcamera, 10.0f);
	checkStatus(frstatus);

	float fStop = FLT_MAX;
	frstatus = rprCameraSetFStop(frcamera, fStop);
	checkStatus(frstatus);

	float apertureWidth = 35.0;
	float apertureHeight = 35.0;
	frstatus = rprCameraSetSensorSize(frcamera, apertureWidth, apertureHeight);
	checkStatus(frstatus);

	frstatus = rprCameraLookAt(frcamera, 0.0, 0.0, 7.5, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0);
	checkStatus(frstatus);
}

void FireRenderCamera::setType(short type)
{
	m_type = type;
}

bool FireRenderCamera::GetAlphaMask() const
{
	return m_alphaMask;
}

//===================
// Display layer
//===================
FireRenderDisplayLayer::FireRenderDisplayLayer(FireRenderContext* context, const MObject& object) :
	FireRenderObject(context, object)
{
}

FireRenderDisplayLayer::~FireRenderDisplayLayer()
{
	clear();
}

void FireRenderDisplayLayer::clear()
{
	FireRenderObject::clear();
}

void FireRenderDisplayLayer::attributeChanged(MNodeMessage::AttributeMessage msg, MPlug & plug, MPlug & otherPlug)
{
	auto frLayer = this;
	FireRenderContext *context = frLayer->context();
	if (!context)
		return;

	if ((plug.partialName() == "v"))
	{
		MObject node = plug.node();
		MFnDependencyNode nodeFn(node);
		MPlug drawInfoPlug = nodeFn.findPlug("drawInfo");
		MPlug visPlug = drawInfoPlug.child(6);
		bool visibility = true;
		visibility = visPlug.asBool();
		MPlugArray connections;
		drawInfoPlug.connectedTo(connections, false, true);
		if (connections.length() == 0)
			return;

		LOCKFORUPDATE(context);
		for (auto connection : connections)
		{
			MObject transformNode = connection.node();
			MDagPath path = MDagPath::getAPathTo(transformNode);
			MItDag itDag;
			itDag.reset(path);
			for (; !itDag.isDone(); itDag.next())
			{
				MDagPath dagPath;
				MStatus status = itDag.getPath(dagPath);
				if (status != MS::kSuccess)
					continue;

				if (auto frNode = context->getRenderObject<FireRenderNode>(dagPath))
				{
					if (dagPath.isVisible() && visibility)
						frNode->attachToScene();
					else
						frNode->detachFromScene();
				}
			}
		}
		context->setDirty();
	}

	if (((plug.partialName() == "di") && (msg & MNodeMessage::kConnectionMade)) ||
		((plug.partialName() == "di") && (msg & MNodeMessage::kConnectionBroken)))
	{
		MObject node = plug.node();
		MFnDependencyNode nodeFn(node);
		MPlug drawInfoPlug = nodeFn.findPlug("drawInfo");
		MPlug visPlug = drawInfoPlug.child(6);
		bool visibility = true;
		visibility = visPlug.asBool();

		LOCKFORUPDATE(context);

		MDagPath path = MDagPath::getAPathTo(otherPlug.node());
		MItDag itDag;
		itDag.reset(path);
		for (; !itDag.isDone(); itDag.next())
		{
			MDagPath dagPath;
			MStatus status = itDag.getPath(dagPath);
			if (status != MS::kSuccess)
				continue;
			if (auto frNode = context->getRenderObject<FireRenderNode>(dagPath))
			{
				bool dagVisibility = dagPath.isVisible();
				if (msg & MNodeMessage::kConnectionMade)
					dagVisibility = dagVisibility && visibility;

				if (dagVisibility)
					frNode->attachToScene();
				else
					frNode->detachFromScene();
			}
		}
		context->setDirty();
	}
}


//===================
// Sky
//===================
FireRenderSky::FireRenderSky(FireRenderContext* context, const MDagPath& dagPath) :
	FireRenderNode(context, dagPath),
	m_skyBuilder(new SkyBuilder(dagPath.node()))
{}

FireRenderSky::~FireRenderSky()
{
	clear();
	delete m_skyBuilder;
}


void setPortal_Sky(MObject transformObject, FireRenderSky *light) {
	MFnTransform transform(transformObject);

	auto childCount = transform.childCount();
	for (auto i = 0u; i < childCount; i++)
	{
		MObject child = transform.child(i);
		MFnTransform childTransform(child);
		auto childChildCount = childTransform.childCount();

		for (auto j = 0u; j < childChildCount; j++)
		{
			MObject portal = childTransform.child(j);

			if (auto ob = light->context()->getRenderObject<FireRenderMesh>(portal))
			{
				ob->m_isPortal_SKY = true;

				light->RecordPortalState(portal, false);

				ob->Freshen();	// make sure we have shapes to attach

				if (ob->IsVisible())
				{
					for (auto& element : ob->Elements())
					{
						if (element.shape)
						{
							light->m_envLight.AttachPortal(element.shape);
						}
					}
				}
			}
		}

		if (child.hasFn(MFn::kTransform))
		{
			setPortal_Sky(child, light);
		}
	}
}

void FireRenderSky::Freshen()
{
	RestorePortalStates(false);

	auto node = Object();
	auto dagPath = DagPath();

	context()->skyLight = nullptr;
	context()->skyTransformObject = MObject();

	if (dagPath.isValid())
	{
		if (FireMaya::translateSky(m_envLight, m_sunLight, m_image, *m_skyBuilder, Context(), node, dagPath.inclusiveMatrix(), m_initialized))
		{
			setPortal_Sky(dagPath.transform(), this);

			//
			context()->skyLight = this;
			context()->skyTransformObject = dagPath.transform();
			//

			m_initialized = true;

			if (dagPath.isVisible())
				attachToScene();
			else
				detachFromScene();
		}
	}

	FireRenderNode::Freshen();
}

void FireRenderSky::attachPortals()
{
	detachPortals();

	auto portals = GetVisiblePortals();
	for (auto portal : portals)
	{
		m_envLight.AttachPortal(portal);
		m_portals.push_back(portal);
	}
}

void FireRenderSky::detachPortals()
{
	for (auto portal : m_portals)
		m_envLight.DetachPortal(portal);

	m_portals.clear();
}

void FireRenderSky::clear()
{
	m_matrix = MMatrix();
	detachPortals();
	m_envLight.Reset();
	m_sunLight.Reset();
	m_image.Reset();
	FireRenderObject::clear();
}

void FireRenderSky::detachFromScene()
{
	if (!m_isVisible)
		return;

	if (auto scene = Scene())
	{
		scene.Detach(m_envLight);
		scene.Detach(m_sunLight);
	}

	m_isVisible = false;
}

void FireRenderSky::attachToScene()
{
	if (m_isVisible)
		return;

	if (auto scene = Scene())
	{
		scene.Attach(m_envLight);
		if (m_sunLight.Handle()) // m_sunLight could be empty if IBL is used for lighting
			scene.Attach(m_sunLight);
		m_isVisible = true;
	}
}


