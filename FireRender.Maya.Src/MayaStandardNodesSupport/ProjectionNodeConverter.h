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

#include "BaseConverter.h"

namespace MayaStandardNodeConverters
{

	class ProjectionNodeConverter : public BaseConverter
	{

		struct TransformInfo
		{
			frw::Value origin;
			frw::Value rotationX;
			frw::Value rotationZ;
			frw::Value scale;
		};

		enum class ProjectionType
		{
			Off,
			Planar,
			Spherical,
			Cylindrical,
			Ball,
			Cubic,
			TriPlanar,
			Concentric,
			Perspective
		};

	public:
		ProjectionNodeConverter(const ConverterParams& params);
		virtual frw::Value Convert() const override;

	private:	
		TransformInfo GetCameraTransform() const;
		TransformInfo GetPlaceTextureTransfrom() const;

		std::unique_ptr<frw::BaseUVNode> CreateUVNode(ProjectionType projectionType) const;
		frw::Image GetImageFromConnectedFileNode() const;
		MObject GetConnectedCamera() const;
		void SubscribeCurrentMeshToCameraUpdates() const;
	};

}