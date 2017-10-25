#include "FireRenderMaterialSwatchRender.h"

#include <maya/MFnDependencyNode.h>
#include <maya/MImage.h>
#include <maya/MPlug.h>
#include <maya/MGlobal.h>
#include <maya/MFileObject.h>

#include "AutoLock.h"
#include "SkyBuilder.h"
#include "FireRenderSkyLocator.h"

#include "FireRenderThread.h"

using namespace FireMaya;

FireRenderMaterialSwatchRender::FireRenderMaterialSwatchRender(MObject obj, MObject renderObj, int res) :
	MSwatchRenderBase(obj, renderObj, res),
	m_runningAsyncRender(false),
	m_finnishedAsyncRender(false),
	m_cancelAsyncRender(false),
	m_warningDialogOpen(false)
{
}

FireRenderMaterialSwatchRender::~FireRenderMaterialSwatchRender()
{

}

bool FireRenderMaterialSwatchRender::doIteration()
{
	MObject mnode = node();

	MImage& img = image();
	int res = resolution();
	img.create(res, res, 4);

	MFnDependencyNode nodeFn(mnode);
	auto fireMayaNode = dynamic_cast<FireMaya::Node*>(nodeFn.userNode());

	if (m_cancelAsyncRender)
	{
		m_runningAsyncRender = false;
		m_finnishedAsyncRender = false;

		return true;
	}

	if (m_runningAsyncRender)
		return false;

	if (m_finnishedAsyncRender)
	{
		if (m_warningDialogOpen &&  rcWarningDialog.shown)
			rcWarningDialog.close();

		size_t width, height;
		auto data = FireRenderThread::RunOnceAndWait<std::vector<float>>([this, &width, &height]() -> std::vector<float>
		{
			FireRenderSwatchInstance& swatchInstance = FireRenderSwatchInstance::instance();

			RPR::AutoLock<MSpinLock> lock(swatchInstance.mutex);
			width = swatchInstance.context.m_width;
			height = swatchInstance.context.m_height;
			return swatchInstance.context.getRenderImageData();
		});

		{
			unsigned int iw = 0;
			unsigned int ih = 0;
			img.getSize(iw, ih);
#pragma warning(disable: 4267)
			if ((iw != width) || (ih != height))
				img.create(width, height, 4u);

			img.setFloatPixels(data.data(), width, height);
#pragma warning(default: 4267)
			img.convertPixelFormat(MImage::kByte);
		}

		finishParallelRender();

		return true;
	}

	if (fireMayaNode)
	{
		auto disableSwatchPlug = nodeFn.findPlug("disableSwatch");

		if (disableSwatchPlug.isNull() || !disableSwatchPlug.asBool())
		{
			m_runningAsyncRender = true;
			m_finnishedAsyncRender = false;

			FireRenderSwatchInstance& swatchInstance = FireRenderSwatchInstance::instance();
			if (swatchInstance.context.isFirstIterationAndShadersNOTCached())
			{
				//first iteration and shaders are _NOT_ cached
				rcWarningDialog.show();
				m_warningDialogOpen = true;
			}

			FireRenderThread::RunOnceProcAndWait([&]
			{
				FireRenderSwatchInstance& swatchInstance = FireRenderSwatchInstance::instance();

				RPR::AutoLock<MSpinLock> lock(swatchInstance.mutex);

				swatchInstance.context.updateSwatchSceneRenderLimits(mnode);
				swatchInstance.context.setStartedRendering();
				// consider using a different mesh depending on surface or value type
				if (auto mesh = swatchInstance.context.getRenderObject<FireRenderMesh>("mesh"))
				{
					if (mesh->Elements().size())
					{
						if (auto shape = mesh->Element(0).shape)
						{
							shape.SetShader(swatchInstance.context.GetShader(mnode));
							shape.SetVolumeShader(swatchInstance.context.GetVolumeShader(mnode));
						}
					}
				}

				if ((swatchInstance.context.width() != res) && (swatchInstance.context.height() != res))
					swatchInstance.context.setResolution(res, res, false);
			});

			FireRenderThread::KeepRunning([this]() -> bool
			{
				try
				{
					if (m_cancelAsyncRender)
						return false;

					FireRenderSwatchInstance& swatchInstance = FireRenderSwatchInstance::instance();

					RPR::AutoLock<MSpinLock> lock(swatchInstance.mutex);

					if (m_cancelAsyncRender)
						return false;

					swatchInstance.context.setDirty();
					swatchInstance.context.Freshen();

					while (swatchInstance.context.keepRenderRunning())
					{
						if (m_cancelAsyncRender)
							return false;

						swatchInstance.context.render();
					}
				}
				catch (...)
				{
					DebugPrint("Unknown error running material swatch render");
				}

				m_runningAsyncRender = false;
				m_finnishedAsyncRender = true;

				return false;
			});
		}

		return false;
	}

	if (auto node = dynamic_cast<FireRenderIBL*>(nodeFn.userNode()))
	{
		MPlug filePathPlug = nodeFn.findPlug("filePath");
		MString filePath;
		filePathPlug.getValue(filePath);
		if (img.readFromFile(filePath) == MS::kSuccess)
		{
			img.resize(res, res, false);
			img.verticalFlip();

			finishParallelRender();
			return true;
		}
	}

	if (auto node = dynamic_cast<FireRenderSkyLocator*>(nodeFn.userNode()))
	{
		SkyBuilder skyBuilder(fObjToRender, res, res);
		skyBuilder.refresh();
		skyBuilder.updateSampleImage(img);

		finishParallelRender();
		return true;
	}

	// OK so we can look for preset icon for this material type
	if (MStatus::kSuccess == img.readFromFile(FireMaya::GetIconPath() + "/" + nodeFn.typeName() + ".png"))
	{
		img.resize(res, res, false);

		finishParallelRender();
		return true;
	}

	// or fall through to basic default
	if (MStatus::kSuccess == img.readFromFile(FireMaya::GetIconPath() + "/Default.png"))
	{
		img.resize(res, res, false);

		finishParallelRender();
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
	return m_runningAsyncRender;
}

void FireRenderMaterialSwatchRender::cancelParallelRendering()
{
	DebugPrint("FireRenderMaterialSwatchRender::cancelParallelRendering()");
	m_cancelAsyncRender = true;
}
