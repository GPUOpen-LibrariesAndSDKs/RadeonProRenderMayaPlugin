#pragma once

#include "FireRenderContext.h"

class HybridContext : public FireRenderContext
{
public:
	HybridContext();

	static rpr_int GetPluginID();
	void setupContext(const FireRenderGlobalsData& fireRenderGlobalsData, bool disableWhiteBalance = false) override;

	bool IsAOVSupported(int aov) const override;

	rpr_int SetRenderQuality(RenderQuality quality) override;

	FireRenderEnvLight* CreateEnvLight(const MDagPath& dagPath) override;
	FireRenderSky* CreateSky(const MDagPath& dagPath) override;

	bool IsRenderRegionSupported() const override { return false; }

	bool IsRenderQualitySupported(RenderQuality quality) const override;

protected:
	rpr_int CreateContextInternal(rpr_creation_flags createFlags, rpr_context* pContext) override;
	void updateTonemapping(const FireRenderGlobalsData&, bool disableWhiteBalance) override;

	bool IsGLInteropEnabled() const override;

private:
	static rpr_int m_gHybridPluginID;
};
