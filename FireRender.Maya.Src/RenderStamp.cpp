// Formatting function from this cpp file is heavily based on RenderStamp functionality from RPR 3ds Max plugin.
// It is worth keeping both implementation in sync. Relevant functions:
// - GetFriendlyUsedGPUName()
// - FormatRenderStamp() ~ ProductionRenderCore::RenderStamp in 3ds Max

#include "RenderStamp.h"


#include <maya/MRenderView.h> // for RV_PIXEL declaration
#include <maya/MIntArray.h>
#include <maya/MGlobal.h>

#ifdef WIN32
//
#elif defined(OSMac_)
#include <unistd.h>
#include <sys/sysctl.h>
#elif defined(__linux__)
#include <sys/utsname.h>
#endif


#include "common.h"
#include "Logger.h"
#include "FireRenderUtils.h"

// We are using bitmap fonts to render text. This is the easiest cross-platform way of drawing texts.
#include "RenderStampFont.h"
#include "RenderStampUtils.h"

namespace FireMaya
{
	//-------------------------------------------------------------------------------------------------
	// Text rendering functions
	//-------------------------------------------------------------------------------------------------

	static float lerp(float A, float B, float Alpha)
	{
		return A * (1 - Alpha) + B * Alpha;
	}

	// Use structure to avoid sending lots of common data to BlitBitmap
	struct BlitData
	{
		RV_PIXEL*	dstBitmap;
		int			dstWidth;
		int			dstHeight;
		bool		flip;
		const unsigned char* srcBitmap;
		int			srcWidth;
		int			srcHeight;
	};

	void BlitBitmap(const BlitData& data, int dstX, int dstY, int srcX, int srcY, int w, int h, float alpha = 1.0f)
	{
		// clamp data
		if (dstX >= data.dstWidth || dstY >= data.dstHeight)
			return;
		if (dstX + w >= data.dstWidth)
			w = data.dstWidth - dstX;
		if (dstY + h >= data.dstHeight)
			h = data.dstHeight - dstY;

		// compute loop parameters
		int stride, pos;
		if (!data.flip)
		{
			stride = data.dstWidth - w;
			pos = dstY * data.dstWidth + dstX;
		}
		else
		{
			stride = -(data.dstWidth + w);
			pos = (data.dstHeight - dstY - 1) * data.dstWidth + dstX;
		}

		// blend source bitmap to destination bitmap
		const unsigned char* srcPic = &data.srcBitmap[data.srcWidth * srcY + srcX];
		for (int y = 0; y < h; y++)
		{
			for (int x = 0; x < w; x++)
			{
				unsigned char c = *srcPic;
				float v = c / 255.0f;
				RV_PIXEL* p = data.dstBitmap + pos;
				if (alpha == 1.0f)
				{
					p->r = p->g = p->b = v;
				}
				else
				{
					p->r = lerp(p->r, v, alpha);
					p->g = lerp(p->g, v, alpha);
					p->b = lerp(p->b, v, alpha);
				}
				pos++;
				srcPic++;
			}
			pos += stride;
			srcPic += data.srcWidth - w;
		}
	}

	static void GetTextExtent(const char* text, int& width, int& height)
	{
		height = DlgFont::CHAR_HEIGHT;
		width = 0;
		while (char c = *text++)
		{
			if (c < DlgFont::FONT_FIRST_CHAR || c >= 128)
				c = '?';
			const unsigned char* pos = DlgFont::CHAR_LOCATIONS + (c - DlgFont::FONT_FIRST_CHAR) * 3;
			width += pos[2];
		}
	}

	static void GetCharRect(char c, int& x, int& y, int& w, int& h)
	{
		if (c < DlgFont::FONT_FIRST_CHAR || c >= 128)
			c = '?';
		const unsigned char* pos = DlgFont::CHAR_LOCATIONS + (c - DlgFont::FONT_FIRST_CHAR) * 3;
		x = pos[0];
		y = pos[1];
		w = pos[2];
		h = DlgFont::CHAR_HEIGHT;
	}

	void RenderStamp::AddRenderStamp(FireRenderContext& context, RV_PIXEL* pixels, int width, int height, bool flip, const char* format)
	{
		if (!format || !format[0])
			return;

		std::string text = RenderStampUtils::FormatRenderStamp(context, format);

		// prepare common blit data
		BlitData data;
		data.dstBitmap = pixels;
		data.dstWidth = width;
		data.dstHeight = height;
		data.flip = flip;
		data.srcBitmap = DlgFont::TEX_DATA;
		data.srcWidth = DlgFont::TEX_WIDTH;
		data.srcHeight = DlgFont::TEX_HEIGHT;

		// compute location of stamp
		int stampWidth, stampHeight;
		const char* pText = text.c_str();
		GetTextExtent(pText, stampWidth, stampHeight);
		int stampX = width - stampWidth;
		if (stampX < 0) stampX = 0;
		int stampY = height - stampHeight;
		if (stampY < 0) stampY = 0;

		// draw characters
		while (char c = *pText++)
		{
			int x, y, w, h;
			GetCharRect(c, x, y, w, h);
			BlitBitmap(data, stampX, stampY, x, y, w, h, 0.5f);
			stampX += w;
			if (stampX >= width) break; // text is too long
		}
	}

}
