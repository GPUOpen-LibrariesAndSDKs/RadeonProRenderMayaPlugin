#pragma once

#include "BaseConverter.h"

namespace MayaStandardNodeConverters
{

	class ProjectionNodeConverter : public BaseConverter
	{

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
		frw::Value GetCameraOrigin(ProjectionType projectionType) const;
		frw::Value GetCameraScale(ProjectionType projectionType) const;
		std::pair<frw::Value, frw::Value> GetCameraRotation(ProjectionType projectionType) const;

		std::unique_ptr<frw::BaseUVNode> CreateUVNode(ProjectionType projectionType) const;
		frw::Image GetImageFromConnectedFileNode() const;
	};

}