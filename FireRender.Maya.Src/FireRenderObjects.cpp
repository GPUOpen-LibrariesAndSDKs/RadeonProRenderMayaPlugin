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
#include "Context/TahoeContext.h"
#include "FireRenderUtils.h"
#include "base_mesh.h"
#include "FireRenderDisplacement.h"
#include "SkyBuilder.h"

#include <float.h>
#include <array>
#include <algorithm>
#include <vector>
#include <iterator>

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
#include <maya/MFloatPointArray.h>
#include <maya/MFloatMatrix.h>
#include <maya/MRenderUtil.h> 
#include <maya/MFnPluginData.h> 
#include <maya/MPxData.h>
#include <maya/MAnimUtil.h>

#include <istream>
#include <ostream>
#include <sstream>
#include <fstream>

#include <Xgen/src/xgsculptcore/api/XgSplineAPI.h>

#include <maya/MUuid.h>
#include "Lights/PhysicalLight/PhysicalLightAttributes.h"

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

FireRenderObject::FireRenderObject(const FireRenderObject& rhs, const std::string& uuid)
{
	m.context = rhs.m.context;
	m.object = rhs.Object();
	m.uuid = uuid;
	if (!rhs.Object().isNull())
		rhs.m.context->setDirtyObject(this);
	m_isPortal_IBL = false;
	m_isPortal_SKY = false;
}

FireRenderObject::~FireRenderObject()
{
	FireRenderObject::clear();
}

std::string FireRenderObject::uuid() const
{
	return m.uuid;
}

std::string FireRenderObject::uuidWithoutInstanceNumber() const
{
	// We assume that real guid and instance number are separated by colon
	return FireRenderObject::uuidWithoutInstanceNumberForString(m.uuid);
}

std::string FireRenderObject::uuidWithoutInstanceNumberForString(const std::string& uuid)
{
	return splitString<std::string>(uuid, ':')[0];
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
	return hash;
}

HashValue FireRenderObject::CalculateHash()
{
	return GetHash(Object());
}

HashValue FireRenderNode::CalculateHash()
{
	HashValue hash = FireRenderObject::CalculateHash();
	auto dagPath = DagPath();

	if (dagPath.isValid())
	{
		hash << dagPath.isVisible();
		hash << dagPath.inclusiveMatrix();
	}
	return hash;
}

void FireRenderNode::MarkDirtyTransformRecursive(const MFnTransform& transform)
{
	unsigned int childCount = transform.childCount();

	for (unsigned int childIndex = 0; childIndex < childCount; ++childIndex)
	{
		MObject child = transform.child(childIndex);

		if (child.hasFn(MFn::kTransform))
		{
			MarkDirtyTransformRecursive(MFnTransform(child));
		}
	}

	MarkDirtyAllDirectChildren(transform);
}


void FireRenderNode::MarkDirtyAllDirectChildren(const MFnTransform& transform)
{
	MDagPath transformDagPath;
	MStatus status = transform.getPath(transformDagPath);

	unsigned int childCount = 0;
	status = transformDagPath.numberOfShapesDirectlyBelow(childCount);

	for (unsigned int childIndex = 0; childIndex < childCount; ++childIndex)
	{
		MDagPath childDagPath = transformDagPath;
		childDagPath.extendToShapeDirectlyBelow(childIndex);

		FireRenderObject* pObject = context()->getRenderObject(childDagPath);

		if (pObject == nullptr && context()->GetCamera().DagPath() == childDagPath)
		{
			pObject = &context()->GetCamera();
		}

		if (pObject != nullptr)
		{
			context()->setDirtyObject(pObject);
		}
	}
}

void FireRenderNode::OnPlugDirty(MObject& node, MPlug &plug)
{
	FireRenderObject::OnPlugDirty(node, plug);

	MString partialShortName = plug.partialName();
	if (partialShortName == "fruuid")
		return;

	if (node.hasFn(MFn::kTransform))
	{
		MFnTransform transform(node);

		// check for RPRObjectId attrbiute change. roi is brief name for this attribute
		if (partialShortName == "roi")
		{
			MarkDirtyAllDirectChildren(transform);
		}

		// If changeing render layers or collections inside render layer
		if (partialShortName.indexW("rlio[") != -1)
		{
			MarkDirtyTransformRecursive(transform);
		}

		// if translattion is changed on the parent transform, make sure all children marked dirty
		static std::set<std::string> attributeSet = { "t", "tx", "ty", "tz",
												"r", "rx", "ry", "rz",
												"s", "sx", "sy", "sz" };

		if (attributeSet.find(std::string(partialShortName.asChar())) != attributeSet.end())
		{
			MarkDirtyTransformRecursive(transform);
		}
	}

	setDirty();
}

void FireRenderNode::OnWorldMatrixChanged()
{
	m_bIsTransformChanged = true;
	setDirty();
}

MMatrix FireRenderNode::GetSelfTransform()
{
	return DagPath().inclusiveMatrix();
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

void FireRenderObject::Freshen(bool shouldCalculateHash)
{
	if (m.callbackId.empty())
		RegisterCallbacks();

	if (shouldCalculateHash)
	{
		m.hash = CalculateHash();
	}
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

void FireRenderObject::OnPlugDirty(MObject& node, MPlug& plug)
{
	MFnDependencyNode nodeFn(node);
	MString nodeName = nodeFn.name();
	MString name = plug.partialName(false, true, true, true, true, true);
	if (name == "visibility" || name == "drawOverride") 
	{
		SetAllChildrenDirty();
	}
}

void FireRenderObject::SetAllChildrenDirty() 
{
	if (!m.object.hasFn(MFn::kDagNode)) 
	{
		DebugPrint("Error > Can't cast to DAG node (%s)", m.object.apiTypeStr());
		return;
	}

	MFnDagNode dagNode(m.object);
	const auto childCount = dagNode.childCount();
	if (childCount == 0) 
	{
		setDirty();
	}
	else 
	{
		for (unsigned i = 0; i < dagNode.childCount(); i++) 
		{
			MObject child = dagNode.child(i);
			FireRenderObject* childAsRenderObject = context()->getRenderObject(child);
			//Null check because cameras (and maybe some other objects) could be in DAG, but they not stored as sceneObject
			if (childAsRenderObject != nullptr) 
			{
				childAsRenderObject->SetAllChildrenDirty();
			}
		}
	}
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

FireRenderNode::FireRenderNode(const FireRenderNode& rhs, const std::string& uuid)
	: FireRenderObject(rhs, uuid)
{ }

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
					ob->Freshen(ob->context()->GetRenderType() == RenderType::ViewportRender);	// make sure we have shapes to attach
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
	MDagPath outPath;
	bool found = context()->GetNodePath(outPath, uuid());

	if (found)
	{
		return outPath;
	}

	MFnDagNode dagNode(Object());
	MDagPathArray pathArray;
	dagNode.getAllPaths(pathArray);

	if (m.instance < pathArray.length())
	{
		context()->AddNodePath(pathArray[m.instance], uuid());
		
		return pathArray[m.instance];
	}

	return MDagPath();
}

//===================
// Mesh
//===================
FireRenderMeshCommon::FireRenderMeshCommon(FireRenderContext* context, const MDagPath& dagPath) :
	FireRenderNode(context, dagPath)
{}

FireRenderMeshCommon::FireRenderMeshCommon(const FireRenderMeshCommon& rhs, const std::string& uuid)
	: FireRenderNode(rhs, uuid)
{}

FireRenderMeshCommon::~FireRenderMeshCommon()
{
	FireRenderObject::clear();
}

FireRenderMesh::FireRenderMesh(FireRenderContext* context, const MDagPath& dagPath) :
	FireRenderMeshCommon(context, dagPath), m_SkipCallbackCounter(0)
{
}

FireRenderMesh::FireRenderMesh(const FireRenderMesh& rhs, const std::string& uuid)
	: FireRenderMeshCommon(rhs, uuid), m_SkipCallbackCounter(0)
{
}

FireRenderMesh::~FireRenderMesh()
{
	FireRenderMesh::clear();
}


void FireRenderMesh::clear()
{
	FireRenderObject::clear();
}

void FireRenderMeshCommon::detachFromScene()
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

		m_isVisible = false;
	}
}

void FireRenderMeshCommon::attachToScene()
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
	for (auto& element : m.elements)
	{
		for (auto& shadingEngine : element.shadingEngines)
		{
			if (shadingEngine.isNull())
				continue;

			MObject shaderOb = getSurfaceShader(shadingEngine);

			MFnDependencyNode fnShdr(shaderOb);
			std::string shdrName = fnShdr.name().asChar();

			MFnDagNode dagMesh(Object());
			MString thisName = dagMesh.fullPathName();

			if (!shaderOb.isNull())
			{
				MStatus returnStatus;
				AddCallback(MNodeMessage::addNodeDirtyCallback(shaderOb, ShaderDirtyCallback, this, &returnStatus));
				assert(returnStatus == MStatus::kSuccess);
			}

			MObject shaderDi = getDisplacementShader(shadingEngine);
			if (!shaderDi.isNull())
			{
				AddCallback(MNodeMessage::addNodeDirtyCallback(shaderDi, ShaderDirtyCallback, this));
			}

			MObject shaderVolume = getVolumeShader(shadingEngine);
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
		m.elements.push_back( FrElement{ sphere } );
	}
}

void FireRenderMesh::SetReloadedSafe() 
{
	if (IsMainInstance() && IsInitialized())
	{
		m.isPreProcessed = true;
	}
}

void FireRenderMeshCommon::setRenderStats(MDagPath dagPath)
{
	MFnDependencyNode depNode(dagPath.node());

	MPlug visibleInReflectionsPlug = depNode.findPlug("visibleInReflections");
	bool visibleInReflections;
	visibleInReflectionsPlug.getValue(visibleInReflections);

	MPlug visibleInRefractionsPlug = depNode.findPlug("visibleInRefractions");
	bool visibleInRefractions;
	visibleInRefractionsPlug.getValue(visibleInRefractions);

	MPlug castsShadowsPlug = depNode.findPlug("castsShadows");
	bool castsShadows;
	castsShadowsPlug.getValue(castsShadows);

	MPlug receivesShadowsPlug = depNode.findPlug("receiveShadows");
	bool receiveShadows;
	receivesShadowsPlug.getValue(receiveShadows);

	MPlug primaryVisibilityPlug = depNode.findPlug("primaryVisibility");
	bool primaryVisibility;
	primaryVisibilityPlug.getValue(primaryVisibility);

	bool isVisisble = IsMeshVisible(dagPath, context());

	setVisibility(isVisisble);

	MFnDagNode mdag(dagPath.node());
	MFnDependencyNode parentTransform(mdag.parent(0));
	MPlug contourVisibilityPlug = parentTransform.findPlug("RPRContourVisibility");
	bool isVisibleInContour = false;
	if (!contourVisibilityPlug.isNull())
	{
		MStatus res = contourVisibilityPlug.getValue(isVisibleInContour);
		CHECK_MSTATUS(res);
	}

	setPrimaryVisibility(primaryVisibility);

	setReflectionVisibility(visibleInReflections);

	setRefractionVisibility(visibleInRefractions);

	if (context()->IsContourModeSupported())
	{
		setContourVisibility(isVisibleInContour);
	}

	setCastShadows(castsShadows);

	setReceiveShadows(receiveShadows);
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

void FireRenderMeshCommon::setVisibility(bool visibility)
{
	if (visibility)
		attachToScene();
	else
		detachFromScene();
}

void FireRenderMeshCommon::setReflectionVisibility(bool reflectionVisibility)
{
	for (auto& element : m.elements)
	{
		for (auto& shader : element.shaders)
		{
			if (shader.IsShadowCatcher() || shader.IsReflectionCatcher())
			{
				if (auto shape = element.shape)
					shape.SetReflectionVisibility(false);

				break;
			}

			if (auto shape = element.shape)
				shape.SetReflectionVisibility(reflectionVisibility);
		}
	}
}

void FireRenderMeshCommon::setRefractionVisibility(bool refractionVisibility)
{
	for (auto& element : m.elements)
	{
		for (auto& shader : element.shaders)
		{
			if (shader.IsShadowCatcher() || shader.IsReflectionCatcher())
			{
				if (auto shape = element.shape)
					shape.setRefractionVisibility(false);

				break;
			}

			if (auto shape = element.shape)
				shape.setRefractionVisibility(refractionVisibility);
		}
	}
}

void FireRenderMeshCommon::setCastShadows(bool castShadow)
{
	for (auto element : m.elements)
	{
		if (auto shape = element.shape)
			shape.SetShadowFlag(castShadow);
	}
}

void FireRenderMeshCommon::setReceiveShadows(bool receiveShadow)
{
	for (auto element : m.elements)
	{
		if (auto shape = element.shape)
			shape.SetReceiveShadowFlag(receiveShadow);
	}
}

void FireRenderMeshCommon::setPrimaryVisibility(bool primaryVisibility)
{
	for (auto element : m.elements)
	{
		if (auto shape = element.shape)
			shape.SetPrimaryVisibility(primaryVisibility);
	}
}

void FireRenderMeshCommon::setContourVisibility(bool contourVisibility)
{
	for (auto element : m.elements)
	{
		if (auto shape = element.shape)
			shape.SetContourVisibilityFlag(contourVisibility);
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

	AddCallback(MDagMessage::addWorldMatrixModifiedCallback(dagPath, WorldMatrixChangedCallback, this));
}

bool FireRenderMesh::setupDisplacement(std::vector<MObject>& shadingEngines, frw::Shape shape)
{
	if (!shape)
		return false;

	bool haveDisplacement = false;
	
	for (auto& shadingEngine : shadingEngines)
	{

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

			if (!displacement)
			{
				// try using uber material params (displacement)
				MObject surfaceShader = getSurfaceShader(shadingEngine);
				MFnDependencyNode shaderNode(surfaceShader);
				MPlug plug = shaderNode.findPlug("displacementEnable");
				if (!plug.isNull())
				{
					bool isDisplacementEnabled = false;
					plug.getValue(isDisplacementEnabled);

					if (isDisplacementEnabled)
					{
						float minHeight = 0;
						float maxHeight = 0;
						int subdivision = 0;
						float creaseWeight = 0;
						int boundary = RPR_SUBDIV_BOUNDARY_INTERFOP_TYPE_EDGE_AND_CORNER;
						frw::Value mapValue;
						bool isAdaptive = false;
						float adaptiveFactor = 0.0f;

						plug = shaderNode.findPlug("displacementMin");
						if (!plug.isNull())
							plug.getValue(minHeight);

						plug = shaderNode.findPlug("displacementMax");
						if (!plug.isNull())
							plug.getValue(maxHeight);

						plug = shaderNode.findPlug("displacementSubdiv");
						if (!plug.isNull())
							plug.getValue(subdivision);

						plug = shaderNode.findPlug("displacementCreaseWeight");
						if (!plug.isNull())
							plug.getValue(creaseWeight);

						auto scope = Scope();
						mapValue = scope.GetConnectedValue(shaderNode.findPlug("displacementMap"));
						bool haveMap = mapValue.IsNode();

						plug = shaderNode.findPlug("displacementBoundary");
						if (!plug.isNull())
						{
							int n = 0;
							if (MStatus::kSuccess == plug.getValue(n))
							{
								FireMaya::Displacement::Type b = static_cast<FireMaya::Displacement::Type>(n);
								if (b == FireMaya::Displacement::kDisplacement_EdgeAndCorner)
								{
									boundary = RPR_SUBDIV_BOUNDARY_INTERFOP_TYPE_EDGE_AND_CORNER;
								}
								else
								{
									boundary = RPR_SUBDIV_BOUNDARY_INTERFOP_TYPE_EDGE_ONLY;
								}
							}
						}

						plug = shaderNode.findPlug("displacementEnableAdaptiveSubdiv");
						if (!plug.isNull())
							plug.getValue(isAdaptive);

						plug = shaderNode.findPlug("displacementASubdivFactor");
						if (!plug.isNull())
							plug.getValue(adaptiveFactor);

						if (haveMap)
						{
							shape.SetDisplacement(mapValue, minHeight, maxHeight);
							if (!isAdaptive)
							{
								shape.SetSubdivisionFactor(subdivision);
							}
							else
							{
								FireRenderContext *ctx = this->context();

								frw::Scene scn = ctx->GetScene();
								frw::Camera cam = scn.GetCamera();
								frw::Context ctx2 = scn.GetContext();
								rpr_framebuffer fb = ctx->frameBufferAOV(RPR_AOV_COLOR);

								shape.SetAdaptiveSubdivisionFactor(adaptiveFactor, ctx->height(), cam.Handle(), fb, true /*isRPR20*/);
							}
							shape.SetSubdivisionCreaseWeight(creaseWeight);
							shape.SetSubdivisionBoundaryInterop(boundary);

							haveDisplacement = true;
						}
					}
				}
			}

			if (displacement)
			{
				FireMaya::Displacement::DisplacementParams params;

				auto scope = Scope();
				haveDisplacement = displacement->getValues(scope, params);

				if (haveDisplacement)
				{
					shape.SetDisplacement(params.map, params.minHeight, params.maxHeight);
					shape.SetSubdivisionFactor(params.subdivision);
					shape.SetSubdivisionCreaseWeight(params.creaseWeight);
					shape.SetSubdivisionBoundaryInterop(params.boundary);

					return haveDisplacement;
				}
			}
		}
	}

	// should be here only if there are no displacement in any of shading engines
	if (!haveDisplacement)
	{
		shape.RemoveDisplacement();
	}

	return haveDisplacement;
}

void FireRenderMesh::ReinitializeMesh(const MDagPath& meshPath)
{
	MMatrix mMtx = meshPath.inclusiveMatrix();

	setVisibility(false);

	m.elements.clear();

	// node is not visible => skip
	if (IsMeshVisible(meshPath, this->context()))
	{
		m.elements.emplace_back();
		GetShapes(m.elements.back().shape);
	}
}

rpr_int rprxMaterialGetParameterType(rpr_material_node material, rpr_uint parameter, rpr_parameter_type* out_type)
{
	rpr_int status = RPR_SUCCESS;
	rpr_material_node material_RPR = (rpr_material_node)material;

	uint64_t nbInput = 0;
	status = rprMaterialNodeGetInfo(material_RPR, RPR_MATERIAL_NODE_INPUT_COUNT, sizeof(nbInput), &nbInput, NULL);
	if (status != RPR_SUCCESS)
		return status;

	int input_idx = 0;
	bool found = false;
	for (input_idx = 0; input_idx < nbInput; input_idx++)
	{
		rpr_uint inputName = 0;
		status = rprMaterialNodeGetInputInfo(material_RPR, input_idx, RPR_MATERIAL_NODE_INPUT_NAME, sizeof(inputName), &inputName, NULL);

		if (status != RPR_SUCCESS)
			return status;

		if (inputName == parameter)
		{
			found = true;
			break;
		}
	}

	if (!found)
		return RPR_ERROR_INVALID_PARAMETER;

	status = rprMaterialNodeGetInputInfo(material_RPR, input_idx, RPR_MATERIAL_NODE_INPUT_TYPE, sizeof(rpr_parameter_type), out_type, NULL);
	return status;
}


rpr_int rprxMaterialGetParameterValue(rpr_material_node material, rpr_uint parameter, void* out_value)
{
	rpr_int status = RPR_SUCCESS;
	rpr_material_node material_RPR = (rpr_material_node)material;

	uint64_t nbInput = 0;
	status = rprMaterialNodeGetInfo(material_RPR, RPR_MATERIAL_NODE_INPUT_COUNT, sizeof(nbInput), &nbInput, NULL);
	if (status != RPR_SUCCESS)
		return status;

	int input_idx = 0;
	bool found = false;
	for (input_idx = 0; input_idx < nbInput; input_idx++)
	{
		rpr_uint inputName = 0;
		status = rprMaterialNodeGetInputInfo(material_RPR, input_idx, RPR_MATERIAL_NODE_INPUT_NAME, sizeof(inputName), &inputName, NULL);

		if (status != RPR_SUCCESS)
			return status;

		if (inputName == parameter)
		{
			found = true;
			break;
		}
	}

	if (!found)
		return RPR_ERROR_INVALID_PARAMETER;

	rpr_parameter_type out_type = (rpr_parameter_type)0;
	status = rprMaterialNodeGetInputInfo(material_RPR, input_idx, RPR_MATERIAL_NODE_INPUT_TYPE, sizeof(rpr_parameter_type), &out_type, NULL);
	if (status != RPR_SUCCESS)
		return status;

	size_t value_size = 0;
	status = rprMaterialNodeGetInputInfo(material_RPR, input_idx, RPR_MATERIAL_NODE_INPUT_VALUE, 0, NULL, &value_size);
	if (status != RPR_SUCCESS)
		return status;

	switch (out_type)
	{
		case RPR_MATERIAL_NODE_INPUT_TYPE_FLOAT4:
		{
			auto out_ptr = reinterpret_cast<float*>(out_value);
			status = rprMaterialNodeGetInputInfo(material_RPR, input_idx, RPR_MATERIAL_NODE_INPUT_VALUE, value_size, out_ptr, NULL);
			if (status != RPR_SUCCESS)
				return status;

			break;
		}
		case RPR_MATERIAL_NODE_INPUT_TYPE_UINT:
		{
			auto out_ptr = reinterpret_cast<rpr_uint*>(out_value);
			status = rprMaterialNodeGetInputInfo(material_RPR, input_idx, RPR_MATERIAL_NODE_INPUT_VALUE, value_size, out_ptr, NULL);
			if (status != RPR_SUCCESS)
				return status;

			break;
		}
		case RPR_MATERIAL_NODE_INPUT_TYPE_NODE:
		{
			auto out_ptr = reinterpret_cast<rpr_material_node*>(out_value);
			status = rprMaterialNodeGetInputInfo(material_RPR, input_idx, RPR_MATERIAL_NODE_INPUT_VALUE, value_size, out_ptr, NULL);
			if (status != RPR_SUCCESS)
				return status;

			break;
		}
		default:
			break;
	}

	return status;
}

bool IsUberEmissive(frw::Shader shader)
{
	// back-off
	if (!shader)
	{
		return false;
	}

	rpr_material_node mHandle = shader.Handle();

	rpr_parameter_type rprType = 0;
	rpr_int res = rprxMaterialGetParameterType(
		mHandle,
		RPR_MATERIAL_INPUT_UBER_EMISSION_WEIGHT,
		&rprType);

	float emissionWeightValue[] = { 0.0f, 0.0f, 0.0f, 0.0f };

	res = rprxMaterialGetParameterValue(
		mHandle,
		RPR_MATERIAL_INPUT_UBER_EMISSION_WEIGHT,
		&emissionWeightValue);

	if (emissionWeightValue[0] > 0.0f)
		return true;

	return false;
}

void FireRenderMesh::ProcessMesh(const MDagPath& meshPath)
{
	FireRenderContext* context = this->context();

	MFnDependencyNode nodeFn(Object());

	for (auto& element : m.elements)
	{
		element.shaders.clear();

		if (!element.shape)
		{
			return;
		}
		assert(element.shape);

		element.shape.SetShader(nullptr);

		unsigned int shaderIdx = 0;
		for (; shaderIdx < element.shadingEngines.size(); ++shaderIdx)
		{
			MObject& shadingEngine = element.shadingEngines[shaderIdx];

			MObject surfaceShader = getSurfaceShader(shadingEngine);
			if (surfaceShader.isNull())
				continue;

			element.shaders.push_back(context->GetShader(surfaceShader, shadingEngine, this, m.changed.shader));

			const std::vector<int>& faceMaterialIndices = GetFaceMaterialIndices();
			std::vector<int> face_ids;
			face_ids.reserve(faceMaterialIndices.size());
			for (int faceIdx = 0; faceIdx < faceMaterialIndices.size(); ++faceIdx)
			{
				if (faceMaterialIndices[faceIdx] == shaderIdx)
					face_ids.push_back(faceIdx);
			}

			if (!face_ids.empty() && (element.shadingEngines.size() != 1))
			{
				element.shape.SetPerFaceShader(element.shaders.back(), face_ids);
			}
			else
			{
				element.shape.SetShader(element.shaders.back());
			}

			frw::ShaderType shType = element.shaders.back().GetShaderType();
			if (shType == frw::ShaderTypeEmissive)
				m.isEmissive = true;

			if (element.shaders.back().IsShadowCatcher() || element.shaders.back().IsReflectionCatcher())
				break;

			if ((shType == frw::ShaderTypeRprx) && (IsUberEmissive(element.shaders.back())))
			{
				m.isEmissive = true;
			}
		}

		if (context->IsDisplacementSupported())
		{
			bool haveDispl = setupDisplacement(element.shadingEngines, element.shape);
		}

		MObject volumeShader = MObject::kNullObj;
		for (auto& it : element.shadingEngines)
		{
			volumeShader = getVolumeShader(it);

			if (volumeShader != MObject::kNullObj)
				break;
		}

		if (volumeShader != MObject::kNullObj)
		{
			element.volumeShader = context->GetVolumeShader(volumeShader);
		}

		if (!element.volumeShader)
		{
			for (auto& it : element.shadingEngines)
			{
				MObject surfaceShader = getSurfaceShader(it);
				if (surfaceShader == MObject::kNullObj)
					continue;

				frw::Shader volumeShader = context->GetVolumeShader(surfaceShader);
				if (!volumeShader.IsValid())
					continue;

				element.volumeShader = volumeShader;
				break;
			}
		}

		// if no valid surface shader, we should set to transparent in case of volumes present
		if (element.volumeShader)
		{
			if ((element.shaders.size() == 0) ||
				((element.shaders.size() == 1) && (element.shaders[0] == element.volumeShader))
				)
			{
				// also catch case where volume assigned to surface
				element.shaders.push_back(frw::TransparentShader(context->GetMaterialSystem()));
				element.shape.SetShader(element.shaders.back());
			}

			element.shape.SetVolumeShader(element.volumeShader);
		}
	}

	RebuildTransforms();

	// motion blur
	ProcessMotionBlur(MFnDagNode(Object()));

	setRenderStats(meshPath);
}

void FireRenderMesh::ProcessIBLLight(void)
{
	FireRenderContext *context = this->context();

	MObject temp = context->iblTransformObject;
	MFnTransform iblLightTransform(temp);

	if (iblLightTransform.isParentOf(Object()))
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

				if (skyLightTransform.isParentOf(Object()))
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

void FireRenderMesh::ProcessSkyLight(void)
{
	FireRenderContext *context = this->context();

	MObject temp = context->skyTransformObject;
	MFnTransform skyLightTransform(temp);

	if (skyLightTransform.isParentOf(Object()))
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

void FireRenderMesh::Rebuild()
{
//************************************************************************************************************************
// TODO: this segment should be moved into separate function as we have a bit of copy-past with mesh pre-processing (for reasons)
	auto node = Object();
	MFnDagNode meshFn(node);
	MDagPath meshPath = DagPath();

	const FireRenderContext* context = this->context();

	MObjectArray shadingEngines = GetShadingEngines(meshFn, Instance());

	m.isEmissive = false;

#ifdef _DEBUG
	MString name = meshFn.name();
#endif

	// If there is just one shader and the number of shader is not changed then just update the shader
	bool shadersChanged = false;
	shadersChanged = (m.elements.size() > 0) && (shadingEngines.length() != m.elements.back().shaders.size());

	if (m.changed.mesh || shadersChanged || (m.elements.size() == 0))
	{
		// the number of shader has changed so reload the mesh
		ReinitializeMesh(meshPath);
	}

//****************************************************************************************************************

	// Assignment should be before callbacks registering, because RegisterCallbacks() use them
	AssignShadingEngines(shadingEngines);

	// Callback registering should be before ProcessMesh() because shaders could add own callbacks that would be erased by RegisterCallbacks()
	RegisterCallbacks();	// we need to do this in case the shaders change (ie we will need to attach new callbacks)
	
	if (meshPath.isValid())
	{
		ProcessMesh(meshPath);
	}

	if (this->context()->iblLight)
	{
		ProcessIBLLight();
	}

	if (this->context()->skyLight)
	{
		ProcessSkyLight();
	}

	SetupObjectId(meshPath.transform());
	if (context->IsShadowColorSupported())
	{
		SetupShadowColor();
	}

	m.changed.mesh = false;
	m.changed.transform = false;
	m.changed.shader = false;
}

void FireRenderMesh::SetupObjectId(MObject parentTransformObject)
{
	MObject node = Object();
	MFnDependencyNode parentTransform(parentTransformObject);

	MPlug plug = parentTransform.findPlug("RPRObjectId");

	rpr_uint objectId = 0;
	if (!plug.isNull())
	{
		objectId = plug.asInt();
	}

	for (auto& element : m.elements)
	{
		if (element.shape.Handle() != nullptr)
		{
			if (context()->IsMeshObjectIDSupported())
			{
				element.shape.SetObjectId(objectId);
			}
		}
	}
}

void FireRenderMesh::SetupShadowColor()
{
	MObject node = Object();
	MPlug plug = MFnDependencyNode(node).findPlug("RPRShadowColor");

	if (plug.isNull())
	{
		return;
	}

	frw::Value colorValue = Scope().GetValue(plug);

	for (FrElement element : m.elements)
	{
		if (element.shape.Handle() != nullptr)
		{
			element.shape.SetShadowColor(colorValue);
		}
	}
}

void FireRenderMeshCommon::ForceShaderDirtyCallback(MObject& node, void* clientData)
{
	if (nullptr == clientData)
	{
		return;
	}

	// clientData should be FireRenderMesh
	FireRenderMesh* self = static_cast<FireRenderMesh*>(clientData);

	for (auto& element : self->m.elements)
	{
		for (auto& shadingEngine : element.shadingEngines)
		{
			MObject shaderOb = getSurfaceShader(shadingEngine);
			MGlobal::executeCommand("dgdirty " + MFnDependencyNode(shaderOb).name());
		}
	}
}

void FireRenderMeshCommon::AddForceShaderDirtyDependOnOtherObjectCallback(MObject dependency)
{
	AddCallback(MNodeMessage::addNodeDirtyCallback(dependency, ForceShaderDirtyCallback, this));
}

bool FireRenderMesh::IsMeshVisible(const MDagPath& meshPath, const FireRenderContext* context) const
{
	bool selectionCheck = true; // true => is rendered

	bool shouldIgnoreRenderSelectedObjects =
		context->GetRenderType() == RenderType::ViewportRender;

	if (!shouldIgnoreRenderSelectedObjects)
	{
		bool isRenderSelectedModeEnabled = context->renderSelectedObjectsOnly();
		selectionCheck = !isRenderSelectedModeEnabled || IsSelected(meshPath);
	}

	bool isVisible = meshPath.isVisible() && selectionCheck;

	return isVisible;
}

void FireRenderMesh::GetShapes(frw::Shape& outShape)
{
	FireRenderContext* context = this->context();

	FireRenderMeshCommon* mainMesh = context->GetMainMesh(uuid());

	if ((mainMesh != nullptr) && !mainMesh->IsNotInitialized())
	{
		assert(!m.elements.empty());

		outShape = mainMesh->Elements().back().shape.CreateInstance(Context());

		m.isMainInstance = false;
	}

	MDagPath dagPath = DagPath();

	if ((mainMesh != nullptr) && (mainMesh != this) && (mainMesh->IsNotInitialized()))
	{
		// should not be here!
		assert(false);
	}

	if ((mainMesh == nullptr) || (mainMesh == this))
	{
		assert(IsNotInitialized());

		bool success = TranslateMeshWrapped(dagPath, outShape);
		assert(success);
	}

	MString fullPathName = dagPath.fullPathName();
	std::string shapeName = std::string(fullPathName.asChar()) + "_" + std::to_string(0);
	outShape.SetName(shapeName.c_str());

	SaveUsedUV(Object());
}

void FireRenderMesh::ProccessSmoothCallbackWorkaroundIfNeeds(const MObject& object)
{
	if (!object.hasFn(MFn::kMesh))
	{
		return;
	}

	DependencyNode attributes(object);
	if (!attributes.getBool("displaySmoothMesh"))
	{
		return;
	}

	// check hierarchy up thorugh parents to analyze do they have animation tracks or not
	MFnDagNode node(object);
	
	bool foundAnimatedGrandParent = false;
	for (unsigned int indexParent = 0; indexParent < node.parentCount(); ++indexParent)
	{
		MObject parent = MFnDagNode(node.parent(indexParent)).parent(0); // get grand parent

		while (!parent.isNull())
		{
			// check if parent is animated
			MSelectionList selList;
			selList.add(parent);

			if (MAnimUtil::isAnimated(selList))
			{
				foundAnimatedGrandParent = true;
				break;
			}

			parent = MFnDagNode(parent).parent(0);
		}
	}

	if (foundAnimatedGrandParent)
	{
		// duplicate polysmooth and deleteNode trigger callback on mesh node - nodeDirty and attributeChanged(world_matrix[0])
		m_SkipCallbackCounter += 2;
	}
}

bool FireRenderMesh::TranslateMeshWrapped(const MDagPath& dagPath, frw::Shape& outShape)
{
	assert(IsMainInstance()); // should already be main instance at this point

	if (!m_meshData.IsInitialized())
		return false;

	FireRenderContext* context = this->context();
	bool deformationMotionBlurEnabled = IsMotionBlurEnabled(MFnDagNode(dagPath.node())) && NorthStarContext::IsGivenContextNorthStar(context) && !context->isInteractive();
	unsigned int motionSamplesCount = deformationMotionBlurEnabled ? context->motionSamples() : 0;

	//Ignore set objects dirty calls while creating a mesh, because it might lead to infinite lookps in case if deformtion motion blur is used
	{
		ContextSetDirtyObjectAutoLocker locker(*context);
		MFnDagNode dagNode(Object());
		MString name = dagNode.fullPathName();
		assert(m_meshData.IsInitialized());

		ProccessSmoothCallbackWorkaroundIfNeeds(Object());
		outShape = FireMaya::MeshTranslator::TranslateMesh(m_meshData, context->GetContext(), Object(), m.faceMaterialIndices, motionSamplesCount, dagPath.fullPathName());
	}

	m.isPreProcessed = false;

	return true;
}

void FireRenderMesh::SaveUsedUV(const MObject& meshNode)
{
	if (!meshNode.hasFn(MFn::kMesh))
		return;

	MStatus mstatus;
	MFnMesh fnMesh(meshNode, &mstatus);
	if (mstatus != MStatus::kSuccess)
	{
		return;
	}

	MStringArray uvSetNames;
	mstatus = fnMesh.getUVSetNames(uvSetNames);
	if (mstatus != MStatus::kSuccess)
	{
		return;
	}

	int uvSetCount = fnMesh.numUVSets();
	for (int idx = 0; idx < uvSetCount; ++idx)
	{
		MString tmpName = uvSetNames[idx];
		MObjectArray textures;
		mstatus = fnMesh.getAssociatedUVSetTextures(tmpName, textures);

		int texturesCount = textures.length();
		for (int texture_idx = 0; texture_idx < texturesCount; texture_idx++)
		{
			MObject tObject = textures[texture_idx];

			MFnDependencyNode fnNode(tObject);
			MObject attrObj = fnNode.attribute("fileTextureName");
			MFnAttribute attr(attrObj);
			MPlug plug = fnNode.findPlug(attr.object(), &mstatus);
			CHECK_MSTATUS(mstatus);
			MString textureFilePath;
			mstatus = plug.getValue(textureFilePath);

			// only one uv coordinates set can be attached to texture file
			// this is the limitation of Maya's relationship editor
			m_uvSetCachedMappingData[textureFilePath.asChar()] = idx;
		}		
	}
}

void FireRenderMesh::RebuildTransforms()
{
	MObject node = Object();
	MFnDagNode meshFn(node);
	MDagPath meshPath = DagPath();

	MMatrix matrix = GetSelfTransform();
	
	// convert Maya mesh in cm to m
	float mfloats[4][4];
	FireMaya::ScaleMatrixFromCmToMFloats(matrix, mfloats);	

	if ((!m.elements.empty()) && (m.elements.back().shape))
	{
		m.elements.back().shape.SetTransform(&mfloats[0][0]);
	}
}

void FireRenderMeshCommon::AssignShadingEngines(const MObjectArray& shadingEngines)
{
	for (auto& element : m.elements)
	{
		WriteMayaArrayTo(element.shadingEngines, shadingEngines);
	}
}

bool FireRenderMeshCommon::IsMotionBlurEnabled(const MFnDagNode& meshFn)
{
	// Checking of MotionBlur parameter in RenderStats group of mesh
	bool objectMotionBlur = true;
	MPlug objectMBPlug = meshFn.findPlug("motionBlur");

	if (!objectMBPlug.isNull())
	{
		objectMotionBlur = objectMBPlug.asBool();
	}

	return (context()->motionBlur() && objectMotionBlur);
}

void FireRenderMeshCommon::ProcessMotionBlur(const MFnDagNode& meshFn)
{
	if (!IsMotionBlurEnabled(meshFn))
	{
		return;
	}

	assert(NorthStarContext::IsGivenContextNorthStar(context()));

	float nextFrameFloats[4][4];
	FireMaya::GetMatrixForTheNextFrame(meshFn, nextFrameFloats, Instance());

	for (auto element : m.elements)
	{
		if (element.shape)
		{
			element.shape.SetMotionTransform(&nextFrameFloats[0][0], false);
		}
	}
}

const std::vector<int>& FireRenderMeshCommon::GetFaceMaterialIndices(void) const
{
	const FireRenderContext* context = this->context();
	const FireRenderMeshCommon* mainMesh = context->GetMainMesh(uuid());

	if (mainMesh != nullptr)
	{
		return mainMesh->m.faceMaterialIndices;
	}

	return m.faceMaterialIndices;
}

void FireRenderMesh::OnNodeDirty()
{
	if (m_SkipCallbackCounter > 0)
	{
		m_SkipCallbackCounter--;
		return;
	}

	m.changed.mesh = true;
	setDirty();
}

void FireRenderMesh::OnPlugDirty(MObject& node, MPlug& plug)
{
	if (m_SkipCallbackCounter > 0)
	{
		m_SkipCallbackCounter--;
		return;
	}

	FireRenderNode::OnPlugDirty(node, plug);
}

void FireRenderMesh::OnShaderDirty()
{
	m.changed.shader = true;
	setDirty();
}

void FireRenderMesh::ShaderDirtyCallback(MObject& node, void* clientData)
{
	DebugPrint("CALLBACK > ShaderDirtyCallback(%s)", node.apiTypeStr());

	MFnDependencyNode fnShdr(node);
	std::string shdrName = fnShdr.name().asChar();

	if (auto self = static_cast<FireRenderMesh*>(clientData))
	{
		assert(node != self->Object());
		self->OnShaderDirty();
	}
}

unsigned int FireRenderMeshCommon::GetAssignedUVMapIdx(const MString& textureFile) const
{
	auto it = m_uvSetCachedMappingData.find(textureFile.asChar());

	if (it == m_uvSetCachedMappingData.end())
		return -1;

	return it->second;
}

void FireRenderMesh::Freshen(bool shouldCalculateHash)
{
	Rebuild();
	FireRenderNode::Freshen(shouldCalculateHash);
}

HashValue FireRenderMesh::CalculateHash()
{
	auto hash = FireRenderNode::CalculateHash();

	if (!m.elements.empty())
	{
		auto& e = m.elements.back();
		hash << e.shadingEngines;
	}

	return hash;
}

bool FireRenderMesh::InitializeMaterials()
{
	auto node = Object();
	MFnDagNode meshFn(node);

	MObjectArray shadingEngines = GetShadingEngines(meshFn, Instance());

	// If there is just one shader and the number of shader is not changed then just update the shader
	bool onlyShaderChanged = !m.changed.mesh && (shadingEngines.length() == 0);

	return !onlyShaderChanged;
}

bool FireRenderMesh::ReloadMesh(unsigned int sampleIdx /*= 0*/)
{
	FireRenderContext* context = this->context();

	const FireRenderMeshCommon* mainMesh = context->GetMainMesh(uuid());

	if ((mainMesh != nullptr) && mainMesh->IsNotInitialized())
	{
		return true;
	}

	if ((mainMesh != nullptr) && (this != mainMesh))
	{
		return true;
	}

	bool success = false;
	MDagPath dagPath = DagPath();
	bool deformationMotionBlurEnabled = IsMotionBlurEnabled(MFnDagNode(dagPath.node())) && NorthStarContext::IsGivenContextNorthStar(context) && !context->isInteractive();
	unsigned int motionSamplesCount = deformationMotionBlurEnabled ? context->motionSamples() : 0;
	//Ignore set objects dirty calls while creating a mesh, because it might lead to infinite lookps in case if deformtion motion blur is used
	{
		ContextSetDirtyObjectAutoLocker locker(*context);

		ProccessSmoothCallbackWorkaroundIfNeeds(Object());
		success = FireMaya::MeshTranslator::PreProcessMesh(m_meshData, context->GetContext(), Object(), motionSamplesCount, sampleIdx, dagPath.fullPathName());
	}

	if (!success)
		return false;

	if (mainMesh == nullptr)
	{
		m.isMainInstance = true;
		context->AddMainMesh(this);
	}

	bool finishedPreProcessing = deformationMotionBlurEnabled ? (motionSamplesCount == sampleIdx + 1) : true;
	if (!finishedPreProcessing)
		return success;

	m.isPreProcessed = true;

	return success;
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

	FireMaya::Scope& scope = context()->GetScope();

	MFnDependencyNode fnNode(Object());
	std::string fnName = fnNode.name().asChar();

	std::string lightId = getNodeUUid(Object());
	auto shaderIds = scope.GetCachedShaderIds(lightId);
	for (auto it = shaderIds.first; it != shaderIds.second; ++it)
	{
		frw::Shader linkedShader = scope.GetCachedShader(it->second);
		linkedShader.ClearLinkedLight(GetFrLight().light);
	}
	if (std::distance(shaderIds.first, shaderIds.second) != 0)
	{
		scope.ClearCachedShaderIds(lightId);
	}

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

void FireRenderLight::UpdateTransform(const MMatrix& matrix)
{
	float matrixfloats[4][4];

	if (!m_light.areaLight)
	{
		FireMaya::ScaleMatrixFromCmToMFloats(matrix, matrixfloats);
		m_light.light.SetTransform((rpr_float*)matrixfloats);
	}
	else
	{
		// For area light we should apply areaWidth and areaLength to transform matrix in order to correctly pass it into core
		MTransformationMatrix transformation;

		float areaWidth = 0.0f;
		float areaLength = 0.0f;

		if (Object().hasFn(MFn::kAreaLight))
		{
			// Maya light has no physical light attributes, width and length should be default
			areaWidth = 1.0f;
			areaLength = 1.0f;
		}
		else
		{
			areaWidth = PhysicalLightAttributes::GetAreaWidth(Object());
			areaLength = PhysicalLightAttributes::GetAreaLength(Object());
		}

		double scale[3]{ areaWidth, areaWidth, areaLength };
		transformation.setScale(scale, MSpace::Space::kObject);
		FireMaya::ScaleMatrixFromCmToMFloats(transformation.asMatrix() * matrix, matrixfloats);

		m_light.areaLight.SetTransform((rpr_float*)matrixfloats);
	}
}

bool FireRenderLight::ShouldUpdateTransformOnly() const
{
	return m_bIsTransformChanged;
}

bool FireRenderPhysLight::ShouldUpdateTransformOnly() const
{
	return false;
}

PLType FireRenderPhysLight::GetPhysLightType(MObject node)
{
	MPlug plug = MFnDependencyNode(node).findPlug("lightType");

	return (PLType) plug.asInt();
}

void FireRenderLight::Freshen(bool shouldCalculateHash)
{
	if (ShouldUpdateTransformOnly())
	{
		MMatrix matrix = DagPath().inclusiveMatrix();
		UpdateTransform(matrix);
		m_bIsTransformChanged = false;
		return;
	}

	detachFromScene();
	m_light = FrLight();
	const MObject& node = Object();
	const MDagPath& dagPath = DagPath();

	if (dagPath.isValid())
	{
		MMatrix mMtx = dagPath.inclusiveMatrix();
		MFnDependencyNode depNode(node);

		if (depNode.typeId() == FireMaya::TypeId::FireRenderPhysicalLightLocator)
		{
			FireMaya::translateLight(m_light, context()->GetScope(), Context(), node, mMtx);
		}
		else if (node.hasFn(MFn::kPluginLocatorNode) || node.hasFn(MFn::kPluginTransformNode))
		{
			FireMaya::translateVrayLight(m_light, context()->GetMaterialSystem(), Context(), node, mMtx);
		}
		else
		{
			FireMaya::translateLight(m_light, context()->GetScope(), Context(), node, mMtx);
		}

		if (dagPath.isVisible())
		{
			attachToScene();
		}
	}

	for (auto* meshPtr : m_linkedMeshes)
	{
		FireRenderMesh* mesh = dynamic_cast<FireRenderMesh*>(const_cast<FireRenderMeshCommon*>(meshPtr));

		if (mesh != nullptr)
		{
			mesh->OnShaderDirty();
		}
	}
	m_linkedMeshes.clear();

	FireRenderNode::Freshen(shouldCalculateHash);
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

void FireRenderLight::addLinkedMesh(FireRenderMeshCommon const* mesh)
{
	m_linkedMeshes.emplace_back(mesh);
}

FireRenderPhysLight::FireRenderPhysLight(FireRenderContext* context, const MDagPath& dagPath) :
	FireRenderLight(context, dagPath)
{}

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

	FireMaya::Scope& scope = context()->GetScope();
	std::string lightId = getNodeUUid(Object());
	auto shaderIds = scope.GetCachedShaderIds(lightId);
	for (auto it = shaderIds.first; it != shaderIds.second; ++it)
	{
		frw::Shader linkedShader = scope.GetCachedShader(it->second);
		linkedShader.ClearLinkedLight(getLight());
	}
	if (std::distance(shaderIds.first, shaderIds.second) != 0)
	{
		scope.ClearCachedShaderIds(lightId);
	}

	if (auto scene = Scene())
	{
		detachFromSceneInternal();
	}

	m_isVisible = false;
}

void FireRenderEnvLight::attachToScene()
{
	if (m_isVisible)
		return;

	if (auto scene = Scene())
	{
		attachToSceneInternal();
		m_isVisible = true;
	}
}

void FireRenderEnvLight::attachToSceneInternal()
{
	Scene().Attach(m.light);
	m.light.SetEnvironmentOverride(frw::EnvironmentOverrideBackground, m.bgOverride);
}

void FireRenderEnvLight::detachFromSceneInternal()
{
	m.light.SetEnvironmentOverride(frw::EnvironmentOverrideBackground, frw::EnvironmentLight());
	Scene().Detach(m.light);
}

void FireRenderEnvLightHybrid::attachToSceneInternal()
{
	Scene().SetEnvironmentLight(m.light);
}

void FireRenderEnvLightHybrid::detachFromSceneInternal()
{
	Scene().SetEnvironmentLight(nullptr);
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

				ob->Freshen(ob->context()->GetRenderType() == RenderType::ViewportRender);	// make sure we have shapes to attach

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

void FireRenderEnvLight::Freshen(bool shouldCalculateHash)
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

				m.image = context()->GetScope().GetImage(filePath, colorSpace, dagPath.partialPathName());
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

	for (auto* meshPtr : m_linkedMeshes)
	{
		FireRenderMesh* mesh = dynamic_cast<FireRenderMesh*>(const_cast<FireRenderMeshCommon*>(meshPtr));

		if (mesh != nullptr)
		{
			mesh->OnShaderDirty();
		}
	}
	m_linkedMeshes.clear();

	FireRenderNode::Freshen(shouldCalculateHash);
}

void FireRenderEnvLight::addLinkedMesh(FireRenderMeshCommon const* mesh)
{
	m_linkedMeshes.emplace_back(mesh);
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

void FireRenderCamera::TranslateCameraExplicit(int viewWidth, int viewHeight)
{
	auto node = Object();
	auto dagPath = DagPath();

	MMatrix camMtx = dagPath.inclusiveMatrix();

	FireMaya::translateCamera(m_camera, node, camMtx, context()->isRenderView(),
		float(viewWidth) / float(viewHeight), true, m_type);
}

void FireRenderCamera::Freshen(bool shouldCalculateHash)
{
	auto node = Object();
	auto dagPath = DagPath();
	MFnDagNode dagNode(node);
	MFnCamera fnCamera(node);

	MString thisName = dagNode.name();

	if (!m_camera)
	{
		m_camera = Context().CreateCamera();
		Scene().SetCamera(m_camera);
	}

	if (dagPath.isValid())
	{
		int viewWidth = context()->width();
		int viewHeight = context()->height();

		TranslateCameraExplicit(viewWidth, viewHeight);

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

					MString name2 = imNode.name();

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

		if (image.IsValid())
		{
			Scene().SetBackgroundImage(image);
		}

		// Set exposure from motion blur block parameters if we didn't add fireRenderExposure attribute to the camera before
		if (context()->IsCameraSetExposureSupported() && context()->motionBlur() && fnCamera.findPlug("fireRenderExposure").isNull())
		{
			rprCameraSetExposure(m_camera.Handle(), context()->motionBlurCameraExposure());
		}

		bool cameraMotionBlur = context()->cameraMotionBlur() && context()->motionBlur();


		// We use different schemes for MotionBlur for Tahoe and NorthStar
		if (cameraMotionBlur)
		{
			float nextFrameFloats[4][4];
			FireMaya::GetMatrixForTheNextFrame(dagNode, nextFrameFloats, Instance());

			m_camera.SetMotionTransform(&nextFrameFloats[0][0], false);
		}
	}
	else
	{
		if (context()->IsCameraSetExposureSupported())
		{
			rprCameraSetExposure(m_camera.Handle(), 1.f);
		}
	}

	RegisterCallbacks();
	FireRenderNode::Freshen(shouldCalculateHash);
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

bool FireRenderCamera::isCameraTypeDefault() const
{
	FireRenderGlobals::CameraType cameraType = (FireRenderGlobals::CameraType) m_type;

	return cameraType == FireRenderGlobals::CameraType::kCameraDefault;
}

bool FireRenderCamera::isDefaultPerspective() const
{
	MFnCamera fnCamera(Object());
	return isCameraTypeDefault() && !fnCamera.isOrtho();
}

bool FireRenderCamera::isDefaultOrtho() const
{
	MFnCamera fnCamera(Object());
	return isCameraTypeDefault() && fnCamera.isOrtho();
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

				const bool isSkyMeshProcessed = ob->ReloadMesh(0);
				assert(isSkyMeshProcessed);
				ob->Freshen(ob->context()->GetRenderType() == RenderType::ViewportRender);	// make sure we have shapes to attach

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

void FireRenderSky::Freshen(bool shouldCalculateHash)
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

	FireRenderNode::Freshen(shouldCalculateHash);
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
		detachFromSceneInternal();
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
		attachToSceneInternal();

		if (m_sunLight.Handle()) // m_sunLight could be empty if IBL is used for lighting
			scene.Attach(m_sunLight);

		m_isVisible = true;
	}
}

void FireRenderSky::detachFromSceneInternal()
{
	Scene().Detach(m_envLight);
}

void FireRenderSky::attachToSceneInternal()
{
	Scene().Attach(m_envLight);
}

void FireRenderSkyHybrid::attachToSceneInternal()
{
	Scene().SetEnvironmentLight(m_envLight);
}

void FireRenderSkyHybrid::detachFromSceneInternal()
{
	Scene().SetEnvironmentLight(nullptr);
}

FireRenderCustomEmitter::FireRenderCustomEmitter(FireRenderContext* context, const MDagPath& dagPath) :
	FireRenderLight(context, dagPath)
{

}

void FireRenderCustomEmitter::Freshen(bool shouldCalculateHash)
{
	if (!m_light.light)
	{
		m_light.light = Context().CreateSpotLight();
		Scene().Attach(m_light.light);

		const char* lightName = MFnDependencyNode(DagPath().transform()).name().asChar();
		m_light.light.SetName(lightName);
		
		m_light.light.AddGLTFExtraIntAttribute("isEmitter", 1);
	}
}
