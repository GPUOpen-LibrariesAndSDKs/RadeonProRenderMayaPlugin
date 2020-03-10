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
