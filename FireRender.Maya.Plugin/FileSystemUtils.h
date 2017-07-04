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
