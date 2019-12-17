#pragma once
#include "FireRenderContext.h"

class ContextCreator
{
public:
	static FireRenderContextPtr CreateAppropriateContextForRenderQuality(RenderQuality quality);
	static FireRenderContextPtr CreateAppropriateContextForRenderType(RenderType renderType);

private:
	static FireRenderContextPtr CreateHybridContext();
	static FireRenderContextPtr CreateTahoeContext();
};
