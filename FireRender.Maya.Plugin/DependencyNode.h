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
