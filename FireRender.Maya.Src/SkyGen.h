/*********************************************************************************************************************************
* Radeon ProRender for Maya plugin
* Copyright (c) 2017 AMD
* All Rights Reserved
*
* Generate a procedural sky based on the physical model of Lukas Hosek and Alexander Wilkie
*
* Part of this code is derived from the work of Matt Pharr, Greg Humphreys, and Wenzel Jakob.
*
* pbrt source code is Copyright(c) 1998-2016
* Matt Pharr, Greg Humphreys, and Wenzel Jakob.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*
* - Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
*
* - Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
* IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
* TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
* PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
* HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*********************************************************************************************************************************/

#pragma once

#if defined(MAX_PLUGIN)
	#include <max.h>
#elif defined(MAYA_PLUGIN)
	#include <maya/MFloatVector.h>
	#include <maya/MColor.h>
#endif

#include <vector>
#include <ctime>
#include <math.h>

// sky should not be too powerful
#define GLOBAL_SCALE 0.3f

#ifndef PI
#define PI 3.14159265358979323846
#endif

struct SkyRgbFloat32
{
	float r, g, b;
	SkyRgbFloat32() : r(0), g(0), b(0) {};

	SkyRgbFloat32(const float &ir, const float &ig, const float &ib)
		: r(ir), g(ig), b(ib) {};

	inline float & operator [] (int c)
	{
		return (c == 0) ? r : (c == 1) ? g : b;
	}
#pragma warning(disable: 4244)
	SkyRgbFloat32 operator * (double v) const { return SkyRgbFloat32(r * v, g * v, b * v); }
	SkyRgbFloat32 operator + (double v) const { return SkyRgbFloat32(r + v, g + v, b + v); }
#pragma warning(default: 4244)
};

struct SkyColor
{
	double r, g, b;
	SkyColor() : r(0), g(0), b(0) {}
	SkyColor(double r, double g, double b) : r(r), g(g), b(b) {}

	operator SkyRgbFloat32()
	{
		return SkyRgbFloat32(float(r), float(g), float(b));
	}

	void sanitize()
	{
		if (isnan(r))
			r = 0;
		if (isnan(g))
			g = 0;
		if (isnan(b))
			b = 0;

		r = fmin(10000, fmax(0, r));
		g = fmin(10000, fmax(0, g));
		b = fmin(10000, fmax(0, b));
	}

#if defined(MAX_PLUGIN)
	Color asColor() { return Color(static_cast<float>(r), static_cast<float>(g), static_cast<float>(b)); }
	SkyColor& operator = (const Color& v) { r = v.r; g = v.g; b = v.b; return *this; }
#elif defined(MAYA_PLUGIN)
	MColor asColor() { return MColor(static_cast<float>(r), static_cast<float>(g), static_cast<float>(b)); }
	SkyColor& operator = (const MColor& v) { r = v.r; g = v.g; b = v.b; return *this; }
#endif

	SkyColor operator * (double v) const { return SkyColor(r * v, g * v, b * v); }
	SkyColor operator / (double v) const { return SkyColor(r / v, g / v, b / v); }
	SkyColor operator * (const SkyColor& v) const { return SkyColor(r * v.r, g * v.g, b * v.b); }
	SkyColor operator / (const SkyColor& v) const { return SkyColor(r / v.r, g / v.g, b / v.b); }
	SkyColor operator + (double v) const { return SkyColor(r + v, g + v, b + v); }
	SkyColor operator - (double v) const { return SkyColor(r - v, g - v, b - v); }
	SkyColor operator + (const SkyColor& v) const { return SkyColor(r + v.r, g + v.g, b + v.b); }
	SkyColor operator - (const SkyColor& v) const { return SkyColor(r - v.r, g - v.g, b - v.b); }
	SkyColor& operator *= (double v) { r *= v; g *= v; b *= v; return *this; }
	SkyColor& operator /= (double v) { r /= v; g /= v; b /= v; return *this; }
	SkyColor& operator *= (const SkyColor& v) { r *= v.r; g *= v.g; b *= v.b; return *this; }
	SkyColor& operator /= (const SkyColor& v) { r /= v.r; g /= v.g; b /= v.b; return *this; }
	SkyColor& operator += (double v) { r += v; g += v; b += v; return *this; }
	SkyColor& operator -= (double v) { r -= v; g -= v; b -= v; return *this; }
	SkyColor& operator += (const SkyColor& v) { r += v.r; g += v.g; b += v.b; return *this; }
	SkyColor& operator -= (const SkyColor& v) { r -= v.r; g -= v.g; b -= v.b; return *this; }
};



#ifdef MAYA_PLUGIN

// Define wrapper from MFloatVector to Max's Point3 class.
class Point3 : public MFloatVector
{
public:
	Point3()
	{}
	Point3(float x, float y, float z)
		: MFloatVector(x, y, z)
	{}
	Point3& operator=(const MFloatVector& other)
	{
		MFloatVector::operator=(other);
		return *this;
	}
	float Length()
	{
		return length();
	}
	Point3 Normalize() const
	{
		Point3 Result = *this;
		Result.normalize();
		return Result;
	}
	friend inline float DotProd(const Point3& A, const Point3& B)
	{
		return A * B;
	}
};

class Point2
{
public:
	Point2()
	{}
	Point2(float inX, float inY)
		: x(inX)
		, y(inY)
	{}

	float	x;
	float	y;
};

#endif // MAYA_PLUGIN

class SkyGen
{
public:
	bool mOn = true;
	double mMultiplier = 0.23;
	SkyColor mFilterColor = SkyColor(0.0, 0.0, 0.0);
	double mSaturation = 1.0;
	SkyColor mGroundColor = SkyColor(0.4, 0.4, 0.4);
	Point3 mSunDirection = Point3(0.0f, 0.3f, 0.4f);
	double mSunIntensity = 0.01;
	double mSunScale = 0.5;

	SkyColor mGroundAlbedo;
	float mTurbidity = 3.f;
#pragma warning(disable: 4305)
	float mElevation = 10.f * PI / 180.f;
#pragma warning(default: 4305)

private:

	void AdjustColor(SkyRgbFloat32& Color, const double& Saturation, const SkyColor& FilterColor)
	{
#pragma warning(disable: 4244)
		SkyColor luminance_weight = SkyColor(0.212671, 0.715160, 0.072169);
		double intensity = (Color.r * luminance_weight.r + Color.g * luminance_weight.g + Color.b * luminance_weight.b);
		if (Color.r < 0.0) Color.r = 0.0;
		if (Color.g < 0.0) Color.g = 0.0;
		if (Color.b < 0.0) Color.b = 0.0;
		if (Saturation <= 0.0)
		{
			Color.r = Color.g = Color.b = intensity;
		}
		else
		{
			Color = Color * Saturation + intensity * (1.0 - Saturation);
			if (Saturation > 1.0)
			{
				if (Color.r < 0.0) Color.r = 0.0;
				if (Color.g < 0.0) Color.g = 0.0;
				if (Color.b < 0.0) Color.b = 0.0;
			}
		}

		Color.r *= 1.0 + FilterColor.r;
		Color.g *= 1.0 + FilterColor.g;
		Color.b *= 1.0 + FilterColor.b;
#pragma warning(default: 4244)
	}

public:
	void GenerateSkyHosek(int w, int h, SkyRgbFloat32 *buffer, float maxIntensity);
};
