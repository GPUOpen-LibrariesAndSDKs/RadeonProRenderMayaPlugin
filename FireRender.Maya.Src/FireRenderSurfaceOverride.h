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

#include <maya/MShaderManager.h>
#include <maya/MPxShadingNodeOverride.h>
#include <maya/MPxSurfaceShadingNodeOverride.h>

class FireRenderMaterialNodeOverride : public MHWRender::MPxSurfaceShadingNodeOverride
{
public:
	FireRenderMaterialNodeOverride(const MObject& obj);
	virtual ~FireRenderMaterialNodeOverride();

	static MHWRender::MPxSurfaceShadingNodeOverride* creator(const MObject& obj);

	virtual MHWRender::DrawAPI supportedDrawAPIs() const override;
	virtual bool allowConnections() const override;

	virtual MString fragmentName() const override;

	// Commented because of AMDMAYA-634
	//virtual void getCustomMappings(MHWRender::MAttributeParameterMappingList& mappings) override;

	MString primaryColorParameter() const override;
	MString transparencyParameter() const override;

	virtual bool valueChangeRequiresFragmentRebuild(const MPlug* plug) const override;

	virtual void updateDG() override;
	virtual void updateShader(MHWRender::MShaderInstance& shader, const MHWRender::MAttributeParameterMappingList& mappings) override;

	static const char* className();

private:
	int getType() const;

	MHWRender::MShaderInstance *mShaderInstance;
	MObject m_shader;

	int m_type;
};

class FireRenderStandardMaterialNodeOverride : public MHWRender::MPxSurfaceShadingNodeOverride
{
public:
	FireRenderStandardMaterialNodeOverride(const MObject& obj);
	virtual ~FireRenderStandardMaterialNodeOverride();

	static MHWRender::MPxSurfaceShadingNodeOverride* creator(const MObject& obj);

	virtual MHWRender::DrawAPI supportedDrawAPIs() const override;
	virtual bool allowConnections() const override;

	virtual MString fragmentName() const override;

	virtual void getCustomMappings(MHWRender::MAttributeParameterMappingList& mappings) override;

	MString primaryColorParameter() const override;
	MString transparencyParameter() const override;

	virtual bool valueChangeRequiresFragmentRebuild(const MPlug* plug) const override;

	virtual void updateDG() override;
	virtual void updateShader(MHWRender::MShaderInstance& shader, const MHWRender::MAttributeParameterMappingList& mappings) override;

	static const char* className();

private:
	MObject m_shader;
};
