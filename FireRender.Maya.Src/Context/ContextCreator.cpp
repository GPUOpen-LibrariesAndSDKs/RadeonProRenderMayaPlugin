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
#include "ContextCreator.h"

#include "HybridContext.h"
#include "TahoeContext.h"


FireRenderContextPtr ContextCreator::CreateHybridContext()
{
	return std::make_shared<HybridContext>();
}

FireRenderContextPtr ContextCreator::CreateTahoeContext(TahoePluginVersion version)
{
	std::shared_ptr<TahoeContext> instance = std::make_shared<TahoeContext>();
	instance->SetPluginEngine(version);

	return instance;
}

FireRenderContextPtr ContextCreator::CreateAppropriateContextForRenderQuality(RenderQuality quality)
{
#ifdef WIN32
	if (quality == RenderQuality::RenderQualityFull)
	{
		return CreateTahoeContext(GetTahoeVersionToUse());
	}
	else
	{
		return CreateHybridContext();
	}
#endif

	return CreateTahoeContext(GetTahoeVersionToUse());
}

FireRenderContextPtr ContextCreator::CreateAppropriateContextForRenderType(RenderType renderType)
{
	return CreateAppropriateContextForRenderQuality(GetRenderQualityForRenderType(renderType));
}
