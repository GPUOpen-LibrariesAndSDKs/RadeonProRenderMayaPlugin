#include "SkyAttributes.h"
#include "FireRenderSkyLocator.h"


// Life Cycle
// -----------------------------------------------------------------------------
SkyAttributes::SkyAttributes(const MObject& object) :
	m_node(object)
{
}

// -----------------------------------------------------------------------------
SkyAttributes::~SkyAttributes()
{
}


// Public Methods
// -----------------------------------------------------------------------------
bool SkyAttributes::refresh()
{
	// Read attributes from the node.
	float turbidity = m_node.getFloat("turbidity");
	float intensity = m_node.getFloat("intensity");
	float sunGlow = m_node.getFloat("sunGlow");
	float sunDiskSize = m_node.getFloat("sunDiskSize");
	short sunPositionType = m_node.getShort("sunPositionType");
	MColor groundColor = m_node.getColor("groundColor");
	MColor groundAlbedo = m_node.getFloat("groundAlbedo");
	MColor filterColor = m_node.getColor("filterColor");
	float azimuth = m_node.getFloat("azimuth");
	float altitude = m_node.getFloat("altitude");
	float latitude = m_node.getFloat("latitude");
	float longitude = m_node.getFloat("longitude");
	float timeZone = m_node.getFloat("timeZone");
	bool daylightSaving = m_node.getBool("daylightSaving");
	int hours = m_node.getInt("hours");
	int minutes = m_node.getInt("minutes");
	int seconds = m_node.getInt("seconds");
	int day = m_node.getInt("day");
	int month = m_node.getInt("month");
	int year = m_node.getInt("year");

	// Determine if the sky has changed.
	bool changed =
		this->turbidity != turbidity ||
	/*	this->intensity != intensity || -- we're using constant intensity, and changing intensity directly for RPR */
		this->albedo != albedo ||
		this->sunGlow != sunGlow ||
		this->sunDiskSize != sunDiskSize ||
		this->groundColor != groundColor ||
		this->groundAlbedo != groundAlbedo ||
		this->filterColor != filterColor;

	// If the base sky attributes have not changed,
	// check if the analytical flag has changed.
	if (!changed)
	{
		if (this->sunPositionType == sunPositionType)
		{
			// Check if analytical attributes have changed.
			if (sunPositionType == kAltitudeAzimuth)
			{
				changed =
				/*    this->azimuth != azimuth || -- ignore azimuth changes, will be adjusted with env's transform matrix */
					this->altitude != altitude;
			}

			// Check if date, time and location attributes have changed.
			else
			{
				changed =
					this->latitude != latitude ||
					this->longitude != longitude ||
					this->timeZone != timeZone ||
					this->daylightSaving != daylightSaving ||
					this->hours != hours ||
					this->minutes != minutes ||
					this->seconds != seconds ||
					this->day != day ||
					this->month != month ||
					this->year != year;
			}
		}

		// The analytical flag has changed.
		else
			changed = true;
	}

	// Update properties.
	this->turbidity = turbidity;
	this->intensity = intensity;
	this->sunGlow = sunGlow;
	this->sunDiskSize = sunDiskSize;
	this->sunPositionType = sunPositionType;
	this->groundColor = groundColor;
	this->groundAlbedo = groundAlbedo;
	this->filterColor = filterColor;
	this->azimuth = azimuth;
	this->altitude = altitude;
	this->latitude = latitude;
	this->longitude = longitude;
	this->timeZone = timeZone;
	this->daylightSaving = daylightSaving;
	this->hours = hours;
	this->minutes = minutes;
	this->seconds = seconds;
	this->day = day;
	this->month = month;
	this->year = year;

	// Return true if sky has changed.
	return changed;
}
