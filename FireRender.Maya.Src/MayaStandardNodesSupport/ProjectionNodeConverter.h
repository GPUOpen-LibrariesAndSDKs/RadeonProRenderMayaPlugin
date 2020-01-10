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
		void SubscribeCurrentMeshToCameraUpdates();
	};

}