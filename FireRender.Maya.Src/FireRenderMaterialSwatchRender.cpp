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
#include "FireRenderMaterialSwatchRender.h"

#include <maya/MFnDependencyNode.h>
#include <maya/MImage.h>
#include <maya/MPlug.h>
#include <maya/MGlobal.h>
#include <maya/MFileObject.h>

#include <thread>

#include "SkyBuilder.h"
#include "FireRenderSkyLocator.h"

#include "FireRenderSwatchInstance.h"

using namespace FireMaya;
using namespace std::chrono;

FireRenderMaterialSwatchRender::FireRenderMaterialSwatchRender(MObject obj, MObject renderObj, int res) :
	MSwatchRenderBase(obj, renderObj, res),
	m_runningAsyncRender(false),
	m_finishedAsyncRender(false),
	m_cancelAsyncRender(false)
{

}

FireRenderMaterialSwatchRender::~FireRenderMaterialSwatchRender()
{

}

FireRenderSwatchInstance& FireRenderMaterialSwatchRender::getSwatchInstance()
{
	return FireRenderSwatchInstance::instance(); 
}

bool FireRenderMaterialSwatchRender::doIteration()
{
	try
	{
		m_cancelAsyncRender = false;

		MImage& img = image();
		if (img.depth() == 0)
		{
			int res = resolution();
			img.create(res, res, 4);
		}

		if (m_runningAsyncRender)
			return false;

		if (IsFRNode())
		{
			if (!setupFRNode())
			{
				return true;
			}
			getSwatchInstance().enqueSwatch(this);
		}
		else
		{
			return doIterationForNonFRNode();
		}
	}
	catch (int errCode)
	{
		errCode;
		// preventing from being crashed
		return false;
	}

	return false;
}

bool FireRenderMaterialSwatchRender::IsFRNode() const
{
	MFnDependencyNode nodeFn(node());
	FireMaya::Node* fireMayaNode = dynamic_cast<FireMaya::Node*>(nodeFn.userNode());

	return fireMayaNode != nullptr;
}

bool FireRenderMaterialSwatchRender::setupFRNode()
{
	MObject mnode = node();
	MFnDependencyNode nodeFn(mnode);

	auto disableSwatchPlug = nodeFn.findPlug("disableSwatch");

	bool enableSwatches = false;
	FireRenderGlobalsData::getThumbnailIterCount(&enableSwatches);
	 
	if (enableSwatches && (disableSwatchPlug.isNull() || !disableSwatchPlug.asBool()))
	{
		FireRenderSwatchInstance& swatchInstance = getSwatchInstance();

		m_shader = swatchInstance.getContext().GetShader(mnode);
		m_volumeShader = swatchInstance.getContext().GetVolumeShader(mnode);

		m_resolution = resolution();

		return true;
	}

	return false;
}

void FireRenderMaterialSwatchRender::processFromBackgroundThread()
{
	if (m_cancelAsyncRender)
		return;

	try
	{
		FireRenderSwatchInstance& swatchInstance = getSwatchInstance();

		swatchInstance.getContext().setStartedRendering();
		// consider using a different mesh depending on surface or value type
		if (auto mesh = swatchInstance.getContext().getRenderObject<FireRenderMesh>("mesh"))
		{
			for (auto& element : mesh->Elements())
			{
				if (!element.shape)
					continue;
				auto& shape = element.shape;
				shape.SetShader(m_shader);
				shape.SetVolumeShader(m_volumeShader);
			}
		}

		if ((swatchInstance.getContext().width() != (unsigned int) m_resolution) || 
			(swatchInstance.getContext().height() != (unsigned int) m_resolution))
		{
			swatchInstance.getContext().setResolution(m_resolution, m_resolution, false);
		}

		swatchInstance.getContext().setDirty();
		swatchInstance.getContext().m_restartRender = true;
		swatchInstance.getContext().UpdateCompletionCriteriaForSwatch();

		while (swatchInstance.getContext().keepRenderRunning())
		{
			if (m_cancelAsyncRender)
				break;

			swatchInstance.getContext().render();
		}

		m_finishedAsyncRender = !m_cancelAsyncRender;
	}
	catch (...)
	{
		DebugPrint("Unknown error running material swatch render");
	}

	if (m_finishedAsyncRender)
	{
		finalizeRendering();
	}

	std::unique_lock<std::mutex> lck(m_cancellationMutex);
	m_runningAsyncRender = false;
	m_cancellationCondVar.notify_one();
}

bool FireRenderMaterialSwatchRender::finalizeRendering()
{
	FireRenderContext & context = getSwatchInstance().getContext();

	unsigned int width = context.m_width;
	unsigned int height = context.m_height;
	std::vector<float> data = context.getRenderImageData();

	MImage& img = image();

	img.setFloatPixels(data.data(), width, height);
	img.convertPixelFormat(MImage::kByte);

	finishParallelRender();

	return true;
}

bool FireRenderMaterialSwatchRender::doIterationForNonFRNode()
{
	MImage& img = image();
	int res = resolution();

	MFnDependencyNode nodeFn(node());
	if (auto node = dynamic_cast<FireRenderIBL*>(nodeFn.userNode()))
	{
		const MPlug filePathPlug = nodeFn.findPlug("filePath");
		MString filePath;
		filePathPlug.getValue(filePath);
		if (img.readFromFile(filePath) == MS::kSuccess)
		{
			img.resize(res, res, false);
			img.verticalFlip();

			return true;
		}
	} 
	else if (auto node = dynamic_cast<FireRenderSkyLocator*>(nodeFn.userNode()))
	{
		SkyBuilder skyBuilder(fObjToRender, res, res);
		skyBuilder.refresh();
		skyBuilder.updateSampleImage(img);

		return true;
	}

	// OK so we can look for preset icon for this material type
	MString path = FireMaya::GetIconPath() + "/" + nodeFn.typeName() + ".png";
	if (MStatus::kSuccess == img.readFromFile(path))
	{
		img.resize(res, res, false);

		return true;
	}

	// or fall through to basic default
	path = FireMaya::GetIconPath() + "/Default.png";
	if (MStatus::kSuccess == img.readFromFile(path))
	{
		img.resize(res, res, false);

		return true;
	}

	return false;
}

MSwatchRenderBase* FireRenderMaterialSwatchRender::creator(MObject dependNode, MObject renderNode, int imageResolution)
{
	auto sw = new FireRenderMaterialSwatchRender(dependNode, renderNode, imageResolution);

	return sw;
}

bool FireRenderMaterialSwatchRender::renderParallel()
{
	// Render parallel in case we render material which is required many passes
	// If we render IBL or any othe non-material type of node we need just one pass and does not require parallel rendering
	return IsFRNode();
}

void FireRenderMaterialSwatchRender::cancelParallelRendering()
{
	DebugPrint("FireRenderMaterialSwatchRender::cancelParallelRendering()");

	getSwatchInstance().removeFromQueue(this);

	m_cancelAsyncRender = true;

	// We need to wait until background thread finishes cancel operation, because
	// current instance of swatch renderer may become invalid (deleted from memory) 
	// after this function (cancelParallelRendering) completes the execution

	std::unique_lock<std::mutex> lck(m_cancellationMutex);
	while (m_runningAsyncRender)
	{
		m_cancellationCondVar.wait(lck);
	}
}
