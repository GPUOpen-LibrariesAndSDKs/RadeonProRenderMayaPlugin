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

class HybridContext : public FireRenderContext
{
public:
	HybridContext();

	static rpr_int GetPluginID();
	void setupContextPostSceneCreation(const FireRenderGlobalsData& fireRenderGlobalsData, bool disableWhiteBalance = false) override;

	bool IsAOVSupported(int aov) const override;

	rpr_int SetRenderQuality(RenderQuality quality) override;

	FireRenderEnvLight* CreateEnvLight(const MDagPath& dagPath) override;
	FireRenderSky* CreateSky(const MDagPath& dagPath) override;

	bool IsRenderRegionSupported() const override { return false; }

	bool IsRenderQualitySupported(RenderQuality quality) const override;

	bool IsDenoiserSupported() const override { return false; }
	bool IsDisplacementSupported() const override { return false; }

	bool IsHairSupported() const override { return false; }
	bool IsVolumeSupported() const override { return false; }

	bool IsPhysicalLightTypeSupported(PLType lightType) const override;

	bool IsShaderSupported(frw::ShaderType type) const override;
	bool IsShaderNodeSupported(FireMaya::ShaderNode* shaderNode) const override;
	frw::Shader GetDefaultColorShader(frw::Value color) override;

	virtual bool IsMaterialNodeIDSupported() const { return false; }
	virtual bool IsMeshObjectIDSupported() const { return false; }
	virtual bool IsContourModeSupported() const { return false; }
	virtual bool IsCameraSetExposureSupported() const { return false; }
	virtual bool IsShadowColorSupported() const { return false; }

	virtual bool IsUberReflectionDielectricSupported() const { return false; }
	virtual bool IsUberRefractionAbsorbtionColorSupported() const { return false; }
	virtual bool IsUberRefractionAbsorbtionDistanceSupported() const { return false; }
	virtual bool IsUberRefractionCausticsSupported() const { return false; }
	virtual bool IsUberSSSWeightSupported() const { return false; }
	virtual bool IsUberSheenWeightSupported() const { return false; }
	virtual bool IsUberBackscatterWeightSupported() const { return false; }
	virtual bool IsUberShlickApproximationSupported() const { return false; }
	virtual bool IsUberCoatingThicknessSupported() const { return false; }
	virtual bool IsUberCoatingTransmissionColorSupported() const { return false; }
	virtual bool IsUberReflectionNormalSupported() const { return false; }
	virtual bool IsUberScaleSupported() const { return false; }

protected:
	virtual rpr_int CreateContextInternal(rpr_creation_flags createFlags, rpr_context* pContext) override;
	void updateTonemapping(const FireRenderGlobalsData&, bool disableWhiteBalance) override;

	bool IsGLInteropEnabled() const override;

private:
	static rpr_int m_gHybridPluginID;
};
