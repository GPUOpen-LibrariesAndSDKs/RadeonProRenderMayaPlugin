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

#include <string>
#include <maya/MString.h>

/** Perform a binary file copy. */
bool copyFile(const char* srcPath, const char* destPath);

/** Given a full file path, return the containing directory. */
std::string getDirectory(std::string filePath);

/**
 * Return the full containing directory path
 * for the specified file or directory path.
 */
MString getFullContainingDirectory(MString path, bool trailingSlash = false);

/**
 * Return the containing directory for the specified
 * file or directory path as a relative path.
 */
MString getContainingDirectory(MString path, bool trailingSlash = false);
