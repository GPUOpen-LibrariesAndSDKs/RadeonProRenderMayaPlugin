#pragma once

#include <string>
// Forward declarations
class FireRenderContext;
struct RV_PIXEL;

namespace FireMaya
{
	class RenderStamp
	{
	public:
		void AddRenderStamp(FireRenderContext& context, RV_PIXEL* pixels, int width, int height, bool flip, const char* format);
	};

}