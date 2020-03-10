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

#include "RadeonProRender.h"

/**
 * Class responsible for wrapping rpr_composite object.
 *
 * rpr_composite created in constructor. Exception thrown in case of error
 * rpr_composite destroyed in destructor.
 */
class RprComposite
{
public:
	RprComposite(rpr_context context, rpr_composite_type type);
	~RprComposite();
	operator rpr_composite ();

	void SetInputC(const char *inputName, rpr_composite input);
	void SetInputFb(const char *inputName, rpr_framebuffer input);
	void SetInput4f(const char *inputName, float r, float g, float b, float a);
	void SetInputOp(const char *inputName, rpr_material_node_arithmetic_operation op);
	void SetInput1U(const char *inputName, rpr_composite_type type);

private:
	rpr_composite mData = 0;
	rpr_context mContext = 0;
};
