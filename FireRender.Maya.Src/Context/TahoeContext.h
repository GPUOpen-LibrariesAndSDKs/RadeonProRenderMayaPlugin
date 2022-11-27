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

#include "FireRenderContext.h"

#include <map>

class NorthStarContext : public FireRenderContext
{
public:
	NorthStarContext();

	static rpr_int GetPluginID();
	static bool IsGivenContextNorthStar(const FireRenderContext* pContext);

	void setupContextContourMode(const FireRenderGlobalsData& fireRenderGlobalsData, int createFlags, bool disableWhiteBalance) override;
	void setupContextPostSceneCreation(const FireRenderGlobalsData& fireRenderGlobalsData, bool disableWhiteBalance = false) override;
	void setupContextAirVolume(const FireRenderGlobalsData& fireRenderGlobalsData) override;
	void setupContextCryptomatteSettings(const FireRenderGlobalsData& fireRenderGlobalsData) override;

	bool IsRenderQualitySupported(RenderQuality quality) const override;

	virtual bool IsDenoiserSupported() const override;
	virtual bool IsDisplacementSupported() const override;
	virtual bool IsHairSupported() const override;
	virtual bool IsVolumeSupported() const override;
	virtual bool IsNorthstarVolumeSupported() const override;
	virtual bool ShouldForceRAMDenoiser() const override;

	virtual bool IsAOVSupported(int aov) const override;

	virtual bool IsPhysicalLightTypeSupported(PLType lightType) const override;

	virtual bool MetalContextAvailable() const override;

	virtual bool IsDeformationMotionBlurEnabled() const override;

	virtual void SetRenderUpdateCallback(RenderUpdateCallback callback, void* data) override;
	virtual void SetSceneSyncFinCallback(RenderUpdateCallback callback, void* data) override;
	virtual void SetFirstIterationCallback(RenderUpdateCallback callback, void* data) override;
	virtual void SetRenderTimeCallback(RenderUpdateCallback callback, void* data) override;
	virtual void AbortRender() override;

protected:
	rpr_int CreateContextInternal(rpr_creation_flags createFlags, rpr_context* pContext) override;

	void updateTonemapping(const FireRenderGlobalsData&, bool disableWhiteBalance) override;

	bool needResolve() const override;

	virtual void OnPreRender() override;

	virtual int GetAOVMaxValue() override;

private:
	static rpr_int m_gTahoePluginID;
	
	bool m_PreviewMode;
};

typedef std::shared_ptr<NorthStarContext> NorthStarContextPtr;
