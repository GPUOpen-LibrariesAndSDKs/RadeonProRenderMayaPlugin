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
#include "FireRenderIBL.h"
#include <maya/MFnDependencyNode.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnMessageAttribute.h>
#include <maya/MEventMessage.h>
#include <maya/MImage.h>
#include <maya/MFileObject.h>
#include <maya/MFnStringArrayData.h>
#include "base_mesh.h"
#include "FireRenderUtils.h"
#include <maya/MPxNode.h>
#include <maya/MDagModifier.h>
#include <maya/MFnTransform.h>

#include "FireMaya.h"

using namespace FireMaya;

MObject FireRenderIBL::aFilePath;
MObject	FireRenderIBL::aIntensity;
MObject	FireRenderIBL::aDisplay;
MObject	FireRenderIBL::aFlipIBL;
MObject FireRenderIBL::aColor;

MTypeId FireRenderIBL::id(FireMaya::TypeId::FireRenderIBL);
MString FireRenderIBL::drawDbClassification("drawdb/geometry/light/FireRenderIBL:drawdb/light/directionalLight:light");
MString FireRenderIBL::drawDbGeomClassification("drawdb/geometry/light/FireRenderIBL");
MString FireRenderIBL::drawRegistrantId("FireRenderIBLNode");

FireRenderIBL::FireRenderIBL():
	mTexturePath(""),
	mTexture(NULL),
	m_selectionChangedCallback(0),
	m_attributeChangedCallback(0)
{
}

FireRenderIBL::~FireRenderIBL()
{
	if (mTexture)
	{
		auto theRenderer = MHWRender::MRenderer::theRenderer();
		if (theRenderer)
		{
			auto textureManager = theRenderer->getTextureManager();
			if (textureManager)
				textureManager->releaseTexture(mTexture);
		}
	}
}

const MString FireRenderIBL::GetNodeTypeName(void) const
{
	return "RPRIBL";
}

MStatus FireRenderIBL::compute(const MPlug& plug, MDataBlock& data)
{
	return MS::kUnknownParameter;
}

MStatus FireRenderIBL::initialize()
{
	MFnNumericAttribute nAttr;
	MFnTypedAttribute tAttr;
	MFnMessageAttribute mAttr;

	MStatus errorCode;

	aFilePath = tAttr.create("filePath", "f", MFnData::kString);
	tAttr.setStorable(true);
	tAttr.setReadable(true);
	tAttr.setWritable(true);
	tAttr.setUsedAsFilename(true);

	aIntensity = nAttr.create("intensity", "i", MFnNumericData::kFloat, 1.0);
	nAttr.setKeyable(true);
	nAttr.setStorable(true);
	nAttr.setReadable(true);
	nAttr.setWritable(true);
	nAttr.setSoftMin(0);
	nAttr.setSoftMax(10);

	aDisplay = nAttr.create("display", "d", MFnNumericData::kBoolean, 1);
	nAttr.setKeyable(true);
	nAttr.setStorable(true);
	nAttr.setReadable(true);
	nAttr.setWritable(true);

	// Create attribute inside current node and connect it to RadeonProRenderGlobals.flipIBL attribute to
	// have an automatical update
	aFlipIBL = nAttr.create("flipIBL", "fibl", MFnNumericData::kBoolean, 0);
	nAttr.setStorable(true);
	nAttr.setReadable(true);
	nAttr.setWritable(true);

	aColor = nAttr.createColor("color", "fc");
	nAttr.setDefault(1.0f, 1.0f, 1.0f);
	nAttr.setKeyable(true);
	nAttr.setStorable(true);
	nAttr.setReadable(true);
	nAttr.setWritable(true);

	MStatus status = addAttribute(aFilePath);
	assert(status == MStatus::kSuccess);

	status = addAttribute(aIntensity);
	assert(status == MStatus::kSuccess);

	status = addAttribute(aDisplay);
	assert(status == MStatus::kSuccess);

	status = addAttribute(aFlipIBL);
	assert(status == MStatus::kSuccess);

	status = addAttribute(aColor);
	assert(status == MStatus::kSuccess);

	return MS::kSuccess;
}

void FireRenderIBL::onAttributeChanged(MNodeMessage::AttributeMessage msg, MPlug &plug, MPlug &otherPlug, void *clientData)
{
	if (!(msg | MNodeMessage::AttributeMessage::kAttributeSet))
	{
		return;
	}
}

void FireRenderIBL::postConstructor()
{
	FireRenderLightCommon::postConstructor();

	MStatus status;
	MObject mobj = thisMObject();
	m_attributeChangedCallback = MNodeMessage::addAttributeChangedCallback(mobj, FireRenderIBL::onAttributeChanged, this, &status);
	assert(status == MStatus::kSuccess);

	// rename node
	MFnDependencyNode nodeFn(thisMObject());
	nodeFn.setName("RPRIBLShape");

	MFnDagNode dagNode(thisMObject());
	MObject parent = dagNode.parent(0, &status);
	CHECK_MSTATUS(status);

	MFnDependencyNode parentFn(parent);
	parentFn.setName("RPRIBL");
}

const char* frIblVS =
R"(#version 110
void main()
{
	gl_Position = ftransform(); //Transform the vertex position
	gl_TexCoord[0] = gl_MultiTexCoord0;
	gl_TexCoord[0].t = 1.0 - gl_TexCoord[0].t;
	gl_TexCoord[0].s = 1.0 - (gl_TexCoord[0].s + 0.3);
};
)";

const char* frIblVSFlipped =
R"(#version 110
void main()
{
	gl_Position = ftransform(); //Transform the vertex position
	gl_TexCoord[0] = gl_MultiTexCoord0;
	gl_TexCoord[0].t = 1.0 - gl_TexCoord[0].t;
	gl_TexCoord[0].s = gl_TexCoord[0].s + 0.3;
};
)";


const char *frIblFS =
R"(#version 110
uniform sampler2D texture1;
uniform float intensity;
void main(void)
{
	vec4 color = texture2D(texture1, gl_TexCoord[0].st) * intensity;
	//linear to srgb
	//color[0] = pow(color[0], 1.0 / 2.2);
	//color[1] = pow(color[1], 1.0 / 2.2);
	//color[2] = pow(color[2], 1.0 / 2.2);
	//color[3] = pow(color[3], 1.0 / 2.2);
	gl_FragColor = color;
};
)";


void FireRenderIBL::draw(
	M3dView & view,
	const MDagPath & /*path*/,
	M3dView::DisplayStyle style,
	M3dView::DisplayStatus status)
{
	// Get the size
	MObject thisNode = thisMObject();
	MPlug plug(thisNode, aFilePath);
	MString filePath;
	plug.getValue(filePath);

	MPlug intensityPlug(thisNode, aIntensity);
	float intensity = 1.0f;
	intensityPlug.getValue(intensity);

	MPlug displayPlug(thisNode, aDisplay);
	bool display = true;
	displayPlug.getValue(display);

	view.beginGL();

	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);

	if ((style == M3dView::kWireFrame) || (!display))
	{
		glEnableClientState(GL_VERTEX_ARRAY);
		glVertexPointer(3, GL_FLOAT, 0, lowSpherePoints);

		glDrawElements(GL_LINES, 3120, GL_UNSIGNED_INT, lowSphereWireConnect);
		glDisableClientState(GL_VERTEX_ARRAY);
	}
	else
	{
		if (!mShader)
		{
			mShader = std::unique_ptr<Shader>(new Shader());

			if (!IsFlipIBL())
			{
				mShader->init(frIblVS, frIblFS, "");
			}
			else
			{
				mShader->init(frIblVSFlipped, frIblFS, "");
			}
		}

		auto theRenderer = MHWRender::MRenderer::theRenderer();
		auto textureManager = theRenderer->getTextureManager();
		if (filePath != mTexturePath)
		{
			textureManager->releaseTexture(mTexture);
			mTexture = NULL;
			mTexturePath = filePath;
			if ((mTexturePath != "") && !mTexture)
			{
				mTexture = textureManager->acquireTexture(mTexturePath, name());
			}
		}

		if (mTexture)
		{
			glEnableClientState(GL_VERTEX_ARRAY);
			glVertexPointer(3, GL_FLOAT, 0, spherePoints);
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			glTexCoordPointer(2, GL_FLOAT, 0, sphereUVs);

			glEnable(GL_TEXTURE_2D);
			GLuint texId = *(GLuint*)mTexture->resourceHandle();
			glBindTexture(GL_TEXTURE_2D, texId);


			if (mShader)
			{
				mShader->bind();
				int texloc = glGetUniformLocation(mShader->id(), "texture1");
				glUniform1i(texloc, 0);

				int loc = glGetUniformLocation(mShader->id(), "intensity");
				glUniform1f(loc, intensity);
			}

			glDrawElements(GL_TRIANGLES, sphereFaces * 3, GL_UNSIGNED_INT, sphereIndices);

			if (mShader)
			{
				mShader->unbind();
			}

			glDisableClientState(GL_VERTEX_ARRAY);
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		}
		else
		{
			glEnableClientState(GL_VERTEX_ARRAY);
			glVertexPointer(3, GL_FLOAT, 0, lowSpherePoints);

			glDrawElements(GL_LINES, 3120, GL_UNSIGNED_INT, lowSphereWireConnect);
			glDisableClientState(GL_VERTEX_ARRAY);
		}
	}
	glPopClientAttrib();
	glPopAttrib();

	view.endGL();

}
bool FireRenderIBL::isBounded() const
{
	return true;
}
MBoundingBox FireRenderIBL::boundingBox() const
{
	MObject thisNode = thisMObject();
	MPoint corner1(-1.0, -1.0, -1.0);
	MPoint corner2(1.0, 1.0, 1.0);
	return MBoundingBox(corner1, corner2);
}
void* FireRenderIBL::creator()
{
	return new FireRenderIBL();
}


// ================================
// Viewport 2.0 override
// ================================

FireRenderIBLOverride::FireRenderIBLOverride(const MObject& obj)
	: MHWRender::MPxGeometryOverride(obj)
	, mLocatorNode(obj)
	, mFilePath("")
	, mIntensity(1.0f)
	, mDisplay(true)
	, mChanged(true)
	, mFileExists(false)
	, mFilePathChanged(false)
{
}

FireRenderIBLOverride::~FireRenderIBLOverride()
{
}

MHWRender::DrawAPI FireRenderIBLOverride::supportedDrawAPIs() const
{

#ifndef MAYA2015
	return (MHWRender::kOpenGL | MHWRender::kDirectX11 | MHWRender::kOpenGLCoreProfile);
#else
	return (MHWRender::kOpenGL | MHWRender::kDirectX11);
#endif
}

const MString colorParameterName_ = "solidColor";
const MString wireframeSphereItemName_ = "sphereWires";
const MString shadedSphereItemName_ = "sphereShaded";

void FireRenderIBLOverride::updateDG()
{
	MPlug plug(mLocatorNode, FireRenderIBL::aFilePath);
	MString filePath;
	plug.getValue(filePath);

	if (filePath != mFilePath)
	{
		mFilePath = filePath;
		MFileObject fileObj;
		fileObj.setRawFullName(mFilePath);
		mFileExists = fileObj.exists();
		mFilePathChanged = true;
	}

	if (mFilePath == "")
	{
		mFileExists = false;
	}

	MPlug intensityPlug(mLocatorNode, FireRenderIBL::aIntensity);
	intensityPlug.getValue(mIntensity);

	MPlug displayPlug(mLocatorNode, FireRenderIBL::aDisplay);
	displayPlug.getValue(mDisplay);
}

void FireRenderIBLOverride::updateRenderItems(const MDagPath& path, MHWRender::MRenderItemList& list)
{
	MHWRender::MRenderItem* wireframeSphere = nullptr;
	MHWRender::MRenderItem* shadedSphere = nullptr;

	int index = list.indexOf(wireframeSphereItemName_);

	if (index < 0)
	{
		wireframeSphere = MHWRender::MRenderItem::Create(
			wireframeSphereItemName_,
			MHWRender::MRenderItem::DecorationItem,
			MHWRender::MGeometry::kLines);
		wireframeSphere->setDrawMode(MHWRender::MGeometry::kAll);

		list.append(wireframeSphere);

		auto renderer = MHWRender::MRenderer::theRenderer();
		auto shaderManager = renderer->getShaderManager();
		auto shader = shaderManager->getStockShader(MHWRender::MShaderManager::k3dSolidShader);
		if (shader)
		{
			static const float theColor[] = { 1.0f, 1.0f, 0.0f, 1.0f };
			shader->setParameter("solidColor", theColor);
			wireframeSphere->setShader(shader);
			shaderManager->releaseShader(shader);
		}
	}
	else
	{
		wireframeSphere = list.itemAt(index);
	}

	if (wireframeSphere)
	{
		wireframeSphere->enable(true);
	}

	// shaded sphere
	index = list.indexOf(shadedSphereItemName_);

	if (index < 0)
	{
		shadedSphere = MHWRender::MRenderItem::Create(
			shadedSphereItemName_,
			MHWRender::MRenderItem::DecorationItem,
			MHWRender::MGeometry::kTriangles);
		shadedSphere->setDrawMode(
			(MHWRender::MGeometry::DrawMode)(MHWRender::MGeometry::kShaded | MHWRender::MGeometry::kTextured));
		list.append(shadedSphere);

		auto renderer = MHWRender::MRenderer::theRenderer();
		auto shaderManager = renderer->getShaderManager();
		auto shader = shaderManager->getStockShader(MHWRender::MShaderManager::k3dSolidTextureShader);

		if (shader)
		{
			auto textureManager = renderer->getTextureManager();
			MHWRender::MTextureAssignment texResource;
			texResource.texture = textureManager->acquireTexture(mFilePath, path.partialPathName());
			shader->setParameter("map", texResource);
			float colorGain[3] = { mIntensity, mIntensity, mIntensity };
			shader->setParameter("colorGain", &colorGain[0]);
			shadedSphere->setShader(shader);
			textureManager->releaseTexture(texResource.texture);
			shaderManager->releaseShader(shader);
		}
	}
	else
	{
		shadedSphere = list.itemAt(index);
	}

	if (shadedSphere)
	{
		// Updating UV coordinates of texture (may be affected by "flipIBL" parameter)
		float flipCoeff = IsFlipIBL() ? 1.0f : -1.0f;
		MHWRender::MShaderInstance* shader = shadedSphere->getShader();

		float uvscale[2] = { flipCoeff * 1.0f, -1.0f };
		float uvoffset[2] = { flipCoeff * 0.3f, 0.0f };
		MStatus status = shader->setParameter("UVScale", &uvscale[0]);
		status = shader->setParameter("UVOffset", &uvoffset[0]);

		shadedSphere->enable(mDisplay && mFileExists);
		if (wireframeSphere)
		{
			if (mDisplay && mFileExists)
			{
				wireframeSphere->setDrawMode(MHWRender::MGeometry::kWireframe);
				auto shader = shadedSphere->getShader();
				if (shader)
				{
					if (mFilePathChanged)
					{
						try
						{
							auto renderer = MHWRender::MRenderer::theRenderer();
							auto textureManager = renderer->getTextureManager();
							MHWRender::MTextureAssignment texResource;
							texResource.texture = textureManager->acquireTexture(mFilePath, path.partialPathName());
							shader->setParameter("map", texResource);
							textureManager->releaseTexture(texResource.texture);
						}
						catch (...)
						{
							MString errorMsg;
							errorMsg.format("Failed to acquire texture: \"^1s\"", mFilePath);
							MGlobal::displayError(errorMsg);
						}
						mFilePathChanged = false;
					}
					float colorGain[3] = { mIntensity, mIntensity, mIntensity };
					shader->setParameter("colorGain", &colorGain[0]);
				}
			}
			else
			{
				wireframeSphere->setDrawMode(MHWRender::MGeometry::kAll);
			}
		}
	}
}

void FireRenderIBLOverride::populateGeometry(
	const MHWRender::MGeometryRequirements& requirements,
	const MHWRender::MRenderItemList& renderItems,
	MHWRender::MGeometry& data)
{
	MHWRender::MVertexBuffer* verticesBuffer = NULL;
	MHWRender::MVertexBuffer* textureBuffer = NULL;

	float* vertices = NULL;
	float* uvs = NULL;

	const MHWRender::MVertexBufferDescriptorList& vertexBufferDescriptorList = requirements.vertexRequirements();
	const int numberOfVertexRequirments = vertexBufferDescriptorList.length();

	MHWRender::MVertexBufferDescriptor vertexBufferDescriptor;
	for (int requirmentNumber = 0; requirmentNumber < numberOfVertexRequirments; ++requirmentNumber)
	{
		if (!vertexBufferDescriptorList.getDescriptor(requirmentNumber, vertexBufferDescriptor))
		{
			continue;
		}

		switch (vertexBufferDescriptor.semantic())
		{
		case MHWRender::MGeometry::kPosition:
		{
			if (!verticesBuffer)
			{
				verticesBuffer = data.createVertexBuffer(vertexBufferDescriptor);
				if (verticesBuffer)
				{
					vertices = (float*)verticesBuffer->acquire(6840 + 1146);
				}
			}
			break;
		}
		case MHWRender::MGeometry::kTexture:
		{
			if (!textureBuffer)
			{
				textureBuffer = data.createVertexBuffer(vertexBufferDescriptor);
				if (textureBuffer)
				{
					uvs = (float*)textureBuffer->acquire(4560);
				}
			}
			break;
		}
		default:
			break;
		}
	}


	if (verticesBuffer && vertices)
	{
		memcpy(&vertices[0], &spherePoints[0], sizeof(float) * 6840);
		memcpy(&vertices[6840], &lowSpherePoints[0], sizeof(float) * 1146);
		verticesBuffer->commit(vertices);
	}

	if (textureBuffer && uvs)
	{
		memcpy(&uvs[0], &sphereUVs[0], sizeof(float) * 4560);
		textureBuffer->commit(uvs);
	}

	for (int i = 0; i < renderItems.length(); ++i)
	{
		const MHWRender::MRenderItem* item = renderItems.itemAt(i);

		if (!item)
		{
			continue;
		}

		int startIndex = 0;
		int numIndex = 0;

		if (item->name() == wireframeSphereItemName_)
		{
			startIndex = 0;
			numIndex = 3120;
		}

		if (item->name() == shadedSphereItemName_)
		{
			startIndex = 0;
			numIndex = 2280;
		}

		if (numIndex)
		{
			MHWRender::MIndexBuffer*  indexBuffer = data.createIndexBuffer(MHWRender::MGeometry::kUnsignedInt32);
			unsigned int* indices = (unsigned int*)indexBuffer->acquire(numIndex);

			if (indices)
			{
				if (item->name() == wireframeSphereItemName_)
				{
					for (int idx = 0; idx < numIndex; idx++)
					{
						indices[idx] = lowSphereWireConnect[idx] + 2280;
					}
				}
				else
				{
					memcpy(&indices[0], &sphereIndices[0], sizeof(unsigned int) * numIndex);
				}
				indexBuffer->commit(indices);
			}
			item->associateWithIndexBuffer(indexBuffer);
		}
	}
	mChanged = false;
}
#ifndef MAYA2015
bool
FireRenderIBLOverride::refineSelectionPath(
	const MHWRender::MSelectionInfo &selectInfo,
	const MHWRender::MRenderItem &hitItem,
	MDagPath &path,
	MObject &components,
	MSelectionMask &objectMask)
{
	return true;
}

void FireRenderIBLOverride::updateSelectionGranularity(
	const MDagPath& path,
	MHWRender::MSelectionContext& selectionContext)
{

}
#endif
