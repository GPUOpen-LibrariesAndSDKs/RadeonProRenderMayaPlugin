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

enum class RenderType;
enum class RenderQuality;

namespace frw
{
	enum ShaderType;
	class Light;
}

namespace FireMaya
{
	class ShaderNode;
}

class IFireRenderContextInfo
{
public:
	virtual RenderType GetRenderType(void) const = 0;
	virtual bool ShouldResizeTexture(unsigned int& width, unsigned int& height) const = 0;

	virtual bool IsRenderQualitySupported(RenderQuality quality) const = 0;
	virtual bool IsRenderRegionSupported() const = 0;
	virtual bool IsDenoiserSupported() const = 0;
	virtual bool IsDisplacementSupported() const = 0;
	virtual bool IsHairSupported() const = 0;
	virtual bool IsVolumeSupported() const = 0;
	virtual bool ShouldForceRAMDenoiser() const = 0;
	virtual bool ShouldUseNoSubdivDisplacement() const = 0;

	virtual bool IsShaderSupported(frw::ShaderType) const = 0;
	virtual bool IsShaderNodeSupported(FireMaya::ShaderNode* shaderNode) const = 0;
	virtual bool IsShadowColorSupported() const = 0;
	
	virtual frw::Shader GetDefaultColorShader(frw::Value color) = 0;

	virtual bool IsUberReflectionDielectricSupported() const = 0;
	virtual bool IsUberRefractionAbsorbtionColorSupported() const = 0;
	virtual bool IsUberRefractionAbsorbtionDistanceSupported() const = 0;
	virtual bool IsUberRefractionCausticsSupported() const = 0;
	virtual bool IsUberSSSWeightSupported() const = 0;
	virtual bool IsUberSheenWeightSupported() const = 0;
	virtual bool IsUberBackscatterWeightSupported() const = 0;
	virtual bool IsUberShlickApproximationSupported() const = 0;
	virtual bool IsUberCoatingThicknessSupported() const = 0;
	virtual bool IsUberCoatingTransmissionColorSupported() const = 0;
	virtual bool IsUberReflectionNormalSupported() const = 0;
	virtual bool IsUberScaleSupported() const = 0;

	virtual bool IsGLTFExport() const = 0;
};
