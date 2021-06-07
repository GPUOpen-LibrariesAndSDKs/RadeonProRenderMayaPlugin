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


#ifdef _WIN32
#include <windows.h>
#include <winreg.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#endif
#include "stdafx.h"
#include <msi.h>
#include <msiquery.h>
#include <Shellapi.h>
#include <RadeonProRender.h> // for get RPR_API_VERSION


#pragma comment(lib, "msi.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Ole32.lib")

#define VERSION_MAJOR(ver) ((ver) >> 28)
#define VERSION_MINOR(ver) ((ver) & 0xfffffff)

std::vector<std::wstring> getMayaVersionWithInstalledPlugin(MSIHANDLE hInstall)
{
	std::vector<std::wstring> versions = {
		L"2019",
		L"2020",
		L"2022"
	};

	std::vector<std::wstring> res;

	for (int i = 0; i < versions.size(); i++)
	{
		TCHAR val[MAX_PATH];
		DWORD valLen = MAX_PATH;

		// like MAYA2016_INSTALLED
		std::wstring propertyName = L"MAYA" + versions[i] + L"_INSTALLED";
		UINT res2 = MsiGetProperty(hInstall, propertyName.c_str(), val, &valLen);
		std::wstring strVal = val;
		if (strVal.empty())
			continue;
		res.push_back(versions[i]);
	}

	return res;
}

bool getMayaPythonDirectory(const std::string& dirName_in, std::string& retPath)
{
	std::string regPath = "SOFTWARE\\Autodesk\\Maya\\" + dirName_in + "\\Setup\\InstallPath";

	HKEY hKey = HKEY();
	if (::RegOpenKeyExA(HKEY_LOCAL_MACHINE, regPath.c_str(), 0, KEY_READ, &hKey) != ERROR_SUCCESS)
	{
		LogSystem(std::string("Could not find path " + regPath + " in the registry!\n").c_str());

		return false;
	}

	char szPath[MAX_PATH] = {};
	DWORD pathSize = sizeof(szPath) / sizeof(szPath[0]);
	LSTATUS status = ::RegGetValueA(hKey, NULL, "MAYA_INSTALL_LOCATION", RRF_RT_ANY, 0, szPath, &pathSize);
	if (status != ERROR_SUCCESS)
	{
		LogSystem(std::string( "failed to read registry record with error code: " + std::to_string(status) + "\n").c_str());

		return false;
	}

	std::string fullPath (szPath);
	fullPath += "bin";

	retPath = fullPath;

	LogSystem(std::string("checking if file path " + fullPath + " exists...\n").c_str());

	DWORD ftyp = GetFileAttributesA(fullPath.c_str());
	if (ftyp == INVALID_FILE_ATTRIBUTES)
	{
		LogSystem(std::string("internal eror when trying to analyze path: " + fullPath + " \n").c_str());

		return false;  // something is wrong with your path!	
	}

	if (ftyp & FILE_ATTRIBUTE_DIRECTORY)
	{
		LogSystem(std::string("Success! file path: " + fullPath + " exists\n").c_str());

		return true;   // this is a directory!
	}

	LogSystem(std::string("file path: " + fullPath + " doesn't exist!\n").c_str());

	return false;    // this is not a directory!
}

void installBoto3()
{
	LogSystem("installBoto3\n");

	// We install boto3 only on Maya versions with Python 3 inside
	const static std::vector<std::string> versions = {
		{"2022"}
	};

	for (const std::string& version : versions)
	{
		LogSystem(std::string("considering installBoto3 on " + version + "\n").c_str());

		std::string fullPath;
		if (!getMayaPythonDirectory(version, fullPath))
			continue;

		SetCurrentDirectoryA(fullPath.c_str());

		std::string cmdScript = "mayapy -m pip install boto3\n";

		try
		{
			LogSystem(std::string("try installBoto3 on " + version + "\n").c_str());

			int res = system(cmdScript.c_str());

			LogSystem(std::string("res = " + std::to_string(res) + "\n").c_str());
		}
		catch(...)
		{
			LogSystem(std::string("failed installBoto3 on " + version + "\n").c_str());
		}
	}
}


void setAutoloadPlugin(const std::wstring &maya_version)
{
	std::wstring destFolder = GetSystemFolderPaths(CSIDL_MYDOCUMENTS) + L"\\maya\\" + maya_version + L"\\prefs";
	std::wstring destF = destFolder + L"\\pluginPrefs.mel";

	std::string autoLoadLine = "evalDeferred(\"autoLoadPlugin(\\\"\\\", \\\"RadeonProRender\\\", \\\"RadeonProRender\\\")\");";

	std::fstream infile(destF);
	std::string line;
	bool found = false;
	while (std::getline(infile, line))
	{
		if (line.compare(autoLoadLine) == 0) {
			found = true;
			break;
		}
	}
	infile.close();

	if (!found) {
		std::fstream oFile(destF, std::ios::app);
		oFile << std::endl;
		oFile << autoLoadLine << std::endl;
	}
}

extern "C" __declspec(dllexport) UINT postInstall(MSIHANDLE hInstall) 
{
	LogSystem("postInstall\n");

	std::vector<std::wstring> versions = getMayaVersionWithInstalledPlugin(hInstall);

	LogSystem(std::string("after getMayaVersionWithInstalledPlugin, versions.size = " + std::to_string(versions.size()) + "\n").c_str());

	for (size_t i = 0; i < versions.size(); i++)
	{
		setAutoloadPlugin(versions[i]);
	}

	return ERROR_SUCCESS;
}

extern "C" __declspec(dllexport) UINT botoInstall(MSIHANDLE hInstall)
{
	LogSystem("botoInstall\n");

	installBoto3();

	return ERROR_SUCCESS;
}
