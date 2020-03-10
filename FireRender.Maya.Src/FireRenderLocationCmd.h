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

#include <maya/MPxCommand.h>
#include <maya/MSyntax.h>
#include <maya/MStringArray.h>
#include <maya/MArgDatabase.h>
#include <maya/MFloatVector.h>
#include <vector>


/**
 * Perform location related tasks that are
 * too expensive to be executed by scripts.
 */
class FireRenderLocationCmd : public MPxCommand
{

public:

	// MPxCommand Implementation
	// -----------------------------------------------------------------------------

	/** Perform a location operation. */
	MStatus doIt(const MArgList& args);

	/** Used by Maya to create the command instance. */
	static void* creator();

	/** Return the command syntax object. */
	static MSyntax newSyntax();


private:

	// Enums
	// -----------------------------------------------------------------------------

	/** The types of region that define a time zone boundary. */
	enum TimeZoneType
	{
		RECTANGLE,
		POLYGON
	};


	// Structs
	// -----------------------------------------------------------------------------

	/** Location information. */
	struct Location
	{
		std::string name;
		float latitude;
		float longitude;
		float utcOffset;
		int countryIndex;
	};

	/** Time zone information. */
	struct TimeZone
	{
		std::string name;
		std::vector<MFloatVector> vertices;
		int type;
		float utcOffset;
	};


	// Static Members
	// -----------------------------------------------------------------------------

	/** The list of all countries. */
	static std::vector<std::string> s_countries;

	/** The list of all locations. */
	static std::vector<Location> s_locations;

	/** The list of all time zones. */
	static std::vector<TimeZone> s_timeZones;

	/** The list of most recent search results. */
	static std::vector<Location> s_searchResults;

	/** True once location data is loaded. */
	static bool s_loaded;

	// Private Methods
	// -----------------------------------------------------------------------------

	/** Load data from the specified path. */
	void load(const MArgDatabase& argData);

	/** Load country data. */
	void loadCountries(const MString& path);

	/** Load location data. */
	void loadLocations(const MString& path);

	/** Load time zone data. */
	void loadTimeZones(const MString& path);


	/** Perform a location search and return the results. */
	void searchLocations(const MArgDatabase& argData);

	/** Get a string for a location, including country. */
	MString getLocationString(const Location& location) const;

	/** Capitalize a location name because they are stored as lower case. */
	void capitalizeLocationName(std::string& name) const;

	/** Return data for a location in the most recent search results. */
	void getLocationData(const MArgDatabase& argData);

	/** Get a UTC offset for the given geographic coordinate. */
	void getUTCOffset(const MArgDatabase& argData) const;

	/** Get a geographic coordinate from argument data. */
	void getGeographicCoordinate(const MArgDatabase& argData, float& latitude, float& longitude) const;

	/** True if the given point is in the given time zone. */
	bool isPointInTimeZone(const TimeZone& timeZone, float x, float y) const;

	/** True if the given point is in the rectangle defined by the vertices. */
	bool isPointInRectangle(const std::vector<MFloatVector>& v, float x, float y) const;

	/** True if the given point is in the polygon defined by the vertices. */
	bool isPointInPolygon(const std::vector<MFloatVector>& v, float x, float y) const;

};


// Command arguments.
#define kPath "-p"
#define kPathLong "-path"
#define kSearch "-s"
#define kSearchLong "-search"
#define kIndex "-i"
#define kIndexLong "-index"
#define kLatitude "-lat"
#define kLatitudeLong "-latitude"
#define kLongitude "-lon"
#define kLongitudeLong "-longitude"
