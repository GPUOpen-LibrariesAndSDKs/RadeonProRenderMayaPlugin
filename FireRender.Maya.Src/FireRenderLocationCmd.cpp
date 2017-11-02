#include "FireRenderLocationCmd.h"
#include <maya/MStatus.h>
#include <maya/MArgList.h>
#include <maya/MGlobal.h>
#include <algorithm>

#undef min
#undef max

// Macros
// -----------------------------------------------------------------------------

/** Use wide character fopen on windows. */
#ifdef _WIN32
#define RPR_FOPEN(fileName, mode) _wfopen(fileName.asWChar(), L ## mode)
#else
#define RPR_FOPEN(fileName, mode) fopen(fileName.asUTF8(), mode)
#endif


// Namespaces
// -----------------------------------------------------------------------------
using namespace std;


// Static Initialization
// -----------------------------------------------------------------------------
vector<std::string> FireRenderLocationCmd::s_countries;
vector<FireRenderLocationCmd::Location> FireRenderLocationCmd::s_locations;
vector<FireRenderLocationCmd::TimeZone> FireRenderLocationCmd::s_timeZones;
vector<FireRenderLocationCmd::Location> FireRenderLocationCmd::s_searchResults;
bool FireRenderLocationCmd::s_loaded = false;


// MPxCommand Implementation
// -----------------------------------------------------------------------------
void* FireRenderLocationCmd::creator()
{
	return new FireRenderLocationCmd();
}

// -----------------------------------------------------------------------------
MSyntax FireRenderLocationCmd::newSyntax()
{
	MSyntax syntax;

	CHECK_MSTATUS(syntax.addFlag(kPath, kPathLong, MSyntax::kString));
	CHECK_MSTATUS(syntax.addFlag(kSearch, kSearchLong, MSyntax::kString));
	CHECK_MSTATUS(syntax.addFlag(kIndex, kIndexLong, MSyntax::kLong));
	CHECK_MSTATUS(syntax.addFlag(kLatitude, kLatitudeLong, MSyntax::kDouble));
	CHECK_MSTATUS(syntax.addFlag(kLongitude, kLongitudeLong, MSyntax::kDouble));

	return syntax;
}

// -----------------------------------------------------------------------------
MStatus FireRenderLocationCmd::doIt(const MArgList & args)
{
	// Parse arguments.
	MArgDatabase argData(syntax(), args);

	// Load location data from the specified path.
	if (argData.isFlagSet(kPath))
		load(argData);

	// Search locations and return a list of results.
	else if (argData.isFlagSet(kSearch))
		searchLocations(argData);

	// Get location data for an index
	// into the most recent search results.
	else if (argData.isFlagSet(kIndex))
		getLocationData(argData);

	// Get a UTC offset for the given coordinate.
	else if (argData.isFlagSet(kLatitude) && argData.isFlagSet(kLongitude))
		getUTCOffset(argData);

	return MStatus::kSuccess;
}

// Private Methods
// -----------------------------------------------------------------------------
void FireRenderLocationCmd::load(const MArgDatabase& argData)
{
	// Check if location data is already loaded.
	if (s_loaded)
		return;

	// Get the path to the location data files.
	MString path;
	argData.getFlagArgument(kPath, 0, path);

	// Load countries, locations and time zones.
	loadCountries(path);
	loadLocations(path);
	loadTimeZones(path);

	// Flag as loaded.
	s_loaded = true;
}

// -----------------------------------------------------------------------------
void FireRenderLocationCmd::loadCountries(const MString& path)
{
	// Open the countries file.
	MString fileName = path + "countries.dat";
	FILE* file = RPR_FOPEN(fileName, "rb");

	// Read the number of countries and resize the container.
	int count = 0;
	fread(&count, sizeof(int), 1, file);
	s_countries.resize(count);

	// Temporary string storage.
	char buffer[2048];
	int length = 0;

	// Read locations.
	for (int i = 0; i < count; i++)
	{
		// Read the country name.
		fread(&length, sizeof(int), 1, file);
		length = std::min<int>(length, sizeof(buffer) / sizeof(buffer[0]) - 1);
		fread(&buffer, sizeof(char), length, file);
		buffer[length] = '\0';
		s_countries[i] = buffer;
	}

	// Close the countries file.
	fclose(file);
}

// -----------------------------------------------------------------------------
void FireRenderLocationCmd::loadLocations(const MString& path)
{
	// Open the locations file.
	MString fileName = path + "locations.dat";
	FILE* file = RPR_FOPEN(fileName, "rb");

	// Read the number of locations and resize the container.
	int count = 0;
	fread(&count, sizeof(int), 1, file);
	s_locations.resize(count);

	// Temporary string storage.
	char buffer[2048];
	int length = 0;

	// Read locations.
	for (int i = 0; i < count; i++)
	{
		// Read the location name.
		fread(&length, sizeof(int), 1, file);
		length = std::min<int>(length, sizeof(buffer) / sizeof(buffer[0]) - 1);
		fread(&buffer, sizeof(char), length, file);
		buffer[length] = '\0';
		s_locations[i].name = buffer;

		// Read geographic information.
		fread(&s_locations[i].latitude, sizeof(float), 1, file);
		fread(&s_locations[i].longitude, sizeof(float), 1, file);
		fread(&s_locations[i].utcOffset, sizeof(float), 1, file);
		fread(&s_locations[i].countryIndex, sizeof(int), 1, file);
	}

	// Close the locations file.
	fclose(file);
}

// -----------------------------------------------------------------------------
void FireRenderLocationCmd::loadTimeZones(const MString& path)
{
	// Open the time zone shapes file.
	MString fileName = path + "time_zones.dat";
	FILE* file = RPR_FOPEN(fileName, "rb");

	// Read the number of locations and resize the container.
	int count = 0;
	fread(&count, sizeof(int), 1, file);
	s_timeZones.resize(count);

	// Temporary storage.
	char buffer[2048];
	int length = 0;
	int vertexCount = 0;
	float latitude = 0;
	float longitude = 0;

	// Read time zones.
	for (int i = 0; i < count; i++)
	{
		// Read the time zone name.
		fread(&length, sizeof(int), 1, file);
		length = std::min<int>(length, sizeof(buffer) / sizeof(buffer[0]) - 1);
		fread(buffer, sizeof(char), length, file);
		buffer[length] = '\0';
		s_timeZones[i].name = buffer;

		// Read time zone type and UTC offset.
		fread(&s_timeZones[i].type, sizeof(int), 1, file);
		fread(&s_timeZones[i].utcOffset, sizeof(float), 1, file);

		// Read time zone vertices.
		fread(&vertexCount, sizeof(int), 1, file);

		for (int j = 0; j < vertexCount; j++)
		{
			fread(&longitude, sizeof(float), 1, file);
			fread(&latitude, sizeof(float), 1, file);
			s_timeZones[i].vertices.push_back(MFloatVector(longitude, latitude));
		}
	}

	// Close the time zones file.
	fclose(file);
}

// -----------------------------------------------------------------------------
void FireRenderLocationCmd::searchLocations(const MArgDatabase& argData)
{
	// Get the search string.
	MString s;
	argData.getFlagArgument(kSearch, 0, s);
	string search = s.asUTF8();

	// Create the result string list that will be
	// returned and clear existing search results.
	MStringArray results;
	s_searchResults.clear();

	// Test the search string against each location entry.
	size_t count = s_locations.size();

	for (size_t i = 0; i < count; i++)
	{
		// Add a result if the search string was found in the location name.
		string::size_type pos = s_locations[i].name.find(search);
		if (pos != wstring::npos)
		{
			results.append(getLocationString(s_locations[i]));
			s_searchResults.push_back(s_locations[i]);
		}
	}

	// Return the result list.
	setResult(results);
}

// -----------------------------------------------------------------------------
MString FireRenderLocationCmd::getLocationString(const Location& location) const
{
	// Capitalize the location name.
	string name = location.name;
	capitalizeLocationName(name);

	// Append the country containing the location.
	MString result = name.c_str();
	result += " (";
	result += s_countries[location.countryIndex].c_str();
	result += ")";

	return result;
}

// -----------------------------------------------------------------------------
void capitalizeCharacter(char& c)
{
	// If the previous character was a space,
	// convert the character to upper case.
	if ((*(&c - sizeof(char))) == ' ')
		c = toupper(c);
}

// -----------------------------------------------------------------------------
void FireRenderLocationCmd::capitalizeLocationName(std::string& name) const
{
	// Capitalize the first letter.
	name[0] = toupper(name[0]);

	// Capitalize the first letter of subsequent words.
	for_each(name.begin() + 1, name.end(), capitalizeCharacter);
}

// -----------------------------------------------------------------------------
void FireRenderLocationCmd::getLocationData(const MArgDatabase& argData)
{
	// Get the index in the search results.
	unsigned int index;
	argData.getFlagArgument(kIndex, 0, index);

	// Check that the index is valid.
	if (index >= s_searchResults.size())
	{
		MGlobal::displayWarning("Location index out of bounds.");
		return;
	}

	// Get the indexed location.
	Location& location = s_searchResults[index];

	// Populate and set the result.
	MDoubleArray data;
	data.append(location.latitude);
	data.append(location.longitude);
	data.append(location.utcOffset);

	setResult(data);
}

// -----------------------------------------------------------------------------
void FireRenderLocationCmd::getUTCOffset(const MArgDatabase& argData) const
{
	// Get the geographic coordinate.
	float latitude;
	float longitude;
	getGeographicCoordinate(argData, latitude, longitude);

	// Test the coordinate against time zone regions.
	for (TimeZone& timeZone : s_timeZones)
	{
		// Return the time zone UTC offset
		// if it contains the coordinate.
		if (isPointInTimeZone(timeZone, longitude, latitude))
		{
			setResult(timeZone.utcOffset);
			return;
		}
	}

	// If the point is not in a region,
	// calculate the nautical time zone.
	setResult(longitude * 24.0f / 360.0f);
}

// -----------------------------------------------------------------------------
void FireRenderLocationCmd::getGeographicCoordinate(const MArgDatabase& argData, float& latitude, float& longitude) const
{
	// Get latitude and longitude from the arguments.
	double lat;
	double lon;

	argData.getFlagArgument(kLatitude, 0, lat);
	argData.getFlagArgument(kLongitude, 0, lon);

	// Cast to 32 bit floating point.
	latitude = static_cast<float>(lat);
	longitude = static_cast<float>(lon);
}

// -----------------------------------------------------------------------------
bool FireRenderLocationCmd::isPointInTimeZone(const TimeZone& timeZone, float x, float y) const
{
	// Determine if the time zone coordinates should
	// be interpreted as a rectangle or a polygon.
	switch (timeZone.type)
	{
	case RECTANGLE:
		return isPointInRectangle(timeZone.vertices, x, y);

	case POLYGON:
		return isPointInPolygon(timeZone.vertices, x, y);

	default:
		return false;
	}
}

// -----------------------------------------------------------------------------
bool FireRenderLocationCmd::isPointInRectangle(const vector<MFloatVector>& v, float x, float y) const
{
	MFloatVector min(
		fminf(v[0].x, v[1].x),
		fminf(v[0].y, v[1].y));

	MFloatVector max(
		fmaxf(v[0].x, v[1].x),
		fmaxf(v[0].y, v[1].y));

	return (x >= min.x && x <= max.x && y >= min.y && y <= max.y);
}

// -----------------------------------------------------------------------------
bool FireRenderLocationCmd::isPointInPolygon(const vector<MFloatVector>& v, float x, float y) const
{
	bool inside = false;
	size_t count = v.size();

	for (size_t i = 0, j = count - 1; i < count; j = i++)
	{
		if (((v[i].y > y) != (v[j].y > y)) &&
			(x < (v[j].x - v[i].x) * (y - v[i].y) / (v[j].y - v[i].y) + v[i].x))
			inside = !inside;
	}

	return inside;
}
