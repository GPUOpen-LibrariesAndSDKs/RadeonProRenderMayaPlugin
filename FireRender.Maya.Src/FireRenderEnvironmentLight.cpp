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
#include "FireRenderEnvironmentLight.h"

#include "FireMaya.h"

#include <maya/MFnTypedAttribute.h>
#include <maya/MFnMessageAttribute.h>

MObject FireRenderEnvironmentLight::aColor;
MObject	FireRenderEnvironmentLight::aIntensity;
MObject	FireRenderEnvironmentLight::aDisplay;
MObject	FireRenderEnvironmentLight::aPortal;

MTypeId FireRenderEnvironmentLight::id(FireMaya::TypeId::FireRenderEnvironmentLight);

MString FireRenderEnvironmentLight::drawDbClassification("drawdb/geometry/FireRenderEnvLight");
MString FireRenderEnvironmentLight::drawRegistrantId("FireRenderEnvironmentLightNode");


FireRenderEnvironmentLight::FireRenderEnvironmentLight()
{
}


FireRenderEnvironmentLight::~FireRenderEnvironmentLight()
{
}

void * FireRenderEnvironmentLight::creator()
{
	return new FireRenderEnvironmentLight();
}

template <class T>
void makeAttribute(T& attr)
{
	CHECK_MSTATUS(attr.setKeyable(true));
	CHECK_MSTATUS(attr.setStorable(true));
	CHECK_MSTATUS(attr.setReadable(true));
	CHECK_MSTATUS(attr.setWritable(true));
}

MStatus FireRenderEnvironmentLight::initialize()
{
	MFnNumericAttribute nAttr;
	MFnTypedAttribute tAttr;
	MFnMessageAttribute mAttr;

	aIntensity = nAttr.create("intensity", "i", MFnNumericData::kFloat, 1.0);
	nAttr.setKeyable(true);
	nAttr.setStorable(true);
	nAttr.setReadable(true);
	nAttr.setWritable(true);
	nAttr.setSoftMin(0);
	nAttr.setSoftMax(10);
	addAttribute(aIntensity);

	aDisplay = nAttr.create("display", "d", MFnNumericData::kBoolean, 1);
	nAttr.setKeyable(true);
	nAttr.setStorable(true);
	nAttr.setReadable(true);
	nAttr.setWritable(true);
	addAttribute(aDisplay);

	aPortal = mAttr.create("portal", "p");
	addAttribute(aPortal);


	aColor = nAttr.createColor("color", "col");
	makeAttribute(nAttr);
	CHECK_MSTATUS(nAttr.setDefault(1.0f, 1.0f, 1.0f));
	addAttribute(aColor);

	return MS::kSuccess;
}
