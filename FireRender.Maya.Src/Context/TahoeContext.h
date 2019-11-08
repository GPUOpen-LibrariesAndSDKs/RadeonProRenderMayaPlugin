#pragma once

#include "FireRenderContext.h"

class TahoeContext : public FireRenderContext
{
public:
	TahoeContext();

	static rpr_int GetPluginID();

	void setupContext(const FireRenderGlobalsData& fireRenderGlobalsData, bool disableWhiteBalance = false) override;

	bool IsRenderQualitySupported(RenderQuality quality) const override;

protected:
	rpr_int CreateContextInternal(rpr_creation_flags createFlags, rpr_context* pContext) override;

	void updateTonemapping(const FireRenderGlobalsData&, bool disableWhiteBalance) override;

	bool needResolve() const override;

private:
	static rpr_int m_gTahoePluginID;
};
