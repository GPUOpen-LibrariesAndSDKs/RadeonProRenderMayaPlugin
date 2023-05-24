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

#include "HybridContext.h"

class HybridProContext : public HybridContext
{
public:
    HybridProContext();

    static rpr_int GetPluginID();

    bool IsRenderQualitySupported(RenderQuality quality) const override;

    bool IsRenderRegionSupported() const override { return false; }

    bool IsDenoiserSupported() const override { return false; }
    bool IsDisplacementSupported() const override { return true; }

    bool IsHairSupported() const override { return true; }
    bool IsVolumeSupported() const override { return true; }
    bool IsNorthstarVolumeSupported() const { return true; }
    bool IsShaderNodeSupported(FireMaya::ShaderNode* shaderNode) const override;

    virtual void setupContextHybridParams(const FireRenderGlobalsData& fireRenderGlobalsData);

    virtual bool IsMaterialNodeIDSupported() const { return false; }
    virtual bool IsMeshObjectIDSupported() const { return false; }
    virtual bool IsContourModeSupported() const { return false; }
    virtual bool IsCameraSetExposureSupported() const { return false; }
    virtual bool IsShadowColorSupported() const { return false; }

    virtual bool ShouldUseNoSubdivDisplacement() const override { return true; }

    virtual bool IsUberReflectionDielectricSupported() const { return true; }
    virtual bool IsUberRefractionAbsorbtionColorSupported() const { return true; }
    virtual bool IsUberRefractionAbsorbtionDistanceSupported() const { return true; }
    virtual bool IsUberRefractionCausticsSupported() const { return true; }
    virtual bool IsUberSSSWeightSupported() const { return true; }
    virtual bool IsUberSheenWeightSupported() const { return true; }
    virtual bool IsUberBackscatterWeightSupported() const { return true; }
    virtual bool IsUberShlickApproximationSupported() const { return true; }
    virtual bool IsUberCoatingThicknessSupported() const { return true; }
    virtual bool IsUberCoatingTransmissionColorSupported() const { return true; }
    virtual bool IsUberReflectionNormalSupported() const { return true; }
    virtual bool IsUberScaleSupported() const { return true; }

    virtual int GetAOVMaxValue() const override;

    void setupContextPostSceneCreation(const FireRenderGlobalsData& fireRenderGlobalsData, bool disableWhiteBalance = false) override;

    bool IsAOVSupported(int aov) const override;

protected:
    virtual rpr_int CreateContextInternal(rpr_creation_flags createFlags, rpr_context* pContext) override;

private:
    static rpr_int m_gHybridProPluginID;
};
