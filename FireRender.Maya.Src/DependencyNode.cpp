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
#include "DependencyNode.h"
#include <maya/MPlug.h>

// Life Cycle
// -----------------------------------------------------------------------------
DependencyNode::DependencyNode(const MObject& object) :
	m_node(object)
{
}

// -----------------------------------------------------------------------------
DependencyNode::~DependencyNode()
{
}


// Public Methods
// -----------------------------------------------------------------------------
bool DependencyNode::getBool(const MString& name, bool defaultValue)
{
	return getValue(name, defaultValue);
}

// -----------------------------------------------------------------------------
float DependencyNode::getFloat(const MString& name, float defaultValue)
{
	return getValue(name, defaultValue);
}

// -----------------------------------------------------------------------------
double DependencyNode::getDouble(const MString& name, double defaultValue)
{
	return getValue(name, defaultValue);
}

// -----------------------------------------------------------------------------
int DependencyNode::getInt(const MString& name, int defaultValue)
{
	return getValue(name, defaultValue);
}

// -----------------------------------------------------------------------------
short DependencyNode::getShort(const MString& name, short defaultValue)
{
	return getValue(name, defaultValue);
}

// -----------------------------------------------------------------------------
MColor DependencyNode::getColor(const MString& name, MColor defaultValue)
{
	MColor color = defaultValue;
	MPlug plug = m_node.findPlug(name, false);

	if (!plug.isNull())
	{
		for (unsigned int i = 0; i < 3; i++)
		{
			const auto& p = plug.child(i);
			color[i] = p.asFloat();
		}
	}

	return color;
}


// Private Methods
// -----------------------------------------------------------------------------
template<typename T>
T DependencyNode::getValue(const MString& name, T defaultValue)
{
	T v = defaultValue;
	MPlug plug = m_node.findPlug(name, false);

	if (!plug.isNull())
		plug.getValue(v);

	return v;
}
