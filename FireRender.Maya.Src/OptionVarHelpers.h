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