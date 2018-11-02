#pragma once
#include <maya/MString.h>
#include <maya/MPlug.h>


MString getOptionVarNameForAttributeName(const MString& attrName);
MString getOptionVarMelCommand(const MString& flag, const MString& varName, const MString& value);
void setOptionVarIntValue(const MString& varName, int val);
void setOptionVarFloatValue(const MString& varName, float val);
void setOptionVarStringValue(const MString& varName, const MString& val);
int getOptionVarIntValue(const MString& varName);
float getOptionVarFloatValue(const MString& varName);
MString getOptionVarStringValue(const MString& varName);

void updateAttributeFromOptionVar(MPlug& plug, const MString& varName);
void updateCorrespondingOptionVar(const MPlug& plug);