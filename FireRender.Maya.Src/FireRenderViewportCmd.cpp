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
#include "FireRenderViewportCmd.h"
#include "FireRenderViewport.h"
#include "FireRenderViewportManager.h"
#include <maya/MRenderView.h>

#include <vector>
#include <functional>

using namespace std;

FireRenderViewportCmd::FireRenderViewportCmd()
{
}

FireRenderViewportCmd::~FireRenderViewportCmd()
{
}

void * FireRenderViewportCmd::creator()
{
	return new FireRenderViewportCmd;
}

MSyntax FireRenderViewportCmd::newSyntax()
{
	MStatus status;
	MSyntax syntax;

	CHECK_MSTATUS(syntax.addFlag(kPanelFlag, kPanelFlagLong, MSyntax::kString));
	CHECK_MSTATUS(syntax.addFlag(kClearFlag, kClearFlagLong, MSyntax::kNoArg));
	CHECK_MSTATUS(syntax.addFlag(kActiveCacheFlag, kActiveCacheFlagLong, MSyntax::kBoolean));
	CHECK_MSTATUS(syntax.addFlag(kViewportModeFlag, kViewportModeFlagLong, MSyntax::kString));
	CHECK_MSTATUS(syntax.addFlag(kRefreshFlag, kRefreshFlagLong, MSyntax::kNoArg));
	CHECK_MSTATUS(syntax.addFlag(kViewportAOVFlag, kViewportAOVFlagLong, MSyntax::kLong));

	return syntax;
}

int FireRenderViewportCmd::convertViewPortMode(MString sViewportMode)
{
	if (sViewportMode == "globalIllumination")
		return RPR_RENDER_MODE_GLOBAL_ILLUMINATION;
	else if (sViewportMode == "directIllumination")
		return RPR_RENDER_MODE_DIRECT_ILLUMINATION;
	else if (sViewportMode == "directIlluminationNoShadow")
		return RPR_RENDER_MODE_DIRECT_ILLUMINATION_NO_SHADOW;
	else if (sViewportMode == "wireframe")
		return RPR_RENDER_MODE_WIREFRAME;
	else if (sViewportMode == "materialId")
		return RPR_RENDER_MODE_MATERIAL_INDEX;
	else if (sViewportMode == "position")
		return RPR_RENDER_MODE_POSITION;
	else if (sViewportMode == "normal")
		return RPR_RENDER_MODE_NORMAL;
	else if (sViewportMode == "texcoord")
		return RPR_RENDER_MODE_TEXCOORD;
	else if (sViewportMode == "ambientOcclusion")
		return RPR_RENDER_MODE_AMBIENT_OCCLUSION;

	ErrorPrint("Invalid Viewport Render Mode: %s", sViewportMode.asChar());
	return RPR_RENDER_MODE_GLOBAL_ILLUMINATION;
}

MStatus FireRenderViewportCmd::doIt(const MArgList & args)
{
	MStatus status;

	MArgDatabase argData(syntax(), args);

	MString panelName;
	if (argData.isFlagSet(kPanelFlag))
	{
		argData.getFlagArgument(kPanelFlag, 0, panelName);
	}

	try
	{
		vector<function<void(FireRenderViewport*)>> viewportActions;
		FireRenderViewportManager& manager = FireRenderViewportManager::instance();

		if (argData.isFlagSet(kClearFlag))
		{
			//clear cache
			viewportActions.push_back([](FireRenderViewport* viewport)
			{
				viewport->clearTextureCache();
			});
		}

		if (argData.isFlagSet(kActiveCacheFlag))
		{
			bool activateCache = true;
			argData.getFlagArgument(kActiveCacheFlag, 0, activateCache);

			viewportActions.push_back([=](FireRenderViewport* viewport)
			{
				viewport->setUseAnimationCache(activateCache);
			});
		}

		if (argData.isFlagSet(kViewportModeFlag))
		{
			MString sViewportMode;
			argData.getFlagArgument(kViewportModeFlag, 0, sViewportMode);

			auto viewportMode = convertViewPortMode(sViewportMode);

			viewportActions.push_back([=](FireRenderViewport* viewport)
			{
				viewport->setViewportRenderMode(viewportMode);
			});
		}

		if (argData.isFlagSet(kViewportAOVFlag))
		{
			int aovIndex;
			argData.getFlagArgument(kViewportAOVFlag, 0, aovIndex);

			viewportActions.push_back([=](FireRenderViewport* viewport)
			{
				viewport->setCurrentAOV(aovIndex);
			});
		}

		if (argData.isFlagSet(kRefreshFlag) && panelName != "")
		{
			viewportActions.push_back([=](FireRenderViewport* viewport)
			{
				viewport->refresh();
			});
		}

		// Apply changes:
		if (panelName != "")
		{
			if (auto viewport = manager.getViewport(panelName.asChar()))
			{
				for (auto viewportAction : viewportActions)
					viewportAction(viewport);
			}
		}
	}
	catch (...)
	{
		// Perform clean up operations.
		MRenderView::endRender();

		// Process the error.
		FireRenderError error(std::current_exception());
		return MStatus::kFailure;
	}

	return MS::kSuccess;
}
