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

#include <functional>

#include "RenderRegion.h"
#include "FireRenderAOV.h"

class FireRenderContext;

enum class TileRenderFillType
{
	Normal = 0

	// _TODO Add Different algorithms, i.e spiral etc
};

struct TileRenderInfo
{
	unsigned int totalWidth;
	unsigned int totalHeight;

	unsigned int tileSizeX;
	unsigned int tileSizeY;

	TileRenderFillType tilesFillType;
};

typedef std::function<bool(RenderRegion&, int, AOVPixelBuffers& out)> TileRenderingCallback;

class TileRenderer
{
public:
	TileRenderer();
	~TileRenderer();

	void Render(FireRenderContext& renderContext, const TileRenderInfo& info, AOVPixelBuffers& outBuffer, TileRenderingCallback callbackFunc);
};

