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
#include "TileRenderer.h"

#include "Context/FireRenderContext.h"
#include "Math/float2.h"

TileRenderer::TileRenderer()
{
}

TileRenderer::~TileRenderer()
{
}

void TileRenderer::Render(FireRenderContext& renderContext, const TileRenderInfo& info, AOVPixelBuffers& outBuffer, TileRenderingCallback callbackFunc)
{
	float tilesXf = info.totalWidth / (float)info.tileSizeX;
	float tilesYf = info.totalHeight / (float)info.tileSizeY;

	int xTiles = (int) std::ceil(tilesXf);
	int yTiles = (int) std::ceil(tilesYf);

	FireRenderCamera& fireRenderCamera = renderContext.camera();
	rpr_camera camera = fireRenderCamera.data().Handle();

	RadeonProRender::float2 sensorSize = 0.0f;
	rprCameraGetInfo(camera, RPR_CAMERA_SENSOR_SIZE, sizeof(sensorSize), &sensorSize, nullptr);

	RadeonProRender::float2 orthoSize = 0.0f;
	rprCameraGetInfo(camera, RPR_CAMERA_ORTHO_WIDTH, sizeof(orthoSize.x), &orthoSize.x, nullptr);
	rprCameraGetInfo(camera, RPR_CAMERA_ORTHO_HEIGHT, sizeof(orthoSize.y), &orthoSize.y, nullptr);

	MObject node = fireRenderCamera.Object();
	MFnDagNode dagNode(node);
	MPlug imagePlanePlug = dagNode.findPlug("imagePlane");
	MString plugName = imagePlanePlug.name();
	if (imagePlanePlug.isArray())
	{
		if (int n = imagePlanePlug.numElements())
			imagePlanePlug = imagePlanePlug.elementByPhysicalIndex(0);
	}
	plugName = imagePlanePlug.name();

	MObject imagePlane = FireMaya::GetConnectedNode(imagePlanePlug);
	if (!imagePlane.isNull())
	{
		MFnDependencyNode imNode(imagePlane);
		MString name2 = imNode.name();
		int debugi = 0;
	}

	MString name = fireRenderCamera.GetPlugValue(imagePlane, "imageName", MString());

	FireMaya::FitType tileFitType = (FireMaya::FitType) fireRenderCamera.GetPlugValue(imagePlane, "fit", 1);

	int counter = 0;
	for (int yTile = yTiles - 1; yTile >= 0; yTile--)
	{
		for (int xTile = 0; xTile < xTiles; xTile++)
		{
			RenderRegion region;

			region.left = xTile * info.tileSizeX;
			region.right = std::min(info.totalWidth, region.left + info.tileSizeX) - 1;

			region.bottom = yTile * info.tileSizeY;
			region.top = std::min(info.totalHeight, region.bottom + info.tileSizeY) - 1;

			float shiftX  = (region.left + 0.5f * ((int)region.getWidth() - (int)info.totalWidth)) / region.getWidth();
			float shiftY = (region.bottom + 0.5f * ((int)region.getHeight() - (int)info.totalHeight)) / region.getHeight();

			rprCameraSetLensShift(camera, shiftX, shiftY);

			if (fireRenderCamera.isDefaultPerspective())
			{
				rprCameraSetSensorSize(camera, sensorSize.x / ((float)info.totalWidth / region.getWidth()),
					sensorSize.y / ((float)info.totalHeight / region.getHeight()));
			}
			else if (fireRenderCamera.isDefaultOrtho())
			{
				rprCameraSetOrthoWidth(camera, orthoSize.x / ((float)info.totalWidth / region.getWidth()));
				rprCameraSetOrthoHeight(camera, orthoSize.y / ((float)info.totalHeight / region.getHeight()));
			}
			else
			{
				// not implemented;
				assert(false);
			}

			// process back plate
			int yTileIdx = yTiles - yTile - 1;

			int tileWidth = region.right - region.left + 1;
			int tileHeight = region.top - region.bottom + 1;

			MString colorSpace;
			frw::Image image = fireRenderCamera.Scope().GetTiledImage(name,
				info.totalWidth, info.totalHeight,
				info.tileSizeX, info.tileSizeY,
				tileWidth, tileHeight,
				xTiles, yTiles,
				xTile, yTileIdx,
				colorSpace, tileFitType);
			fireRenderCamera.Scene().SetBackgroundImage(image);

			counter++;
			if (!callbackFunc(region, 100 * counter / (xTiles * yTiles), outBuffer))
			{
				// to exit the loop
				xTile = xTiles;
				yTile = -1; 
			}
		}
	}

	// back previous values
	// probably this may be ommited, since this is already end of the rendering
	if (fireRenderCamera.isDefaultPerspective())
	{
		rprCameraSetSensorSize(camera, sensorSize.x, sensorSize.y);
	}
	else if (fireRenderCamera.isDefaultOrtho())
	{
		rprCameraSetOrthoWidth(camera, orthoSize.x);
		rprCameraSetOrthoHeight(camera, orthoSize.y);
	}
}
