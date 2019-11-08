#pragma once
#include "FireRenderContext.h"

class ContextCreator
{
public:
	static FireRenderContextPtr CreateHybridContext();
	static FireRenderContextPtr CreateTahoeContext();

	static FireRenderContextPtr CreateAppropriateContextForRenderQuality(RenderQuality quality);
	static FireRenderContextPtr CreateAppropriateContextForRenderType(RenderType renderType);
};
