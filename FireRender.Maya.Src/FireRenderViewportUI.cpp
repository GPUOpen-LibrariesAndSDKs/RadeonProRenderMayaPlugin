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
