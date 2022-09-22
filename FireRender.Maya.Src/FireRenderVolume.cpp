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
#include "Volumes/VolumeAttributes.h"
#include "FastNoise.h"

#include <float.h>
#include <array>
#include <algorithm>
#include <vector>
#include <iterator>
#include <stdint.h>

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
#include <maya/MRampAttribute.h> 
#include <maya/MPointArray.h>
#include <maya/MFnNurbsCurve.h>
#include <istream>
#include <ostream>
#include <sstream>

#include <maya/MFnFluid.h>

//===================
// Common Volume
//===================
FireRenderCommonVolume::FireRenderCommonVolume(FireRenderContext* context, const MDagPath& dagPath)
	: FireRenderNode(context, dagPath)
{}

FireRenderCommonVolume::~FireRenderCommonVolume()
{
	clear();
}

void FireRenderCommonVolume::ApplyTransform(void)
{
	if (!m_volume.IsValid())
		return;

	auto node = Object();
	MFnDagNode fnDagNode(node);
	MDagPath path = MDagPath::getAPathTo(node);

	// get transform
	MMatrix matrix = path.inclusiveMatrix();
	matrix = m_matrix * matrix;

	// convert Maya mesh in cm to m
	MMatrix scaleM;
	scaleM.setToIdentity();
	scaleM[0][0] = scaleM[1][1] = scaleM[2][2] = GetSceneUnitsConversionCoefficient();

	// apply bbox grid size (by scale matrix)
	matrix = m_bboxScale * matrix * scaleM;

	float mfloats[4][4];
	matrix.get(mfloats);

	// apply transform to volume
	m_volume.SetTransform(*mfloats, false);

	// apply transform to bbox
	m_boundingBoxMesh.SetTransform(*mfloats, false);
}

void FireRenderCommonVolume::Freshen(bool shouldCalculateHash)
{
	detachFromScene();

	clear();

	auto node = Object();
	MFnDagNode fnDagNode(node);
	MString name = fnDagNode.name();

	bool haveVolume = TranslateVolume();

	if (haveVolume)
	{
		ApplyTransform();

		MDagPath path = MDagPath::getAPathTo(node);
		if (path.isVisible())
			attachToScene();
	}

	FireRenderNode::Freshen(shouldCalculateHash);
}

void FireRenderCommonVolume::clear()
{
	m_volume.Reset();
	m_densityGrid.Reset();
	m_albedoGrid.Reset();
	m_emissionGrid.Reset();

	FireRenderObject::clear();
}

void FireRenderCommonVolume::attachToScene()
{
	if (m_isVisible)
		return;

	frw::Scene scene = context()->GetScene();
	if (!scene.IsValid())
		return;

	scene.Attach(m_volume);

	scene.Attach(m_boundingBoxMesh);

	m_volume.AttachToShape(m_boundingBoxMesh);

	// create material RPR_MATERIAL_NODE_TRANSPARENT fake material for fake mesh
	frw::Shader fakeShader = Scope().GetCachedShader(std::string("FakeShaderForVolume"));
	if (!fakeShader)
	{
		fakeShader = frw::Shader(context()->GetMaterialSystem(), frw::ShaderType::ShaderTypeTransparent);
		Scope().SetCachedShader(std::string("FakeShaderForVolume"), fakeShader);
	}

	// attach material to shape
	m_boundingBoxMesh.SetShader(fakeShader);

	m_isVisible = true;
}

void FireRenderCommonVolume::detachFromScene()
{
	if (!m_isVisible)
		return;

	if (frw::Scene scene = context()->GetScene())
	{
		scene.Detach(m_volume);

		m_isVisible = false;
	}
}

bool FireRenderCommonVolume::SetBBox(double Xdim, double Ydim, double Zdim) 
{
	// scale is applied when tranformation matrix is applied to volume!

	// proceed with creating mesh
	std::vector<float> veritces;
	std::vector<float> normals;
	std::vector<int> vertexIndices;
	std::vector<int> normalIndices;
	CreateBoxGeometry(veritces, normals, vertexIndices, normalIndices);

	// create fake shape for volume to be hooked with
	m_boundingBoxMesh = Context().CreateMeshEx(
		veritces.data(), veritces.size() / 3, sizeof(Float3),
		normals.data(), normals.size() / 3, sizeof(Float3),
		nullptr, 0, 0,
		0, /*puvCoords.data()*/ nullptr, 0 /*sizeCoords.data()*/, nullptr /*multiUV_texcoord_strides.data()*/,
		vertexIndices.data(), sizeof(rpr_int),
		normalIndices.data(), sizeof(rpr_int),
		nullptr /*puvIndices.data()*/, nullptr /*texIndexStride.data()*/,
		std::vector<int>(vertexIndices.size() / 3, 3).data(), vertexIndices.size() / 3);

	return true;
}

void FireRenderCommonVolume::OnShaderDirty()
{
	setDirty();
}

static void VolumeDirtyCallback(MObject& node, void* clientData)
{
	DebugPrint("CALLBACK > VolumeDirtyCallback(%s)", node.apiTypeStr());
	if (auto self = static_cast<FireRenderCommonVolume*>(clientData))
	{
		assert(node != self->Object());
		self->OnShaderDirty();
	}
}

void FireRenderCommonVolume::RegisterCallbacks()
{
	FireRenderNode::RegisterCallbacks();

	if (!m_volume.IsValid())
		return;

	// add callback
	MObject node = Object();
	MStatus status;
	MDagPath path = MDagPath::getAPathTo(node, &status);

	// get hair shader node
	MObjectArray shdrs = getConnectedShaders(path);
	if (shdrs.length() == 0)
		return;

	AddCallback(MNodeMessage::addNodeDirtyCallback(node, VolumeDirtyCallback, this, &status));
}

//===================
// RPR Volume
//===================
FireRenderRPRVolume::FireRenderRPRVolume(FireRenderContext* context, const MDagPath& dagPath)
	: FireRenderCommonVolume(context, dagPath)
{}

FireRenderRPRVolume::~FireRenderRPRVolume()
{
	clear();
}

bool FireRenderRPRVolume::TranslateVolume()
{
	// setup
	const MObject& node = Object();

	// get volume data
	VDBVolumeData vdata;
	RPRVolumeAttributes::FillVolumeData(vdata, node);
	
	if (!vdata.IsValid())
		return false;

	// create fake mesh for volume
	if (!SetBBox(1.0f, 1.0f, 1.0f)) // scale is applied when tranformation matrix is applied to volume
	{
		return false;
	}

	// create rpr volumes
	m_densityGrid.Reset();
	m_albedoGrid.Reset();
	m_emissionGrid.Reset();

	if (vdata.densityGrid.IsValid()) // grid exists
	{
		m_densityGrid = Context().CreateVolumeGrid(
			vdata.densityGrid.size.gridSizeX,
			vdata.densityGrid.size.gridSizeY,
			vdata.densityGrid.size.gridSizeZ,
			vdata.densityGrid.gridOnIndices,
			vdata.densityGrid.gridOnValueIndices,
			RPR_GRID_INDICES_TOPOLOGY_XYZ_U32
		);

		// scale
		m_bboxScale.setToIdentity();
		m_bboxScale[0][0] = vdata.densityGrid.size.gridSizeX * vdata.densityGrid.size.voxelSizeX;
		m_bboxScale[1][1] = vdata.densityGrid.size.gridSizeY * vdata.densityGrid.size.voxelSizeY;
		m_bboxScale[2][2] = vdata.densityGrid.size.gridSizeZ * vdata.densityGrid.size.voxelSizeZ;
	}

	if (vdata.albedoGrid.IsValid()) // grid exists
	{
		m_albedoGrid = Context().CreateVolumeGrid(
			vdata.albedoGrid.size.gridSizeX,
			vdata.albedoGrid.size.gridSizeY,
			vdata.albedoGrid.size.gridSizeZ,
			vdata.albedoGrid.gridOnIndices,
			vdata.albedoGrid.gridOnValueIndices,
			RPR_GRID_INDICES_TOPOLOGY_XYZ_U32
		);
	}

	if (vdata.emissionGrid.IsValid()) // grid exists
	{
		m_emissionGrid = Context().CreateVolumeGrid(
			vdata.emissionGrid.size.gridSizeX,
			vdata.emissionGrid.size.gridSizeY,
			vdata.emissionGrid.size.gridSizeZ,
			vdata.emissionGrid.gridOnIndices,
			vdata.emissionGrid.gridOnValueIndices,
			RPR_GRID_INDICES_TOPOLOGY_XYZ_U32
		);
	}

	m_volume = Context().CreateVolume(
		m_densityGrid.Handle(),
		m_albedoGrid.Handle(),
		m_emissionGrid.Handle(),
		vdata.densityGrid.valuesLookUpTable,
		vdata.albedoGrid.valuesLookUpTable,
		vdata.emissionGrid.valuesLookUpTable
	);

	return m_volume.IsValid();
}

//==================
// Northstar RPR volume
//==================

NorthstarRPRVolume::NorthstarRPRVolume(FireRenderContext* context, const MDagPath& dagPath)
	: FireRenderRPRVolume(context, dagPath)
{}

NorthstarRPRVolume::~NorthstarRPRVolume()
{
	clear();
}

void NorthstarRPRVolume::attachToScene()
{
	if (m_isVisible)
		return;

	frw::Scene scene = context()->GetScene();
	if (!scene.IsValid())
		return;

	scene.Attach(m_boundingBoxMesh);

	m_isVisible = true;
}

void NorthstarRPRVolume::detachFromScene()
{
	if (!m_isVisible)
		return;

	if (frw::Scene scene = context()->GetScene())
	{
		scene.Detach(m_boundingBoxMesh);

		m_isVisible = false;
	}
}

void NorthstarRPRVolume::ApplyTransform(void)
{
	if (!m_boundingBoxMesh.IsValid())
		return;

	auto node = Object();
	MFnDagNode fnDagNode(node);
	MDagPath path = MDagPath::getAPathTo(node);

	// get transform
	MMatrix matrix = path.inclusiveMatrix();
	matrix = m_matrix * matrix;

	// convert Maya mesh in cm to m
	MMatrix scaleM;
	scaleM.setToIdentity();
	scaleM[0][0] = scaleM[1][1] = scaleM[2][2] = GetSceneUnitsConversionCoefficient();

	// apply bbox grid size (by scale matrix)
	matrix = m_bboxScale * matrix * scaleM;

	float mfloats[4][4];
	matrix.get(mfloats);

	// apply transform to fake mesh
	m_boundingBoxMesh.SetTransform(*mfloats, false);
}

// we use a texture as a lookup table
frw::ImageNode CreateLookupTextureNode(FireRenderCommonVolume *object, std::vector<float> lookupValues, frw::GridNode grid)
{
	rpr_image_desc lookupImageDesc;
	lookupImageDesc.image_width = lookupValues.size() / 3;
	lookupImageDesc.image_height = 1;
	lookupImageDesc.image_depth = 0;
	lookupImageDesc.image_row_pitch = lookupImageDesc.image_width * sizeof(float) * 3;
	lookupImageDesc.image_slice_pitch = 0;
	auto lookupImage = frw::Image(object->Context(), { 3, RPR_COMPONENT_TYPE_FLOAT32 },
		lookupImageDesc, lookupValues.data());

	auto lookupNode = frw::ImageNode(object->context()->GetMaterialSystem());
	lookupNode.SetMap(lookupImage);
	lookupNode.SetValue(RPR_MATERIAL_INPUT_UV, grid);
	lookupNode.SetValueInt(RPR_MATERIAL_INPUT_WRAP_U, RPR_IMAGE_WRAP_TYPE_CLAMP_TO_EDGE);
	lookupNode.SetValueInt(RPR_MATERIAL_INPUT_WRAP_V, RPR_IMAGE_WRAP_TYPE_CLAMP_TO_EDGE);

	return lookupNode;
}

void generateBitmapImage(unsigned char* image, int height, int width, int pitch, const char* imageFileName);

void NorthstarRPRVolume_DebugDumpLookupFloat3(std::vector<float>& dataToDump, const std::string& pathToFile, const std::string& caption)
{
	unsigned int height = 100;
	unsigned int width = dataToDump.size() / 3;

	std::vector<unsigned char> buffer2;
	buffer2.reserve(height * width);

	for (unsigned int y = 0; y < height; y++)
	{
		for (unsigned int x = 0; x < dataToDump.size(); x+=3)
		{
			char r = (char)(255 * dataToDump[x+0]);
			char g = (char)(255 * dataToDump[x+1]);
			char b = (char)(255 * dataToDump[x+2]);

			buffer2.push_back(b);
			buffer2.push_back(g);
			buffer2.push_back(r);
			buffer2.push_back(255);
		}
	}

	static int debugDumpIdx = 0;
	std::string dumpAddr = pathToFile + caption + std::to_string(debugDumpIdx++) + ".bmp";
	unsigned char* dst2 = buffer2.data();
	generateBitmapImage(dst2, height, width, width * 4, dumpAddr.c_str());
}

void NorthstarRPRVolume_DebugDumpGrid(
	size_t gridSizeX, 
	size_t gridSizeY, 
	size_t gridSizeZ, 
	std::vector<uint32_t>& gridOnIndices,
	std::vector<float>& dataToDump, 
	std::vector<float>* plookupTable,
	const std::string& pathToFile,
	const std::string& caption)
{
	static int debugDumpIdx = 0;

	for (size_t Z = 0; Z < gridSizeZ; ++Z) // slices by z
	{
		std::vector<std::array<unsigned char, 4>> buffer4;

		buffer4.resize(gridSizeX * gridSizeZ, { 0, 255, 0, 255} );

		for (size_t idx = 0; idx < gridOnIndices.size()/3; idx++)
		{
			uint32_t z = gridOnIndices[idx*3 + 2];

			if (z != Z)
				continue;

			uint32_t x = gridOnIndices[idx*3];
			uint32_t y = gridOnIndices[idx*3 + 1];

			std::array<unsigned char, 4>& cell = buffer4[x + y * gridSizeX];
			if (plookupTable == nullptr)
			{
				cell[0] = (char)(255 * dataToDump[idx]);
				cell[1] = (char)(255 * dataToDump[idx]);
				cell[2] = (char)(255 * dataToDump[idx]);
			}
			else
			{
				float gridVal = dataToDump[idx];

				// find 2 closest ctrl points
				size_t countCtrlPoints = plookupTable->size() / 3;
				float step = 1.0f / countCtrlPoints;
				size_t leftSideCtrlPoint = round(gridVal/step);
				size_t rightSideCtrlPoint = leftSideCtrlPoint;
				if (leftSideCtrlPoint < countCtrlPoints - 1)
				{
					rightSideCtrlPoint = leftSideCtrlPoint + 1;
				}

				// interpolate color values from 2 ctrl points
				std::vector<float>& lookupTable = *plookupTable;
				float r = (lookupTable[leftSideCtrlPoint*3] + lookupTable[rightSideCtrlPoint*3])/2;
				float g = (lookupTable[leftSideCtrlPoint*3 + 1] + lookupTable[rightSideCtrlPoint*3 + 1])/2;
				float b = (lookupTable[leftSideCtrlPoint*3 + 2] + lookupTable[rightSideCtrlPoint*3 + 2])/2;

				cell[2] = (char)(255 * r);
				cell[1] = (char)(255 * g);
				cell[0] = (char)(255 * b);
			}

			cell[3] = 255;
		}

		std::string dumpAddr = pathToFile + caption + std::to_string(Z) + "_" + std::to_string(debugDumpIdx) + ".bmp";
		unsigned char* dst4 = reinterpret_cast<unsigned char*>(buffer4.data());
		generateBitmapImage(dst4, gridSizeY, gridSizeX, gridSizeX * 4, dumpAddr.c_str());
	}
	debugDumpIdx++;
}

bool NorthstarRPRVolume::TranslateVolume()
{
	// setup
	const MObject& node = Object();
	MFnDependencyNode depNode(node);

	// get volume data
	VDBVolumeData vdata;
	RPRVolumeAttributes::FillVolumeData(vdata, node);

	if (!vdata.IsValid())
		return false;

	//create void mesh for volume
	m_boundingBoxMesh = Context().CreateVoidMesh();
	if (!m_boundingBoxMesh.IsValid()) {
		return false;
	}

	//create main volume shader
	auto volumeShader = frw::Shader(context()->GetMaterialSystem(), frw::ShaderType::ShaderTypeVolume);	

	// create grids
	m_densityGrid.Reset();
	m_albedoGrid.Reset();
	m_emissionGrid.Reset();

	if (vdata.densityGrid.IsValid()) // grid exists
	{
		// normalize density grid
		float maxDensityGridValue = *std::max_element(
			vdata.densityGrid.gridOnValueIndices.begin(), vdata.densityGrid.gridOnValueIndices.end());
		if (maxDensityGridValue > 1.0f)
		{
			for (size_t idx = 0; idx < vdata.densityGrid.gridOnValueIndices.size(); idx++)
			{
				vdata.densityGrid.gridOnValueIndices[idx] = vdata.densityGrid.gridOnValueIndices[idx] / maxDensityGridValue;
			}
		}

		// compute grid values because density lookup is not implemented in Northstar
		std::vector<float>& lookupTable = vdata.densityGrid.valuesLookUpTable;
		size_t countCtrlPoints = lookupTable.size() / 3;
		float step = 1.0f / ((countCtrlPoints > 1) ? (countCtrlPoints - 1) : 1);
		for (float& gridVal : vdata.densityGrid.gridOnValueIndices)
		{
			// - find 2 closest ctrl points
			volatile size_t leftSideCtrlPoint = floor(gridVal / step);

			volatile size_t rightSideCtrlPoint = leftSideCtrlPoint;
			if (leftSideCtrlPoint < countCtrlPoints - 1)
			{
				rightSideCtrlPoint = leftSideCtrlPoint + 1;
			}

			// - interpolate color values from 2 ctrl points
			gridVal = gridVal * (lookupTable[leftSideCtrlPoint * 3] + lookupTable[rightSideCtrlPoint * 3]) / 2.0f;
		}

		// proceed with grid creation
		m_densityGrid = Context().CreateVolumeGrid(
			vdata.densityGrid.size.gridSizeX,
			vdata.densityGrid.size.gridSizeY,
			vdata.densityGrid.size.gridSizeZ,
			vdata.densityGrid.gridOnIndices,
			vdata.densityGrid.gridOnValueIndices,
			RPR_GRID_INDICES_TOPOLOGY_XYZ_U32
		);

		float const maxDensityValue = RPRVolumeAttributes::GetDensityMultiplier(depNode);
		auto densityGridNode = frw::GridNode(context()->GetMaterialSystem());
		densityGridNode.SetGrid(m_densityGrid);
		volumeShader.xSetValue(RPR_MATERIAL_INPUT_DENSITYGRID, densityGridNode);
		volumeShader.xSetParameterF(RPR_MATERIAL_INPUT_DENSITY, maxDensityValue, 1.0f, 1.0f, 1.0f);

		// scale
		m_bboxScale.setToIdentity();
		m_bboxScale[0][0] = vdata.densityGrid.size.gridSizeX * vdata.densityGrid.size.voxelSizeX;
		m_bboxScale[1][1] = vdata.densityGrid.size.gridSizeY * vdata.densityGrid.size.voxelSizeY;
		m_bboxScale[2][2] = vdata.densityGrid.size.gridSizeZ * vdata.densityGrid.size.voxelSizeZ;

#ifdef DUMP_VOLUME_DATA
		NorthstarRPRVolume_DebugDumpGrid(
			vdata.densityGrid.gridSizeX,
			vdata.densityGrid.gridSizeY,
			vdata.densityGrid.gridSizeZ,
			vdata.densityGrid.gridOnIndices,
			vdata.densityGrid.gridOnValueIndices,
			nullptr,
			"C://temp//dbg//",
			"density_grid_Z_"
		);
#endif
	}
	
	if (vdata.albedoGrid.IsValid()) // grid exists
	{
		// normalize grid data
		float maxAlbedoGridValue = *std::max_element(
			vdata.albedoGrid.gridOnValueIndices.begin(),
			vdata.albedoGrid.gridOnValueIndices.end());
		if (maxAlbedoGridValue > 1.0f)
		{
			for (size_t idx = 0; idx < vdata.albedoGrid.gridOnValueIndices.size(); idx++)
			{
				vdata.albedoGrid.gridOnValueIndices[idx] = vdata.albedoGrid.gridOnValueIndices[idx] / maxAlbedoGridValue;
			}
		}

		// create grid
		m_albedoGrid = Context().CreateVolumeGrid(
			vdata.albedoGrid.size.gridSizeX,
			vdata.albedoGrid.size.gridSizeY,
			vdata.albedoGrid.size.gridSizeZ,
			vdata.albedoGrid.gridOnIndices,
			vdata.albedoGrid.gridOnValueIndices,
			RPR_GRID_INDICES_TOPOLOGY_XYZ_U32
		);

		auto albedoGridNode = frw::GridNode(context()->GetMaterialSystem());
		albedoGridNode.SetGrid(m_albedoGrid);		

		float maxAlbedoValue = *std::max_element(vdata.albedoGrid.valuesLookUpTable.begin(), vdata.albedoGrid.valuesLookUpTable.end());
		if (maxAlbedoValue > 1.0f)  //we need to normalize albedo with enabled emission
		{
			std::vector<float> albedoValues;
			albedoValues.resize(vdata.albedoGrid.valuesLookUpTable.size());

			for (size_t idx = 0; idx < vdata.albedoGrid.valuesLookUpTable.size(); idx++)
			{
				albedoValues[idx] = vdata.albedoGrid.valuesLookUpTable[idx] / maxAlbedoValue;
			}
			auto albedoLookupNode = CreateLookupTextureNode(this, albedoValues, albedoGridNode);
			volumeShader.xSetValue(RPR_MATERIAL_INPUT_COLOR, albedoLookupNode);

#ifdef DUMP_VOLUME_DATA
			NorthstarRPRVolume_DebugDumpLookupFloat3(albedoValues, "C://temp//dbg//", "temperature_lookup");

			NorthstarRPRVolume_DebugDumpGrid(
				vdata.albedoGrid.gridSizeX,
				vdata.albedoGrid.gridSizeY,
				vdata.albedoGrid.gridSizeZ,
				vdata.albedoGrid.gridOnIndices,
				vdata.albedoGrid.gridOnValueIndices,
				&albedoValues,
				"C://temp//dbg//",
				"temperature_grid_Z_"
			);
#endif
		}
		else
		{
			auto albedoLookupNode = CreateLookupTextureNode(this, vdata.albedoGrid.valuesLookUpTable, albedoGridNode);
			volumeShader.xSetValue(RPR_MATERIAL_INPUT_COLOR, albedoLookupNode);
		}
	}

	if (vdata.emissionGrid.IsValid()) // grid exists
	{
		// normalize grid data
		float maxEmissionGridValue = *std::max_element(
			vdata.emissionGrid.gridOnValueIndices.begin(),
			vdata.emissionGrid.gridOnValueIndices.end());
		if (maxEmissionGridValue > 1.0f)
		{
			for (size_t idx = 0; idx < vdata.emissionGrid.gridOnValueIndices.size(); idx++)
			{
				vdata.emissionGrid.gridOnValueIndices[idx] = vdata.emissionGrid.gridOnValueIndices[idx] / maxEmissionGridValue;
			}
		}

		// create grid
		m_emissionGrid = Context().CreateVolumeGrid(
			vdata.emissionGrid.size.gridSizeX,
			vdata.emissionGrid.size.gridSizeY,
			vdata.emissionGrid.size.gridSizeZ,
			vdata.emissionGrid.gridOnIndices,
			vdata.emissionGrid.gridOnValueIndices,
			RPR_GRID_INDICES_TOPOLOGY_XYZ_U32
		);

		if (RPRVolumeAttributes::GetEmissionInputType(depNode) == RPRVolumeAttributes::kByValue)
		{
			auto emissionGridNode = frw::GridNode(context()->GetMaterialSystem());
			emissionGridNode.SetGrid(m_emissionGrid);

			auto emissionLookupNode = CreateLookupTextureNode(this, vdata.emissionGrid.valuesLookUpTable, emissionGridNode);
			volumeShader.xSetValue(RPR_MATERIAL_INPUT_EMISSION, emissionLookupNode);
		}
		else if (RPRVolumeAttributes::GetEmissionInputType(depNode) == RPRVolumeAttributes::kByKelvin)
		{
			auto emissionGridNode = frw::GridNode(context()->GetMaterialSystem());
			emissionGridNode.SetGrid(m_emissionGrid);

			auto blackBodyNode = frw::BlackBodyNode(context()->GetMaterialSystem());
			blackBodyNode.SetGridSampler(emissionGridNode);
			blackBodyNode.SetTemperatureValueKelvin(RPRVolumeAttributes::GetEmissionKelvin(depNode));

			volumeShader.xSetValue(RPR_MATERIAL_INPUT_EMISSION, blackBodyNode);
		}
		else
		{
			assert(false); // should not be here!
		}

		if (!vdata.albedoGrid.IsValid()) // albedo doesn't set
		{
			volumeShader.xSetParameterF(RPR_MATERIAL_INPUT_COLOR, 0.0f, 0.0f, 0.0f, 1.0f); // we need to set zero albedo
		}

#ifdef DUMP_VOLUME_DATA
		float maxEmissionValue = *std::max_element(vdata.emissionGrid.valuesLookUpTable.begin(), vdata.emissionGrid.valuesLookUpTable.end());
		std::vector<float> emissionValues;
		emissionValues.resize(vdata.emissionGrid.valuesLookUpTable.size());
		for (size_t idx = 0; idx < vdata.emissionGrid.valuesLookUpTable.size(); idx++)
		{
			emissionValues[idx] = vdata.emissionGrid.valuesLookUpTable[idx] / maxEmissionValue;
		}

		NorthstarRPRVolume_DebugDumpLookupFloat3(/*emissionValues*/ vdata.emissionGrid.valuesLookUpTable, "C://temp//dbg//", "emission_lookup");

		NorthstarRPRVolume_DebugDumpGrid(
			vdata.emissionGrid.gridSizeX,
			vdata.emissionGrid.gridSizeY,
			vdata.emissionGrid.gridSizeZ,
			vdata.emissionGrid.gridOnIndices,
			vdata.emissionGrid.gridOnValueIndices,
			/*&emissionValues, */ &vdata.emissionGrid.valuesLookUpTable,
			"C://temp//dbg//",
			"emission_grid_Z_"
		);
#endif
	}
	m_boundingBoxMesh.SetVolumeShader(volumeShader);

	return m_boundingBoxMesh.IsValid();
}

//===================
// Volume
//===================
FireRenderFluidVolume::FireRenderFluidVolume(FireRenderContext* context, const MDagPath& dagPath)
	: FireRenderCommonVolume(context, dagPath)
{}

FireRenderFluidVolume::~FireRenderFluidVolume()
{
	clear();
}

// not used now, but might need in the future
/*	 bool GetIntersectionPointOfLineAndPlane(
		float x1, float y1, float z1,
		float x2, float y2, float z2,
		float A, float B, float C, float D,
		std::array<float, 3>& out) const
	{
		// - plane equation coeff (need parametric plane equation to calculate intersection with plane)
		// x = x1 + ax*lambda
		// y = y1 + ay*lambda
		// z = z1 + az*lambda
		//---------------
		// x = x2 + (x2-x1)*lambda
		// y = y2 + (y2-y1)*lambda
		// z = z2 + (z2-z1)*lambda
		//---------------
		// A*x + B*y + C*z + D = 0

		// A*(x2 + (x2-x1)*lambda) + B*(y2 + (y2-y1)*lambda) + C*(z2 + (z2-z1)*lambda) + D = 0
		// A*x2 + A*(x2-x1)*lambda + B*y2 + B*(y2-y1)*lambda + C*z2 + C*(z2-z1)*lambda + D = 0
		// A*(x2-x1)*lambda + B*(y2-y1)*lambda + C*(z2-z1)*lambda = - D - A*x2 - B*y2 - C*z2
		// lambda*(A*(x2-x1) + B*(y2-y1) + C*(z2-z1)) = -D -A*x2 -B*y2 -C*z2
		// lambda = (-D -A*x2 -B*y2 -C*z2) / (A*(x2-x1) + B*(y2-y1) + C*(z2-z1))

		 float a1 = A * (x2 - x1);
		 float b1 = B * (y2 - y1);
		 float c1 = C * (z2 - z1);

		 if (abs(a1 + b1 + c1) < FLT_EPSILON)
			 return false;

		float lambda = (-D - A*x2 - B*y2 - C*z2) / (a1 + b1 + c1);
		out[0] = x2 + (x2 - x1)*lambda;
		out[1] = y2 + (y2 - y1)*lambda;
		out[2] = z2 + (z2 - z1)*lambda;

		return true;
	}
	*/

void AddValToArr(MColor val, std::vector<float>& outputControlPoints)
{
	outputControlPoints.push_back(val.r);
	outputControlPoints.push_back(val.g);
	outputControlPoints.push_back(val.b);
}

void AddValToArr(float val, std::vector<float>& outputControlPoints)
{
	outputControlPoints.push_back(val);
}

template <class valType>
void RemapControlPoints(std::vector<float>& outputControlPoints, const std::vector<RampCtrlPoint<valType>>& inputControlPoints)
{
	const size_t count_of_new_control_points = 100;

	for (size_t new_ctrl_point_idx = 0; new_ctrl_point_idx < count_of_new_control_points; ++new_ctrl_point_idx)
	{
		float dist2vx_normalized = (1.0f / count_of_new_control_points) * new_ctrl_point_idx;

		// get 2 most close control points from opacity ramp control points array
		const auto* prev_point = &inputControlPoints[0];
		const auto* next_point = &inputControlPoints[0];

		for (unsigned int idx = 0; idx < inputControlPoints.size(); ++idx) // yes, non-optimal to search every time, but...
		{
			auto& ctrlPoint = inputControlPoints[idx];

			if (ctrlPoint.position > dist2vx_normalized)
			{
				if (idx == 0)
				{
					AddValToArr(ctrlPoint.ctrlPointData, outputControlPoints);
					break;
				}

				next_point = &inputControlPoints[idx];
				prev_point = &inputControlPoints[idx - 1];

				// interpolate values from theese points
				// - only linear interpolation is supported atm
				float coef = ((dist2vx_normalized - prev_point->position) / (next_point->position - prev_point->position));
				auto prevValue = prev_point->ctrlPointData;
				auto nextValue = next_point->ctrlPointData;
				auto calcRes = nextValue - prevValue;
				calcRes = coef * calcRes;
				calcRes = calcRes + prevValue;

				AddValToArr(calcRes, outputControlPoints);
				break;
			}
		}

	}
}

bool FireRenderFluidVolume::TranslateGeneralVolumeData(VolumeData* pVolumeData, MFnFluid& fnFluid)
{
	MStatus mstatus;
	FireRenderError error;

	// extract resolution of volume from fluid object
	unsigned int Xres = 0;
	unsigned int Yres = 0;
	unsigned int Zres = 0;
	mstatus = fnFluid.getResolution(Xres, Yres, Zres);

	unsigned int gridSize = fnFluid.gridSize();
	if ((MStatus::kSuccess != mstatus) || (gridSize < (Xres * Yres * Zres))) // gridSize is an upper bound, not an actual size
	{
		error.set("MFnFluid:", "failed to get resolutions", false, false);
		return false;
	}

	// save result in volume data container
	pVolumeData->gridSizeX = Xres;
	pVolumeData->gridSizeY = Yres;
	pVolumeData->gridSizeZ = Zres;

	// prepare volume data container for future input data
	pVolumeData->voxels.resize(Xres * Yres * Zres);

	// extract xyz dimetions of the volume
	// they will be applied to volume bbox as scale
	double Xdim = 0.0f;
	double Ydim = 0.0f;
	double Zdim = 0.0f;
	mstatus = fnFluid.getDimensions(Xdim, Ydim, Zdim);
	if (MStatus::kSuccess != mstatus)
	{
		error.set("MFnFluid:", "failed to get dimensions", false, false);
		return false;
	}

	if (!SetBBox(Xdim, Ydim, Zdim))
	{
		return false;
	}

	// success!
	return true;
}

void FillArrayWithGradient(
	std::vector<float>& outputValues,
	unsigned int Xres,
	unsigned int Yres,
	unsigned int Zres,
	const MFnFluid::FluidGradient gradient)
{
	VoxelParams voxelParams;
	voxelParams.Xres = Xres;
	voxelParams.Yres = Yres;
	voxelParams.Zres = Zres;

	VolumeGradient volGrad = static_cast<VolumeGradient>(static_cast<int>(gradient) + 4);

	// - write data to output
	for (size_t z_idx = 0; z_idx < Zres; ++z_idx)
		for (size_t y_idx = 0; y_idx < Yres; ++y_idx)
			for (size_t x_idx = 0; x_idx < Xres; ++x_idx)
			{
				voxelParams.x = (unsigned int) x_idx;
				voxelParams.y = (unsigned int) y_idx;
				voxelParams.z = (unsigned int) z_idx;

				float dist2vx_normalized = GetDistParamNormalized(voxelParams, volGrad);
				outputValues.push_back(dist2vx_normalized);
			}
}

bool FireRenderFluidVolume::ReadDensityIntoArray(MFnFluid& fnFluid, std::vector<float>& outputValues)
{
	MStatus mstatus;
	FireRenderError error;

	// get density set up method
	MFnFluid::FluidMethod density_method = MFnFluid::kZero;
	MFnFluid::FluidGradient density_gradient = MFnFluid::kConstant;
	mstatus = fnFluid.getDensityMode(density_method, density_gradient);
	if (MStatus::kSuccess != mstatus)
	{
		error.set("MFnFluid:", "failed to read density", false, false);
		return false;
	}

	unsigned int Xres = 0;
	unsigned int Yres = 0;
	unsigned int Zres = 0;
	mstatus = fnFluid.getResolution(Xres, Yres, Zres);
	unsigned int gridSize = Xres*Yres*Zres;
	outputValues.reserve(gridSize);

	// empty grid
	if (density_method == MFnFluid::kZero)
	{
		outputValues.resize(gridSize, 0.0f);
		return true;
	}

	// generated by Maya simulation
	if ((density_method == MFnFluid::kDynamicGrid) || (density_method == MFnFluid::kStaticGrid))
	{
		float* density = fnFluid.density(&mstatus);
		if (MStatus::kSuccess != mstatus)
		{
			mstatus.perror("failed to get opacity ramp");
			return false;
		}

		// - convert data to rpr representation
		for (size_t idx = 0; idx < gridSize; ++idx)
		{
			outputValues.push_back(density[idx]);
		}

		return true;
	}

	// need to generate ourselves by computation
	if (density_method == MFnFluid::kGradient)
	{
		// - write data to output
		FillArrayWithGradient(outputValues, Xres, Yres, Zres, density_gradient);

		return true;
	}

	// dummy return for compiler
	return false;
}

bool FireRenderFluidVolume::ReadTemperatureIntoArray(MFnFluid& fnFluid, std::vector<float>& outputValues)
{
	MStatus mstatus;
	FireRenderError error;

	// get temperature set up method
	MFnFluid::FluidMethod temperature_method = MFnFluid::kZero;
	MFnFluid::FluidGradient temperature_gradient = MFnFluid::kConstant;
	mstatus = fnFluid.getTemperatureMode(temperature_method, temperature_gradient);
	if (MStatus::kSuccess != mstatus)
	{
		error.set("MFnFluid:", "failed to read temperature", false, false);
		return false;
	}

	unsigned int Xres = 0;
	unsigned int Yres = 0;
	unsigned int Zres = 0;
	mstatus = fnFluid.getResolution(Xres, Yres, Zres);
	unsigned int gridSize = Xres*Yres*Zres;
	outputValues.reserve(gridSize);

	// empty grid
	if (temperature_method == MFnFluid::kZero)
	{
		outputValues.resize(gridSize, 0.0f);
		return true;
	}

	// generated by Maya simulation
	if ((temperature_method == MFnFluid::kDynamicGrid) || (temperature_method == MFnFluid::kStaticGrid))
	{
		float* temperature = fnFluid.temperature(&mstatus);
		if (MStatus::kSuccess != mstatus)
		{
			error.set("MFnFluid:", "failed to get temperature from fluid", false, false);
			return false;
		}

		// - convert data to rpr representation
		for (size_t idx = 0; idx < gridSize; ++idx)
		{
			outputValues.push_back(temperature[idx]);
		}

		return true;
	}

	// need to generate ourselves by computation
	if (temperature_method == MFnFluid::kGradient)
	{
		mstatus = fnFluid.getResolution(Xres, Yres, Zres);

		// - write data to output
		FillArrayWithGradient(outputValues, Xres, Yres, Zres, temperature_gradient);

		return true;
	}

	// dummy return for compiler
	return false;
}

bool FireRenderFluidVolume::ReadFuelIntoArray(MFnFluid& fnFluid, std::vector<float>& outputValues)
{
	MStatus mstatus;
	FireRenderError error;

	// get fuel set up method
	MFnFluid::FluidMethod fuel_method = MFnFluid::kZero;
	MFnFluid::FluidGradient fuel_gradient = MFnFluid::kConstant;
	mstatus = fnFluid.getFuelMode(fuel_method, fuel_gradient);
	if (MStatus::kSuccess != mstatus)
	{
		error.set("MFnFluid:", "failed to read fuel", false, false);
		return false;
	}
	
	unsigned int Xres = 0;
	unsigned int Yres = 0;
	unsigned int Zres = 0;
	mstatus = fnFluid.getResolution(Xres, Yres, Zres);
	unsigned int gridSize = Xres*Yres*Zres;
	outputValues.reserve(gridSize);

	// empty grid
	if (fuel_method == MFnFluid::kZero)
	{
		outputValues.resize(gridSize, 0.0f);
		return true;
	}

	// generated by Maya simulation
	if ((fuel_method == MFnFluid::kDynamicGrid) || (fuel_method == MFnFluid::kStaticGrid))
	{
		float* fuel = fnFluid.fuel(&mstatus);
		if (MStatus::kSuccess != mstatus)
		{
			error.set("MFnFluid:", "failed to get fuel from fluid", false, false);
			return false;
		}

		// - convert data to rpr representation
		for (size_t idx = 0; idx < gridSize; ++idx)
		{
			outputValues.push_back(fuel[idx]);
		}

		return true;
	}

	// need to generate ourselves by computation
	if (fuel_method == MFnFluid::kGradient)
	{
		// - write data to output
		FillArrayWithGradient(outputValues, Xres, Yres, Zres, fuel_gradient);

		return true;
	}

	// dummy return for compiler
	return false;
}

bool FireRenderFluidVolume::ReadPressureIntoArray(MFnFluid& fnFluid, std::vector<float>& outputValues)
{
	MStatus mstatus;
	FireRenderError error;

	// pressure is different from the rest of the inputs:
	// it doesn't have an input method
	float* pressure = fnFluid.pressure(&mstatus);
	if (MStatus::kSuccess != mstatus)
	{
		error.set("MFnFluid:", "failed to get fuel from fluid", false, false);
		return false;
	}

	// convert data to rpr representation
	unsigned int Xres = 0;
	unsigned int Yres = 0;
	unsigned int Zres = 0;
	mstatus = fnFluid.getResolution(Xres, Yres, Zres);
	unsigned int gridSize = Xres * Yres*Zres;
	outputValues.reserve(gridSize);

	for (size_t idx = 0; idx < gridSize; ++idx)
	{
		outputValues.push_back(pressure[idx]);
	}

	return true;
}

bool FireRenderFluidVolume::ReadSpeedIntoArray(MFnFluid& fnFluid, std::vector<float>& outputValues)
{
	MStatus mstatus;
	FireRenderError error;

	// get fuel set up method
	MFnFluid::FluidMethod velocityMethod = MFnFluid::kZero;
	MFnFluid::FluidGradient velocityGradient = MFnFluid::kConstant;
	mstatus = fnFluid.getVelocityMode(velocityMethod, velocityGradient);
	if (MStatus::kSuccess != mstatus)
	{
		error.set("MFnFluid:", "failed to read fuel", false, false);
		return false;
	}

	// NOTE: unlike all of the other inputs, velocity uses its own XYZ resolutions!
	int Xres = 0;
	int Yres = 0;
	int Zres = 0;
	mstatus = fnFluid.velocityGridSizes(Xres, Yres, Zres);
	unsigned int gridSize = Xres*Yres*Zres;
	outputValues.reserve(gridSize);

	// empty grid
	if (velocityMethod == MFnFluid::kZero)
	{
		outputValues.resize(gridSize, 0.0f);
		return true;
	}

	// generated by Maya simulation
	if ((velocityMethod == MFnFluid::kDynamicGrid) || (velocityMethod == MFnFluid::kStaticGrid))
	{
		float* xSpeed = nullptr;
		float* ySpeed = nullptr;
		float* zSpeed = nullptr;
		mstatus = fnFluid.getVelocity(xSpeed, ySpeed, zSpeed);
		if (MStatus::kSuccess != mstatus)
		{
			error.set("MFnFluid:", "failed to get fuel from fluid", false, false);
			return false;
		}

		// - convert data to rpr representation
		for (size_t z_idx = 0; z_idx < Zres; ++z_idx)
			for (size_t y_idx = 0; y_idx < Yres; ++y_idx)
				for (size_t x_idx = 0; x_idx < Xres; ++x_idx)
				{
					outputValues.push_back(sqrt(xSpeed[x_idx]*xSpeed[x_idx] + ySpeed[y_idx]*ySpeed[y_idx] + zSpeed[z_idx]*zSpeed[z_idx]));
				}

		return true;
	}

	// need to generate ourselves by computation
	if (velocityMethod == MFnFluid::kGradient)
	{
		// - write data to output
		FillArrayWithGradient(outputValues, Xres, Yres, Zres, velocityGradient);

		return true;
	}

	// dummy return for compiler
	return false;
}

bool FireRenderFluidVolume::ProcessInputField(int inputField,
	std::vector<float>& outData,
	unsigned int Xres,
	unsigned int Yres,
	unsigned int Zres,
	MFnFluid& fnFluid)
{
	outData.clear();
	outData.reserve(fnFluid.gridSize());

	switch (inputField)
	{
		case 0 /*Constant*/:
		{
			outData.resize(Xres*Yres*Zres, 1.0f);
			break;
		}

		case 1 /*X Gradient*/:
		{
			FillArrayWithGradient(outData, Xres, Yres, Zres, MFnFluid::kXGradient);
			break;
		}

		case 2 /*Y Gradient*/:
		{
			FillArrayWithGradient(outData, Xres, Yres, Zres, MFnFluid::kYGradient);
			break;
		}

		case 3 /*Z Gradient*/:
		{
			FillArrayWithGradient(outData, Xres, Yres, Zres, MFnFluid::kZGradient);
			break;
		}

		case 4 /*Center Gradient*/:
		{
			FillArrayWithGradient(outData, Xres, Yres, Zres, MFnFluid::kCenterGradient);
			break;
		}

		case 5 /*Density*/:
		{
			bool success = ReadDensityIntoArray(fnFluid, outData);
			if (!success)
			{
				return false;
			}
			break;
		}

		case 6 /*Temperature*/:
		{
			bool success = ReadTemperatureIntoArray(fnFluid, outData);
			if (!success)
			{
				return false;
			}
			break;
		}

		case 7 /*Fuel*/:
		{
			bool success = ReadFuelIntoArray(fnFluid, outData);
			if (!success)
			{
				return false;
			}
			break;
		}

		case 8 /*Pressure*/:
		{
			bool success = ReadPressureIntoArray(fnFluid, outData);
			if (!success)
			{
				return false;
			}
			break;
		}

		case 9 /*Speed (Velocity) */:
		{
			bool success = ReadSpeedIntoArray(fnFluid, outData);
			if (!success)
			{
				return false;
			}
			break;
		}

		default:
		{
			// not supported yet
			return false;
		}
	}

	return true;
}

void GetRampValue(MRampAttribute& valueRamp, float position, float& value, MStatus* mstatus)
{
	valueRamp.getValueAtPosition(position, value, mstatus);
}

void GetRampValue(MRampAttribute& valueRamp, float position, MColor& value, MStatus* mstatus)
{
	valueRamp.getColorAtPosition(position, value, mstatus);
}

float GetValFromNURBS(MFnNurbsCurve& nurbsCurve, float inX, float tol = 0.01f)
{
	if (inX < tol)
		return 0.0f;

	if ((1.0f - tol) < inX)
		return 1.0f;

	float param = 0.5f;
	int step = 0;

	const int maxStepCount = 100;

	do
	{
		MPoint tpoint;
		nurbsCurve.getPointAtParam(param, tpoint);

		float tempX = (float) tpoint.x;
		float tempY = (float) tpoint.y;

		if (abs(tempX - inX) < tol)
			return tempY;

		if (tempX > inX)
		{
			param = param - (param / 2);
		}
		else
		{
			param = param + (param / 2);
		}

		++step;
	} 
	while (step < maxStepCount);

	return 1.0f;
}

const size_t numCtrlPoints = 100; // number of control points to be passed to RPR; RPR doesn't do interpolation thus we can't pass Maya control points to RPR
template <class valType>
bool ProcessControlPoints(MPlug& valuePlug, std::vector<float>& outputCtrlPoints, float bias)
{
	MStatus mstatus;

	// get ramp
	MRampAttribute valueRamp(valuePlug);

	// prepare output
	outputCtrlPoints.clear();
	outputCtrlPoints.reserve(numCtrlPoints * (sizeof(valType) / sizeof(float)));

	// process bias
	MFnNurbsCurve nurbsCurve; MObject nurbsMObject;
	bool applyBias = (bias != FLT_EPSILON) && ((bias >= -1.0f) && (bias <= 1.0f));
	
	if (applyBias)
	{
		float yPos = 0.5f + (bias / 2.0f);
		float xPos = 1.0f - yPos;

		MPointArray controlVtx;
		controlVtx.append(0.0f, 0.0f);
		controlVtx.append(xPos, yPos);
		controlVtx.append(1.0f, 1.0f);

		MDoubleArray knots;
		knots.append(0.0f);
		knots.append(0.0f);
		knots.append(1.0f);
		knots.append(1.0f);

		nurbsMObject = nurbsCurve.create(controlVtx, knots, 2, MFnNurbsCurve::kOpen, true, true, MObject::kNullObj, &mstatus);
		if (MStatus::kSuccess != mstatus)
		{
			return false;
		}
	}

	// remap control points (can't pass maya control points to RPR)
	for (size_t idx = 0; idx <= numCtrlPoints; ++idx)
	{
		float pos = idx * (1.0f / numCtrlPoints);
		if (applyBias)
		{
			pos = GetValFromNURBS(nurbsCurve, pos);
		}
		valType value;
		GetRampValue(valueRamp, pos, value, &mstatus);
		if (MStatus::kSuccess != mstatus)
		{
			return false;
		}

		AddValToArr(value, outputCtrlPoints);
	}

	if (!nurbsMObject.isNull())
		MGlobal::deleteNode(nurbsMObject);

	return true;
}

bool FireRenderFluidVolume::TranslateDensity(VolumeData* pVolumeData, MFnFluid& fnFluid, MFnDependencyNode& shaderNode)
{
	FireRenderError error;

	// get opacity plug
	MPlug opacityPlug = shaderNode.findPlug("opacity");
	if (opacityPlug.isNull())
	{
		error.set("MFnFluid:", "failed to get opacity ramp", false, false);
		return false;
	}

	// get bias
	MPlug opacityBiasPlug = shaderNode.findPlug("opacityInputBias");
	if (opacityBiasPlug.isNull())
	{
		error.set("MFnFluid:", "failed to get opacity input bias", false, false);
		return false;
	}
	float bias = opacityBiasPlug.asFloat();

	// get control points
	if (!ProcessControlPoints<float>(opacityPlug, pVolumeData->denstiyLookupCtrlPoints, bias))
	{
		error.set("MFnFluid:", "failed to get density control points", false, false);
	}

	// get albedo (color) input channel (can be density, temperature, fuel, velocity, etc.)
	MPlug opacityInputPlug = shaderNode.findPlug("opacityInput");
	if (opacityInputPlug.isNull())
	{
		error.set("MFnFluid:", "failed to get opacityInputPlug", false, false);
		return false;
	}
	int opacityInputField = opacityInputPlug.asInt();

	// fill voxel data with values from input channel
	ProcessInputField(opacityInputField, pVolumeData->densityVal, (unsigned int) pVolumeData->gridSizeX, (unsigned int) pVolumeData->gridSizeY, (unsigned int) pVolumeData->gridSizeZ, fnFluid);

	// apply noise
	// - check if noise is enabled for this channel
	MPlug isNoiseForDensityEnabledPlug = shaderNode.findPlug("opacityTexture");
	if (isNoiseForDensityEnabledPlug.isNull())
	{
		error.set("MFnFluid:", "failed to get noise enabled plug for Opacity", false, false);
		return false;
	}
	bool isNoiseForDensityEnabled = isNoiseForDensityEnabledPlug.asBool();

	// - apply noise to voxel values
	if (isNoiseForDensityEnabled)
	{
		bool success = ApplyNoise(pVolumeData->densityVal);
		if (!success)
			return false;
	}

	// success!
	return true;
}

bool FireRenderFluidVolume::TranslateAlbedo(VolumeData* pVolumeData, MFnFluid& fnFluid, MFnDependencyNode& shaderNode)
{
	MStatus mstatus;
	FireRenderError error;

	// ensure color method is byShader; direct color grid set up is not supported atm
	MFnFluid::ColorMethod colorMethod;
	mstatus = fnFluid.getColorMode(colorMethod);
	if (MStatus::kSuccess != mstatus)
	{
		error.set("MFnFluid:", "failed to get color for Albedo", false, false);
		return false;
	}

	if (colorMethod != MFnFluid::kUseShadingColor)
	{
		error.set("MFnFluid:", "not supported color method for Albedo", false, false);
		return false;
	}

	// get color plug
	MPlug colorPlug = shaderNode.findPlug("color");
	if (colorPlug.isNull())
	{
		error.set("MFnFluid:", "failed to get color ramp for Albedo", false, false);
		return false;
	}

	// get bias
	MPlug colorBiasPlug = shaderNode.findPlug("colorInputBias");
	if (colorBiasPlug.isNull())
	{
		error.set("MFnFluid:", "failed to get color input bias", false, false);
		return false;
	}
	float bias = colorBiasPlug.asFloat();

	// get control points
	if (!ProcessControlPoints<MColor>(colorPlug, pVolumeData->albedoLookupCtrlPoints, bias))
	{
		error.set("MFnFluid:", "failed to get density albedo points", false, false);
	}

	// get albedo (color) input channel (can be density, temperature, fuel, velocity, etc.)
	MPlug colorInputPlug = shaderNode.findPlug("colorInput");
	if (colorInputPlug.isNull())
	{
		error.set("MFnFluid:", "failed to get colorInputPlug", false, false);
		return false;
	}
	int colorInputField = colorInputPlug.asInt();

	// fill voxel data with values from input channel
	ProcessInputField(colorInputField, pVolumeData->albedoVal, (unsigned int) pVolumeData->gridSizeX, (unsigned int) pVolumeData->gridSizeY, (unsigned int) pVolumeData->gridSizeZ, fnFluid);

	// apply noise
	// - check if noise is enabled for this channel
	MPlug isNoiseForAlbedoEnabledPlug = shaderNode.findPlug("colorTexture");
	if (isNoiseForAlbedoEnabledPlug.isNull())
	{
		error.set("MFnFluid:", "failed to get noise enabled plug for Albedo", false, false);
		return false;
	}
	bool isNoiseForAlbedoEnabled = isNoiseForAlbedoEnabledPlug.asBool();

	// - apply noise to voxel values
	if (isNoiseForAlbedoEnabled)
	{
		bool success = ApplyNoise(pVolumeData->albedoVal);
		if (!success)
			return false;
	}

	// success!
	return true;
}

bool FireRenderFluidVolume::TranslateEmission(VolumeData* pVolumeData, MFnFluid& fnFluid, MFnDependencyNode& shaderNode)
{
	MStatus mstatus;
	FireRenderError error;

	// ensure color method is byShader; direct color grid set up is not supported atm
	MFnFluid::ColorMethod colorMethod;
	mstatus = fnFluid.getColorMode(colorMethod);
	if (MStatus::kSuccess != mstatus)
	{
		error.set("MFnFluid:", "failed to get color for Emission", false, false);
		return false;
	}

	if (colorMethod != MFnFluid::kUseShadingColor)
	{
		error.set("MFnFluid:", "not supported color method for Emission", false, false);
		return false;
	}

	// get incandescence plug
	MPlug incandescencePlug = shaderNode.findPlug("incandescence");
	if (incandescencePlug.isNull())
	{
		error.set("MFnFluid:", "failed to get incandescence ramp for Emission", false, false);
		return false;
	}

	// get bias
	MPlug incandescenceBiasPlug = shaderNode.findPlug("incandescenceInputBias");
	if (incandescenceBiasPlug.isNull())
	{
		error.set("MFnFluid:", "failed to get incandescence input bias", false, false);
		return false;
	}
	float bias = incandescenceBiasPlug.asFloat();

	// get control points
	if (!ProcessControlPoints<MColor>(incandescencePlug, pVolumeData->emissionLookupCtrlPoints, bias))
	{
		error.set("MFnFluid:", "failed to get emission albedo points", false, false);
	}

	// get albedo (color) input channel (can be density, temperature, fuel, velocity, etc.)
	MPlug incandescenceInputPlug = shaderNode.findPlug("incandescenceInput");
	if (incandescenceInputPlug.isNull())
	{
		error.set("MFnFluid:", "failed to get incandescenceInputPlug", false, false);
		return false;
	}
	int incandescenceInputPlugField = incandescenceInputPlug.asInt();

	// fill voxel data with values from input channel
	ProcessInputField(incandescenceInputPlugField, pVolumeData->emissionVal, (unsigned int) pVolumeData->gridSizeX, (unsigned int) pVolumeData->gridSizeY, (unsigned int) pVolumeData->gridSizeZ, fnFluid);

	// apply noise
	// - check if noise is enabled for this channel
	MPlug isNoiseForEmissionEnabledPlug = shaderNode.findPlug("incandTexture");
	if (isNoiseForEmissionEnabledPlug.isNull())
	{
		error.set("MFnFluid:", "failed to get noise enabled plug for Emission", false, false);
		return false;
	}
	bool isNoiseForEmissionEnabled = isNoiseForEmissionEnabledPlug.asBool();

	// - apply noise to voxel values
	if (isNoiseForEmissionEnabled)
	{
		bool success = ApplyNoise(pVolumeData->albedoVal);
		if (!success)
			return false;
	}

	// success!
	return true;
}

bool FireRenderFluidVolume::ApplyNoise(std::vector<float>& channelValues)
{
	return true;
}

bool FireRenderFluidVolume::TranslateVolume(void)
{
	VolumeData vdata;
	FireRenderError error;

	// setup
	const MObject& node = Object();

	// get fluid object
	MStatus mstatus;
	MFnFluid fnFluid(node, &mstatus);
	if (MStatus::kSuccess != mstatus)
	{
		error.set("Volumes:", "MFnFluid constructor", false, false);
		return false;
	}

	// extract data from fluid object
	bool success;
	success = TranslateGeneralVolumeData(&vdata, fnFluid);
	if (!success)
	{
		return false;
	}

	// get volume shader
	MDagPath path = MDagPath::getAPathTo(node);
	MObjectArray shdrs = getConnectedShaders(path);
	size_t count_shdrs = shdrs.length();
	MFnDependencyNode shaderNode(shdrs[0 /*idx*/]); // assuming first volume shder is the one we need
#ifdef _DEBUG
	MString shaderName = shaderNode.name();
	MString shaderType = shaderNode.typeName();
	MTypeId mayaId = shaderNode.typeId();
#endif

	// get initial dimensions from volume
	m_bboxScale.setToIdentity();
	MPlug dimensionWPlug = shaderNode.findPlug("dimensionsW");
	MPlug dimensionHPlug = shaderNode.findPlug("dimensionsH");
	MPlug dimensionDPlug = shaderNode.findPlug("dimensionsD");
	if (dimensionWPlug.isNull() || dimensionHPlug.isNull() || dimensionDPlug.isNull())
	{
		error.set("Volumes:", "MFnFluid failed to get dimentions plugs", false, false);
		return false;
	}
	m_bboxScale[0][0] = dimensionWPlug.asFloat();
	m_bboxScale[1][1] = dimensionHPlug.asFloat();
	m_bboxScale[2][2] = dimensionDPlug.asFloat();

	// get extra transform from volume shader
	// - it is applied only when "Auto Resize" is checked
	MPlug isGridAutoResizePlug = shaderNode.findPlug("autoResize");
	if (isGridAutoResizePlug.isNull())
	{
		error.set("Volumes:", "MFnFluid failed to get auto resize plug", false, false);
		return false;
	}
	bool isGridAutoResize = isGridAutoResizePlug.asBool();

	if (isGridAutoResize)
	{
		MPlug dynamicOffsetXPlug = shaderNode.findPlug("dynamicOffsetX");
		MPlug dynamicOffsetYPlug = shaderNode.findPlug("dynamicOffsetY");
		MPlug dynamicOffsetZPlug = shaderNode.findPlug("dynamicOffsetZ");
		if (dynamicOffsetXPlug.isNull() || dynamicOffsetYPlug.isNull() || dynamicOffsetZPlug.isNull())
		{
			error.set("Volumes:", "MFnFluid failed to get dynamic offsets plugs", false, false);
			return false;
		}
		m_matrix.setToIdentity();
		m_matrix[3][0] = dynamicOffsetXPlug.asFloat();
		m_matrix[3][1] = dynamicOffsetYPlug.asFloat();
		m_matrix[3][2] = dynamicOffsetZPlug.asFloat();

		// apply auto resize scale
		MPlug baseResolutionPlug = shaderNode.findPlug("baseResolution");
		MPlug resolutionWPlug = shaderNode.findPlug("resolutionW");
		MPlug resolutionHPlug = shaderNode.findPlug("resolutionH");
		MPlug resolutionDPlug = shaderNode.findPlug("resolutionD");

		if (baseResolutionPlug.isNull() || resolutionWPlug.isNull() || resolutionHPlug.isNull() || resolutionDPlug.isNull())
		{
			error.set("Volumes:", "MFnFluid failed to get dynamic scale plugs", false, false);
			return false;
		}

		m_bboxScale[0][0] *= resolutionWPlug.asFloat() / baseResolutionPlug.asFloat();
		m_bboxScale[1][1] *= resolutionHPlug.asFloat() / baseResolutionPlug.asFloat();
		m_bboxScale[2][2] *= resolutionDPlug.asFloat() / baseResolutionPlug.asFloat();

	}

	// read fluid data fields to volume data container
	success = TranslateAlbedo(&vdata, fnFluid, shaderNode);
	if (!success)
	{
		return false;
	}
	success = TranslateDensity(&vdata, fnFluid, shaderNode);
	if (!success)
	{
		return false;
	}
	success = TranslateEmission(&vdata, fnFluid, shaderNode);
	if (!success)
	{
		return false;
	}

	// create rpr volume
	m_volume = Context().CreateVolume(
		vdata.gridSizeX, vdata.gridSizeY, vdata.gridSizeZ,
		(float*)vdata.voxels.data(), vdata.voxels.size(),
		(float*)vdata.albedoLookupCtrlPoints.data(), vdata.albedoLookupCtrlPoints.size(),
		(float*)vdata.albedoVal.data(),
		(float*)vdata.emissionLookupCtrlPoints.data(), vdata.emissionLookupCtrlPoints.size(),
		(float*)vdata.emissionVal.data(),
		(float*)vdata.denstiyLookupCtrlPoints.data(), vdata.denstiyLookupCtrlPoints.size(),
		(float*)vdata.densityVal.data()
	);

	return m_volume.IsValid();
}

//===================
// Northstar fluid volume
//===================
NorthstarFluidVolume::NorthstarFluidVolume(FireRenderContext* context, const MDagPath& dagPath)
	: FireRenderFluidVolume(context, dagPath)
{}

NorthstarFluidVolume::~NorthstarFluidVolume()
{
	clear();
}

void NorthstarFluidVolume::attachToScene()
{
	if (m_isVisible)
		return;

	frw::Scene scene = context()->GetScene();
	if (!scene.IsValid())
		return;

	scene.Attach(m_boundingBoxMesh);

	m_isVisible = true;
}

void NorthstarFluidVolume::detachFromScene()
{
	if (!m_isVisible)
		return;

	if (frw::Scene scene = context()->GetScene())
	{
		scene.Detach(m_boundingBoxMesh);

		m_isVisible = false;
	}
}

void NorthstarFluidVolume::ApplyTransform(void)
{
	if (!m_boundingBoxMesh.IsValid())
		return;

	auto node = Object();
	MFnDagNode fnDagNode(node);
	MDagPath path = MDagPath::getAPathTo(node);

	// get transform
	MMatrix matrix = path.inclusiveMatrix();
	matrix = m_matrix * matrix;

	// convert Maya mesh in cm to m
	MMatrix scaleM;
	scaleM.setToIdentity();
	scaleM[0][0] = scaleM[1][1] = scaleM[2][2] = GetSceneUnitsConversionCoefficient();

	// apply bbox grid size (by scale matrix)
	matrix = m_bboxScale * matrix * scaleM;

	float mfloats[4][4];
	matrix.get(mfloats);

	// apply transform to fake mesh
	m_boundingBoxMesh.SetTransform(*mfloats, false);
}

bool NorthstarFluidVolume::TranslateVolume(void)
{
	VolumeData vdata;
	FireRenderError error;

	// setup
	const MObject& node = Object();

	// get fluid object
	MStatus mstatus;
	MFnFluid fnFluid(node, &mstatus);
	if (MStatus::kSuccess != mstatus)
	{
		error.set("Volumes:", "MFnFluid constructor", false, false);
		return false;
	}

	// extract data from fluid object
	bool success;
	success = TranslateGeneralVolumeData(&vdata, fnFluid);
	if (!success)
	{
		return false;
	}

	// get volume shader
	MDagPath path = MDagPath::getAPathTo(node);
	MObjectArray shdrs = getConnectedShaders(path);
	size_t count_shdrs = shdrs.length();
	MFnDependencyNode shaderNode(shdrs[0 /*idx*/]); // assuming first volume shder is the one we need
#ifdef _DEBUG
	MString shaderName = shaderNode.name();
	MString shaderType = shaderNode.typeName();
	MTypeId mayaId = shaderNode.typeId();
#endif

	// get initial dimensions from volume
	m_bboxScale.setToIdentity();
	MPlug dimensionWPlug = shaderNode.findPlug("dimensionsW");
	MPlug dimensionHPlug = shaderNode.findPlug("dimensionsH");
	MPlug dimensionDPlug = shaderNode.findPlug("dimensionsD");
	if (dimensionWPlug.isNull() || dimensionHPlug.isNull() || dimensionDPlug.isNull())
	{
		error.set("Volumes:", "MFnFluid failed to get dimentions plugs", false, false);
		return false;
	}
	m_bboxScale[0][0] = dimensionWPlug.asFloat();
	m_bboxScale[1][1] = dimensionHPlug.asFloat();
	m_bboxScale[2][2] = dimensionDPlug.asFloat();

	// get extra transform from volume shader
	// - it is applied only when "Auto Resize" is checked
	MPlug isGridAutoResizePlug = shaderNode.findPlug("autoResize");
	if (isGridAutoResizePlug.isNull())
	{
		error.set("Volumes:", "MFnFluid failed to get auto resize plug", false, false);
		return false;
	}
	bool isGridAutoResize = isGridAutoResizePlug.asBool();

	if (isGridAutoResize)
	{
		MPlug dynamicOffsetXPlug = shaderNode.findPlug("dynamicOffsetX");
		MPlug dynamicOffsetYPlug = shaderNode.findPlug("dynamicOffsetY");
		MPlug dynamicOffsetZPlug = shaderNode.findPlug("dynamicOffsetZ");
		if (dynamicOffsetXPlug.isNull() || dynamicOffsetYPlug.isNull() || dynamicOffsetZPlug.isNull())
		{
			error.set("Volumes:", "MFnFluid failed to get dynamic offsets plugs", false, false);
			return false;
		}
		m_matrix.setToIdentity();
		m_matrix[3][0] = dynamicOffsetXPlug.asFloat();
		m_matrix[3][1] = dynamicOffsetYPlug.asFloat();
		m_matrix[3][2] = dynamicOffsetZPlug.asFloat();

		// apply auto resize scale
		MPlug baseResolutionPlug = shaderNode.findPlug("baseResolution");
		MPlug resolutionWPlug = shaderNode.findPlug("resolutionW");
		MPlug resolutionHPlug = shaderNode.findPlug("resolutionH");
		MPlug resolutionDPlug = shaderNode.findPlug("resolutionD");

		if (baseResolutionPlug.isNull() || resolutionWPlug.isNull() || resolutionHPlug.isNull() || resolutionDPlug.isNull())
		{
			error.set("Volumes:", "MFnFluid failed to get dynamic scale plugs", false, false);
			return false;
		}

		m_bboxScale[0][0] *= resolutionWPlug.asFloat() / baseResolutionPlug.asFloat();
		m_bboxScale[1][1] *= resolutionHPlug.asFloat() / baseResolutionPlug.asFloat();
		m_bboxScale[2][2] *= resolutionDPlug.asFloat() / baseResolutionPlug.asFloat();

	}

	// read fluid data fields to volume data container
	success = TranslateAlbedo(&vdata, fnFluid, shaderNode);
	if (!success)
	{
		return false;
	}
	success = TranslateDensity(&vdata, fnFluid, shaderNode);
	if (!success)
	{
		return false;
	}
	success = TranslateEmission(&vdata, fnFluid, shaderNode);
	if (!success)
	{
		return false;
	}

	// create base mesh
	m_boundingBoxMesh = Context().CreateVoidMesh();

	// create volume shader
	auto volumeShader = frw::Shader(context()->GetMaterialSystem(), frw::ShaderType::ShaderTypeVolume);

	// compute grid values because density lookup is not implemented in Northstar
	std::vector<float> densityValues;
	densityValues.resize(vdata.densityVal.size());

	for (size_t idx = 0; idx < vdata.densityVal.size(); idx++) 
	{
		if (vdata.densityVal[idx] < 0)
		{
			densityValues[idx] = 0;  // process incorrect lookup index
			continue;
		}
		if (vdata.densityVal[idx] >= 1)
		{
			densityValues[idx] = vdata.denstiyLookupCtrlPoints.back(); // process incorrect lookup index
			continue;
		}
		size_t lookupIdx = floor(vdata.densityVal[idx] * (vdata.denstiyLookupCtrlPoints.size() - 1));

		// linear interpolation
		float firstValue = vdata.denstiyLookupCtrlPoints[lookupIdx];
		float secondValue = vdata.denstiyLookupCtrlPoints[lookupIdx + 1];
		densityValues[idx] = firstValue + (secondValue - firstValue) * (vdata.densityVal[idx] * vdata.denstiyLookupCtrlPoints.size() - lookupIdx);
	}

	// create grids and lookups
	auto volumeData = Context().CreateVolumeData(
		vdata.gridSizeX, vdata.gridSizeY, vdata.gridSizeZ,
		(float*)vdata.voxels.data(), vdata.voxels.size(),
		(float*)vdata.albedoLookupCtrlPoints.data(), vdata.albedoLookupCtrlPoints.size(),
		(float*)vdata.albedoVal.data(),
		(float*)vdata.emissionLookupCtrlPoints.data(), vdata.emissionLookupCtrlPoints.size(),
		(float*)vdata.emissionVal.data(),
		(float*)vdata.denstiyLookupCtrlPoints.data(), vdata.denstiyLookupCtrlPoints.size(),
		(float*)densityValues.data()
	);

	// create density nodes
	auto densityGridNode = frw::GridNode(context()->GetMaterialSystem());
	densityGridNode.SetGrid(frw::VolumeGrid(volumeData.m_densityGrid, Context()));

	auto emissionGridNode = frw::GridNode(context()->GetMaterialSystem());
	emissionGridNode.SetGrid(frw::VolumeGrid(volumeData.m_emissionGrid, Context()));
	auto emissionLookupNode = CreateLookupTextureNode(this, volumeData.m_emissionLookup, emissionGridNode);

	auto albedoGridNode = frw::GridNode(context()->GetMaterialSystem());
	albedoGridNode.SetGrid(frw::VolumeGrid(volumeData.m_albedoGrid, Context()));
	auto albedoLookupNode = CreateLookupTextureNode(this, volumeData.m_albedoLookup, albedoGridNode);
	
	// apply nodes to main shader
	volumeShader.xSetValue(RPR_MATERIAL_INPUT_DENSITYGRID, densityGridNode);
	volumeShader.xSetParameterF(RPR_MATERIAL_INPUT_DENSITY, 1000.f, 1.f, 1.f, 1.f); // multiplication coefficient
	volumeShader.xSetValue(RPR_MATERIAL_INPUT_COLOR, albedoLookupNode);
	volumeShader.xSetValue(RPR_MATERIAL_INPUT_EMISSION, emissionLookupNode);

	m_boundingBoxMesh.SetVolumeShader(volumeShader);

	return m_boundingBoxMesh.IsValid();
}
