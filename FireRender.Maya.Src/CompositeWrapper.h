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
#include "frWrap.h"
#include "RprComposite.h"
#include "ImageFilter.h"

class CompositeWrapper
{
public:
	CompositeWrapper(const CompositeWrapper& other);
	CompositeWrapper(CompositeWrapper&& other);
	~CompositeWrapper();

	CompositeWrapper& operator= (CompositeWrapper&& other); // move assignment

	friend CompositeWrapper operator+ (const CompositeWrapper& w1, const CompositeWrapper& w2);
	friend CompositeWrapper operator- (const CompositeWrapper& w1, const CompositeWrapper& w2);
	friend CompositeWrapper operator* (const CompositeWrapper& w1, const CompositeWrapper& w2);
	static CompositeWrapper min(const CompositeWrapper& first, const CompositeWrapper& second);

	CompositeWrapper(frw::Context& context, rpr_framebuffer frameBuffer);
	CompositeWrapper(frw::Context& context, const float val);
	CompositeWrapper(frw::Context& context, const float val1, const float val2, const float val3, const float val4);
	CompositeWrapper(frw::Context& context, const float val, const float val4);

	void Compute(frw::FrameBuffer& out);

	// use for debugging
	const RprComposite& GetComposite(void);

protected:
	CompositeWrapper(rpr_context pContext, rpr_composite_type type);

protected:
	std::shared_ptr<RprComposite> m_composite;
	void* m_pContext;
};

