#include "SkyBuilder.h"
#include "SkyGen.h"
#include "SunPosition/SPA.h"
#include "FireRenderMath.h"
#include "frWrap.h" // just for SkyBuilder::updateImage
#include <vector>
#include <stdio.h>
#include <cstring>

// Life Cycle
// -----------------------------------------------------------------------------
SkyBuilder::SkyBuilder(const MObject& object, unsigned int imageWidth, unsigned int imageHeight) :
	m_attributes(object),
	m_imageWidth(imageWidth),
	m_imageHeight(imageHeight),
	m_imageBuffer()
{
}

// -----------------------------------------------------------------------------
SkyBuilder::~SkyBuilder()
{
	// Delete the image buffer if required.
	if (m_imageBuffer)
		m_imageBuffer.reset();
}


// Public Methods
// -----------------------------------------------------------------------------
bool SkyBuilder::refresh()
{
	// Refresh sky attributes.
	bool changed = m_attributes.refresh();

	// Calculate the sun position.
	calculateSunPosition();

	return changed;
}

// -----------------------------------------------------------------------------
void SkyBuilder::updateImage(frw::Context& context, frw::Image& image)
{
	// Create the image.
	createSkyImage();

	// Update the RPR image.
	rpr_image_desc imgDesc = {};
	imgDesc.image_width = m_imageWidth;
	imgDesc.image_height = m_imageHeight;
	image = frw::Image(context, { 3, RPR_COMPONENT_TYPE_FLOAT32 }, imgDesc, m_imageBuffer.get());
}

// -----------------------------------------------------------------------------
void SkyBuilder::updateSampleImage(MImage& image)
{
	// Create the image.
	createSkyImage();

	// Covert the image to 1 byte per channel.
	std::vector<unsigned char> bytes;
	unsigned int count = m_imageWidth * m_imageHeight;
	bytes.resize(count * 4);

	float scale = 255 * getSkyIntensity();
	int offset = int((m_sunAzimuth - M_PI / 2 ) / (M_PI * 2) * m_imageWidth) % m_imageWidth;

	unsigned int s = 0;
	unsigned char* dst = &bytes[0];
	for (unsigned int y = 0; y < m_imageHeight; y++, s += m_imageWidth)
	{
		for (unsigned int x = 0; x < m_imageWidth; x++)
		{
			// Flip the image horizontally using "-offset" so the sun moves in the correct direction.
			// Source pointer: use (x,y) and apply offset to x
			unsigned int i = s + (x - offset) % m_imageWidth;
			const SkyRgbFloat32& src = m_imageBuffer.get()[i];

			*dst++ = static_cast<unsigned int>(fminf(src.b * scale, 255));
			*dst++ = static_cast<unsigned int>(fminf(src.g * scale, 255));
			*dst++ = static_cast<unsigned int>(fminf(src.r * scale, 255));
			*dst++ = 255;
		}
	}

	// Update the Maya image.
	image.setPixels(bytes.data(), m_imageWidth, m_imageHeight);
}

// -----------------------------------------------------------------------------
MColor& SkyBuilder::getSunLightColor()
{
	return m_sunLightColor;
}

// -----------------------------------------------------------------------------
float SkyBuilder::getSunAzimuth()
{
	auto rotated = m_sunAzimuth - M_PI / 2.0f;

	return (float) -rotated;
}

// -----------------------------------------------------------------------------
float SkyBuilder::getSunAltitude()
{
	return m_sunAltitude;
}

// -----------------------------------------------------------------------------
float SkyBuilder::getSkyIntensity()
{
	return m_attributes.intensity;
}

// -----------------------------------------------------------------------------
MFloatVector SkyBuilder::getWorldSpaceSunDirection()
{
	float phi = (float) ((M_PI * 0.5f) + m_sunAltitude);
	float theta = (float) (m_sunAzimuth + M_PI * 0.5f);
	float sinphi = sin(phi);

	return MFloatVector(-cos(theta) * sinphi, -cos(phi), sin(theta) * sinphi);
}


// Private Methods
// -----------------------------------------------------------------------------
void SkyBuilder::calculateSunPosition()
{
	// Determine whether the sun position should be
	// calculated directly from azimuth and altitude,
	// or if date, time and location should be used.
	if (m_attributes.sunPositionType == SkyAttributes::kAltitudeAzimuth)
	{
		m_sunAzimuth = m_attributes.azimuth;
		m_sunAltitude = m_attributes.altitude;
	}
	else
		calculateSunPositionGeographic();

	// Rotate so North matches Maya's default North.
	m_sunAzimuth -= 180;
	if (m_sunAzimuth < 0)
		m_sunAzimuth += 360;

	// Before this point, m_sunAltitude and m_sunAzimuth are in degrees

	// Convert to radians.
	m_sunAzimuth = toRadians(m_sunAzimuth);
	m_sunAltitude = toRadians(fmaxf(m_sunAltitude, -88));

	// Calculate the sun's direction vector.
	//m_sunDirection = MFloatVector(0.f, sin(m_sunAltitude), cos(m_sunAltitude));

	// Calculate the sun's direction vector.
	float phi = (float) ((M_PI * 0.5f) + m_sunAltitude);
	float theta = 0.0f; // -m_sunAzimuth;
	float sinphi = sin(phi);

	m_sunDirection = Point3(
		cos(theta) * sinphi,
		sin(theta) * sinphi,
		-cos(phi)
	);

}

// -----------------------------------------------------------------------------
void SkyBuilder::calculateSunPositionGeographic()
{
	// Adjust time for daylight saving if required.
	bool daylightSaving = m_attributes.daylightSaving;
	int hours = m_attributes.hours;
	int day = m_attributes.day;
	int month = m_attributes.month;
	int year = m_attributes.year;

	if (daylightSaving)
		adjustDaylightSavingTime(hours, day, month, year);

	// Initialize the sun position calculator
	JulianDate date = julian_day(year, month, day,
		hours, m_attributes.minutes, m_attributes.seconds,
		m_attributes.timeZone);

	AzimuthZenithAngle pos = calculateSolarPosition(date,
		m_attributes.latitude, m_attributes.longitude,
		0.0, 0.0, 820, 11);

	m_sunAzimuth = (float) pos.Azimuth;
	m_sunAltitude = (float) (90.0 - pos.Zenith);
}

// -----------------------------------------------------------------------------
void SkyBuilder::adjustDaylightSavingTime(int& hours, int& day, int& month, int& year) const
{
	static const int monthDays[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

	hours--;
	if (hours == -1)
	{
		hours = 23;
		day--;
		if (day == 0)
		{
			month--;
			if (month == 0)
			{
				month = 12;
				day = 31;
				year--;
			}
			else
			{
				if (month == 2)
				{
					bool leapdays = (year % 4 == 0) && (year % 400 == 0 || year % 100 != 0);
					if (leapdays)
						day = 29;
					else
						day = 28;
				}
				else
					day = monthDays[month - 1];
			}
		}
	}
}

// -----------------------------------------------------------------------------
void SkyBuilder::createSkyImage()
{
	// Create the image buffer if necessary.
	if (!m_imageBuffer)
		m_imageBuffer = std::make_unique<SkyRgbFloat32[]>(m_imageWidth * m_imageHeight);

	// Initialize the sky generator.
	SkyGen sg;
	sg.saturation = m_attributes.saturation;
#ifdef USE_DIRECTIONAL_SKY_LIGHT
	sg.mSunIntensity = 0.01f;
#else
	sg.sun_disk_intensity = 100.0f;
#endif
	sg.ground_color = m_attributes.groundColor;
	sg.horizon_height = m_attributes.horizonHeight;
	sg.horizon_blur = m_attributes.horizonBlur;
	sg.sun_disk_scale = m_attributes.sunDiskSize;
	sg.sun_glow_intensity = m_attributes.sunGlow;
	sg.multiplier = 1.0;
	sg.filter_color = m_attributes.filterColor;
	sg.sun_direction = m_sunDirection;
	sg.haze = 1.f + m_attributes.turbidity * (9.0f / 50.0f);


	// Generate the image.
	memset(m_imageBuffer.get(), 0, sizeof(SkyRgbFloat32) * m_imageWidth * m_imageHeight);
	sg.generate(m_imageWidth, m_imageHeight, m_imageBuffer.get());

	SkyColor c = sg.computeColor(sg.sun_direction);
	m_sunLightColor = c.asColor();
}
