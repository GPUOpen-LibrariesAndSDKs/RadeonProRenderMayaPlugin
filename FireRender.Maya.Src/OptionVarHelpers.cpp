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
#include "OptionVarHelpers.h"

#include <assert.h>
#include <string>

#include <maya/MGlobal.h>
#include <maya/MPlug.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnStringArrayData.h>
#include <maya/MFnStringData.h>

#include <maya/MDGModifier.h>

MString getOptionVarNameForAttributeName(const MString& attrName)
{
	return "RPR_" + attrName;
}

MString getOptionVarMelCommand(const MString& flag, const MString& varName, const MString& value)
{
	MString str;
	str.format("optionVar ^1s ^2s ^3s", flag, varName, value);
	return str;
}

void setOptionVarIntValue(const MString& varName, int val)
{
	MGlobal::executeCommand(getOptionVarMelCommand("-iv", varName, std::to_string(val).c_str()));
}

void setOptionVarFloatValue(const MString& varName, float val)
{
	MGlobal::executeCommand(getOptionVarMelCommand("-fv", varName, std::to_string(val).c_str()));
}

void setOptionVarStringValue(const MString& varName, const MString& val)
{
	MGlobal::executeCommand(getOptionVarMelCommand("-sv", varName, "\"" + val + "\""));
}

int getOptionVarIntValue(const MString& varName)
{
	MString command = getOptionVarMelCommand("-q", varName, "");

	int val;
	MGlobal::executeCommand(command, val);

	return val;
}

float getOptionVarFloatValue(const MString& varName)
{
	MString command = getOptionVarMelCommand("-q", varName, "");

	double val;
	MGlobal::executeCommand(command, val);

	return (float) val;
}

MString getOptionVarStringValue(const MString& varName)
{
	MString command = getOptionVarMelCommand("-q", varName, "");

	MString val;
	MStatus res = MGlobal::executeCommand(command, val);

	MString out = val;
	return out;
}

void updateAttributeFromOptionVar(MPlug& plug, const MString& varName)
{
	MObject attrObj = plug.attribute();

	MDGModifier dg;

	if (attrObj.hasFn(MFn::kNumericAttribute))
	{
		MFnNumericAttribute numAttr(attrObj);

		MString optionVarName = getOptionVarNameForAttributeName(numAttr.name());

		switch (numAttr.unitType())
		{
		case MFnNumericData::Type::kBoolean:
		{
			int val = getOptionVarIntValue(varName);

			dg.newPlugValueBool(plug, val > 0);
			dg.doIt();
			break;
		}
		case MFnNumericData::Type::kInt:
		{
			int val = getOptionVarIntValue(varName);

			dg.newPlugValueInt(plug, val);
			dg.doIt();
			break;
		}
		case MFnNumericData::Type::kFloat:
		{
			float val = getOptionVarFloatValue(varName);

			dg.newPlugValueFloat(plug, val);
			dg.doIt();
			break;
		}
		default:
			assert(false);
		}
	}
	else if (attrObj.hasFn(MFn::kEnumAttribute))
	{
		MFnEnumAttribute eAttr(attrObj);
		MString optionVarName = getOptionVarNameForAttributeName(eAttr.name());
		int val = getOptionVarIntValue(varName);

		dg.newPlugValueInt(plug, val);
		dg.doIt();
	}
	else if (attrObj.hasFn(MFn::kTypedAttribute))
	{
		MFnTypedAttribute tAttr(attrObj);

		MString optionVarName = getOptionVarNameForAttributeName(tAttr.name());

		switch (tAttr.attrType())
		{
		case MFnData::Type::kString:
		{
			MString val = getOptionVarStringValue(varName);

			dg.newPlugValueString(plug, val);
			dg.doIt();

			break;
		}
		default:
			assert(false);
		}
	}
	else
	{
		assert(false);
	}
}

void updateCorrespondingOptionVar(const MPlug& plug)
{
	MObject attrObj = plug.attribute();

	if (attrObj.hasFn(MFn::kNumericAttribute))
	{
		MFnNumericAttribute numAttr(attrObj);

		MString optionVarName = getOptionVarNameForAttributeName(numAttr.name());

		MFnNumericData::Type type = numAttr.unitType();

		switch (numAttr.unitType())
		{
		case MFnNumericData::Type::kBoolean:
		case MFnNumericData::Type::kInt:
		{
			int val;
			plug.getValue(val);
			setOptionVarIntValue(optionVarName, val);
			break;
		}
		case MFnNumericData::Type::kFloat:
		{
			float val;
			plug.getValue(val);
			setOptionVarFloatValue(optionVarName, val);
			break;
		}
		default:
			assert(false);
		}
	}
	else if (attrObj.hasFn(MFn::kEnumAttribute))
	{
		MFnEnumAttribute enumAttr(attrObj);

		MString optionVarName = getOptionVarNameForAttributeName(enumAttr.name());

		int val;
		plug.getValue(val);
		setOptionVarIntValue(optionVarName, val);
	}
	else if (attrObj.hasFn(MFn::kTypedAttribute))
	{
		MFnTypedAttribute tAttr(attrObj);

		MString optionVarName = getOptionVarNameForAttributeName(tAttr.name());

		MFnData::Type type = tAttr.attrType();

		switch (type)
		{
		case MFnData::Type::kString:
		{
			MStatus res;
			MString val;
			val = plug.asString(&res);
			setOptionVarStringValue(optionVarName, val);

			break;
		}
		default:
			assert(false); // souldn't be here
		}
	}
	else
	{
		assert(false);
	}
}
