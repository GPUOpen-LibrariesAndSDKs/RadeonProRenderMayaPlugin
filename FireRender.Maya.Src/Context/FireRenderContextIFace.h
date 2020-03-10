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

