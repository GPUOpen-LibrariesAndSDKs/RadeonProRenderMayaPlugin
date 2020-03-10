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

#include <maya/MViewport2Renderer.h>

/** A scene render that draws UI elements such as the resolution gate. */
class FireRenderViewportUI : public MHWRender::MSceneRender
{

public:

	// Life Cycle
	// -----------------------------------------------------------------------------

	FireRenderViewportUI(const MString& name);
	virtual ~FireRenderViewportUI();


	// MSceneRender Implementation
	// -----------------------------------------------------------------------------

	/** Filter the scene to show only UI elements. */
	virtual MHWRender::MSceneRender::MSceneFilterOption renderFilterOverride();

	/** Exclude object types that shouldn't be drawn. */
	virtual MHWRender::MSceneRender::MObjectTypeExclusions objectTypeExclusions();

	/** Disable clear. */
	virtual MHWRender::MClearOperation& clearOperation();
};
