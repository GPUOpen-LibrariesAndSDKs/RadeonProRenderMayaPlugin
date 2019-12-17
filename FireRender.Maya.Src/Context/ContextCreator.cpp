#include "ContextCreator.h"

#include "HybridContext.h"
#include "TahoeContext.h"


FireRenderContextPtr ContextCreator::CreateHybridContext()
{
	return std::make_shared<HybridContext>();
}

FireRenderContextPtr ContextCreator::CreateTahoeContext()
{
	return std::make_shared<TahoeContext>();
}

FireRenderContextPtr ContextCreator::CreateAppropriateContextForRenderQuality(RenderQuality quality)
{
#ifdef WIN32
	if (quality == RenderQuality::RenderQualityFull)
	{
		return CreateTahoeContext();
	}
	else
	{
		return CreateHybridContext();
	}
#endif

	return CreateTahoeContext();
}

FireRenderContextPtr ContextCreator::CreateAppropriateContextForRenderType(RenderType renderType)
{
	return CreateAppropriateContextForRenderQuality(GetRenderQualityForRenderType(renderType));
}
