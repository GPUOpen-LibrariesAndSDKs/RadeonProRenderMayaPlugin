#include "athenaSystemInfo_Win.h"
#include <sstream>
#include <codecvt>
#include <vector>

#if defined(_WIN32)
bool ReadRegValue(HKEY& hKey, const std::string& valueName, std::string& out)
{
	DWORD valueSize = MAX_PATH;
	UINT uiRetVal = ERROR_SUCCESS;
	std::vector<char> buffer;

	do
	{
		buffer.resize(valueSize, 0);
		uiRetVal = RegQueryValueExA(hKey, TEXT(valueName.c_str()), NULL, NULL, (LPBYTE)buffer.data(), &valueSize);
		valueSize *= 2;
	} 
	while (uiRetVal == ERROR_MORE_DATA);

	if (uiRetVal != ERROR_SUCCESS)
		return false;

	out = std::string(buffer.data());
	return true;
}

void getOSName(std::string& osName, std::string& osVersion)
{
	HKEY hKey = NULL;
	UINT uiRetVal = RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion"), 0, KEY_READ, &hKey);
	if (uiRetVal != ERROR_SUCCESS)
		return;

	osName = "Unknown OS";
	ReadRegValue(hKey, "ProductName", osName);

	std::string osBuild;
	std::string osCurrVersion;

	bool res = ReadRegValue(hKey, "CurrentBuild", osBuild) && ReadRegValue(hKey, "CurrentVersion", osCurrVersion);
	RegCloseKey(hKey);

	if (!res)
		return;

	osVersion = osCurrVersion + "." + osBuild;
}

void getTimeZone(std::string& timezoneName)
{
	TIME_ZONE_INFORMATION TimeZoneInfo;
	GetTimeZoneInformation(&TimeZoneInfo);

	WCHAR* tzName = TimeZoneInfo.StandardName;
	std::wostringstream ssTzName;
	ssTzName << tzName;

	using convert_typeX = std::codecvt_utf8<wchar_t>;
	std::wstring_convert<convert_typeX, wchar_t> converterX;
	timezoneName = converterX.to_bytes(ssTzName.str());
}

int getNumCPUCores()
{
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	int numCPU = sysinfo.dwNumberOfProcessors;

	return numCPU;
}

#endif
