#pragma once

#include <maya/MPxShaderOverride.h>
#include <maya/MShaderManager.h>

class FireRenderBlendOverride : public MHWRender::MPxShaderOverride
{
public:
	FireRenderBlendOverride(const MObject& obj);
	virtual ~FireRenderBlendOverride();

	static MHWRender::MPxShaderOverride* creator(const MObject& obj);

	virtual MString initialize(const MInitContext& initContext, MInitFeedback& initFeedback) override;
	virtual MHWRender::MShaderInstance* shaderInstance() const override;
#ifndef MAYA2015
	virtual MHWRender::MShaderInstance* nonTexturedShaderInstance(bool &monitorNode) const override;
#endif
	virtual void updateDG(MObject object) override;
	virtual void updateDevice() override;
	virtual void endUpdate() override;

	virtual bool handlesDraw(MHWRender::MDrawContext& context) override;
	virtual void activateKey(MHWRender::MDrawContext& context, const MString& key) override;
	virtual bool draw(MHWRender::MDrawContext& context, const MHWRender::MRenderItemList& renderItemList) const override;
	virtual void terminateKey(MHWRender::MDrawContext& context, const MString& key) override;

	virtual MHWRender::DrawAPI supportedDrawAPIs() const override;
	virtual bool isTransparent() override;
	virtual bool overridesDrawState() override;
#ifndef MAYA2015
	virtual bool overridesNonMaterialItems() const override;
#endif

private:
	MHWRender::MShaderInstance *mShaderInstance;
	bool m_shaderBound;
	MObject m_shader;
	MString m_fragmentName;
};
