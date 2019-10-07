#pragma once

#include "maya/MObject.h"
#include "DependencyNode.h"

/** A wrapper for sky attributes. */
class SkyAttributes
{

public:

	// Life Cycle
	// -----------------------------------------------------------------------------

	SkyAttributes(const MObject& object);
	virtual ~SkyAttributes();


	// Properties
	// -----------------------------------------------------------------------------
	float turbidity = 0;
	float intensity = 0;
	float albedo = 0;
	float sunGlow = 0;
	float sunDiskSize = 0;
	short sunPositionType = 0;
	MColor groundColor = MColor::kOpaqueBlack;
	MColor filterColor = MColor::kOpaqueBlack;

	float horizonHeight;
	float horizonBlur;
	float saturation;

	float azimuth = 0;
	float altitude = 0;

	float latitude = 0;
	float longitude = 0;
	float timeZone = 0;

	bool daylightSaving = false;
	int hours = 0;
	int minutes = 0;
	int seconds = 0;
	int day = 0;
	int month = 0;
	int year = 0;


	// Enums
	// -----------------------------------------------------------------------------

	/** The method used for positioning the sun. */
	enum SunPositionType
	{
		kAltitudeAzimuth = 0,
		kTimeLocation
	};


	// Public Methods
	// -----------------------------------------------------------------------------

	/** Refresh attributes. Return true if any changed. */
	bool refresh();


private:

	// Members
	// -----------------------------------------------------------------------------

	/** Sky attributes node. */
	DependencyNode m_node;
};
