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
#include "RprComposite.h"

#include "FireRenderThread.h"
#include "FireRenderError.h"

RprComposite::RprComposite(rpr_context context, rpr_composite_type type)
{
	RPR_THREAD_ONLY;

	if (!context) checkStatus(RPR_ERROR_INTERNAL_ERROR);
	mContext = context;

	rpr_int status = rprContextCreateComposite(context, type, &mData);
	checkStatus(status);
}

RprComposite::~RprComposite()
{
	RPR_THREAD_ONLY;

	if (mData) rprObjectDelete(mData);
}

RprComposite::operator rpr_composite()
{
	RPR_THREAD_ONLY;

	return mData;
}

void RprComposite::SetInputC(const char *inputName, rpr_composite input)
{
	RPR_THREAD_ONLY;

	rpr_int status = rprCompositeSetInputC(mData, inputName, input);
	checkStatus(status);
}

void RprComposite::SetInput1U(const char *inputName, rpr_composite_type type)
{
	RPR_THREAD_ONLY;

	rpr_int status = rprCompositeSetInput1u(mData, inputName, type);
	checkStatus(status);
}

void RprComposite::SetInputFb(const char *inputName, rpr_framebuffer input)
{
	RPR_THREAD_ONLY;

	rpr_int status = rprCompositeSetInputFb(mData, inputName, input);
	checkStatus(status);
}

void RprComposite::SetInput4f(const char *inputName, float r, float g, float b, float a)
{
	RPR_THREAD_ONLY;

	rpr_int status = rprCompositeSetInput4f(mData, inputName, r, g, b, a);
	checkStatus(status);
}

void RprComposite::SetInputOp(const char *inputName, rpr_material_node_arithmetic_operation op)
{
	RPR_THREAD_ONLY;

	rpr_int status = rprCompositeSetInputOp(mData, inputName, op);
	checkStatus(status);
}
