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
#include "FileSystemUtils.h"
#include <fstream>

// -----------------------------------------------------------------------------
bool copyFile(const char* srcPath, const char* destPath)
{
	std::ifstream src(srcPath, std::ios::binary);
	std::ofstream dest(destPath, std::ios::binary);
	dest << src.rdbuf();

	return src && dest;
}

// -----------------------------------------------------------------------------
std::string getDirectory(std::string filePath)
{
	std::string directory = "";
	std::string fileName = filePath;
	size_t last_slash_idx = fileName.rfind('\\');
	if (std::string::npos != last_slash_idx)
	{
		bool again = false;
		if (last_slash_idx == (fileName.length() - 1)) {
			again = true;
		}
		directory = fileName.substr(0, last_slash_idx);
		if (again) {
			last_slash_idx = fileName.rfind('\\');
			directory = fileName.substr(0, last_slash_idx);
		}
		directory += "\\";
	}

	if (directory.length() == 0)
	{
		last_slash_idx = fileName.rfind('/');
		if (std::string::npos != last_slash_idx)
		{
			bool again = false;
			if (last_slash_idx == (fileName.length() - 1)) {
				again = true;
			}
			directory = fileName.substr(0, last_slash_idx);
			if (again) {
				last_slash_idx = fileName.rfind('/');
				directory = fileName.substr(0, last_slash_idx);
			}

			directory += "/";
		}
	}
	return directory;
}

// -----------------------------------------------------------------------------
MString getFullContainingDirectory(MString path, bool trailingSlash)
{
	int i = -1;
	bool complete = false;

	// Find the directory.
	while (!complete)
	{
		// Check for back or forward slashes.
		i = path.rindexW("\\");
		if (i < 0)
			i = path.rindexW("/");

		// Check for a trailing slash and remove if necessary.
		if (i == path.numChars() - 1)
			path = path.substringW(0, i);
		else
			complete = true;
	}

	// Check for no containing directory.
	if (i < 0)
		return "";

	// Return the containing directory, with or without a trailing slash.
	return path.substringW(0, i - (trailingSlash ? 0 : 1));
}

// -----------------------------------------------------------------------------
MString getContainingDirectory(MString path, bool trailingSlash)
{
	MString a = getFullContainingDirectory(path, false);
	MString b = getFullContainingDirectory(a, false);

	return path.substringW(b.numChars() + 1, a.numChars() - (trailingSlash ? 0 : 1));
}
