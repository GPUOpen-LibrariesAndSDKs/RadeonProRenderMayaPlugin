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

#include <maya/MObject.h>
#include <maya/MColor.h>
#include <maya/MFnDependencyNode.h>

/**
 * A wrapper class to simplify the process
 * of reading attributes from Maya objects.
 */
class DependencyNode
{

public:

	// Life Cycle
	// -----------------------------------------------------------------------------

	DependencyNode(const MObject& object);
	virtual ~DependencyNode();


	// Public Methods
	// -----------------------------------------------------------------------------

	/** Get a boolean value. */
	bool getBool(const MString& name, bool defaultValue = false);

	/** Get a floating point value. */
	float getFloat(const MString& name, float defaultValue = 0);

	/** Get a double precision floating point value. */
	double getDouble(const MString& name, double defaultValue = 0);

	/** Get an integer value. */
	int getInt(const MString& name, int defaultValue = 0);

	/** Get a short integer value. */
	short getShort(const MString& name, short defaultValue = 0);

	/** Get a color value . */
	MColor getColor(const MString& name, MColor defaultValue = MColor(0, 0, 0));


public:

	// Members
	// -----------------------------------------------------------------------------

	/** The Maya dependency graph node. */
	MFnDependencyNode m_node;

	// Private Methods
	// -----------------------------------------------------------------------------

	/** Get a value of the specified type. */
	template<typename T>
	T getValue(const MString& name, T defaultValue);

};
