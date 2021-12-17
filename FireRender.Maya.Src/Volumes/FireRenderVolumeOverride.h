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
#pragma once

#include <maya/MFnDependencyNode.h>
#include <maya/MNodeMessage.h>
#include <maya/MPxGeometryOverride.h>

/** The override is required for drawing to Maya's Viewport 2.0. */
class FireRenderVolumeOverride : public MHWRender::MPxGeometryOverride
{
public:
	static MHWRender::MPxGeometryOverride* Creator(const MObject& obj)
	{
		return new FireRenderVolumeOverride(obj);
	}

	virtual ~FireRenderVolumeOverride();

	virtual MHWRender::DrawAPI supportedDrawAPIs() const override;

	virtual bool hasUIDrawables() const override;
	virtual void addUIDrawables(const MDagPath &path, MHWRender::MUIDrawManager &drawManager, const MHWRender::MFrameContext &frameContext) override;

	virtual void updateDG() override;
	virtual bool isIndexingDirty(const MHWRender::MRenderItem &item) override { return m_changed; }
	virtual bool isStreamDirty(const MHWRender::MVertexBufferDescriptor &desc) override { return m_changed; }
	virtual void updateRenderItems(const MDagPath &path, MHWRender::MRenderItemList& list) override;
	virtual void populateGeometry(const MHWRender::MGeometryRequirements &requirements, const MHWRender::MRenderItemList &renderItems, MHWRender::MGeometry &data) override;
	virtual void cleanUp() override {};

	virtual bool refineSelectionPath(const MHWRender::MSelectionInfo &  	selectInfo,
		const MHWRender::MRenderItem &  	hitItem,
		MDagPath &  	path,
		MObject &  	components,
		MSelectionMask &  	objectMask
	) override;

	virtual void updateSelectionGranularity(
		const MDagPath& path,
		MHWRender::MSelectionContext& selectionContext) override;

private:
	FireRenderVolumeOverride(const MObject& obj);

	//void CreateGizmoGeometry(GizmoVertexVector& vertexVector, IndexVector& indexVector, const MDagPath& dagPath);
	MColor GetColor(const MDagPath& path);

private:
	struct GridSizeValues
	{
		short nx, ny, nz;
	} m_GridSizeValues;

	struct VoxelSizeValues
	{
		double nx, ny, nz;
	} m_VoxelSizeValues;

	MFnDependencyNode m_depNodeObj;

	bool m_changed;

	static MColor m_SelectedColor;
	static MColor m_Color;
};
