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
    bool IsDisplacementSupported() const override { return false; }

    bool IsHairSupported() const override { return true; }
    bool IsVolumeSupported() const override { return true; }

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

    bool IsAOVSupported(int aov) const override;

protected:
    virtual rpr_int CreateContextInternal(rpr_creation_flags createFlags, rpr_context* pContext) override;

private:
    static rpr_int m_gHybridProPluginID;
};
