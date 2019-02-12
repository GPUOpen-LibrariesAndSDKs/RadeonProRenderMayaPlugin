#pragma once
//
// Copyright (C) AMD
//
// File: FireRenderContextIFace.h
//
// Abstract interface class for FireRenderContext class
//

enum class RenderType;

class IFireRenderContextInfo
{
public:
	virtual RenderType GetRenderType(void) const = 0;
	virtual bool ShouldResizeTexture(unsigned int& width, unsigned int& height) const = 0;
};

