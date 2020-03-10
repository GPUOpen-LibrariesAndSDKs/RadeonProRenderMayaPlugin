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
#include "FireRenderViewportUI.h"

using namespace MHWRender;

// -----------------------------------------------------------------------------
FireRenderViewportUI::FireRenderViewportUI(const MString& name) :
	MSceneRender(name)
{
}

// -----------------------------------------------------------------------------
FireRenderViewportUI::~FireRenderViewportUI()
{
}

// -----------------------------------------------------------------------------
MSceneRender::MSceneFilterOption FireRenderViewportUI::renderFilterOverride()
{
	// Only render UI elements.
	return MSceneRender::kRenderNonShadedItems;
}

// -----------------------------------------------------------------------------
MSceneRender::MObjectTypeExclusions FireRenderViewportUI::objectTypeExclusions()
{
	// Exclude elements that are not required.
	return (MSceneRender::MObjectTypeExclusions)(
		kExcludeNurbsCurves | kExcludeNurbsSurfaces | kExcludeMeshes |
		kExcludePlanes | kExcludeLights | kExcludeCameras | kExcludeJoints |
		kExcludeIkHandles | kExcludeDeformers | kExcludeDynamics | kExcludeParticleInstancers |
		kExcludeLocators | kExcludeDimensions |
		kExcludeTextures | kExcludeGrid | kExcludeCVs | kExcludeHulls |
		kExcludeStrokes | kExcludeSubdivSurfaces | kExcludeFluids | kExcludeFollicles |
		kExcludeHairSystems | kExcludeImagePlane | kExcludeNCloths | kExcludeNRigids |
		kExcludeDynamicConstraints | kExcludeNParticles | kExcludeMotionTrails 
#ifndef MAYA2015
		| kExcludeHoldOuts
#endif
		);
}

// -----------------------------------------------------------------------------
MClearOperation& FireRenderViewportUI::clearOperation()
{
	// Clearing is not required as it would also clear
	// the blitted RPR texture from the previous step.
	mClearOperation.setMask(static_cast<unsigned int>(MClearOperation::kClearNone));
	return mClearOperation;
}
