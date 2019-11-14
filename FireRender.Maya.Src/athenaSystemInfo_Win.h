#pragma once
#include <string>
#include <wtypes.h>

#if defined(_WIN32)
bool ReadRegValue(HKEY& hKey, const std::string& valueName, std::string& out);

void getOSName(std::string& osName, std::string& osVersion);

void getTimeZone(std::string& timezoneName);

int getNumCPUCores(void);
#endif

