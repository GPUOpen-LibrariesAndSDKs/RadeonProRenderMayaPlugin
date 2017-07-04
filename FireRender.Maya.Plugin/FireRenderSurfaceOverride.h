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
