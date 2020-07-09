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
#include "CompositeWrapper.h"
#include "RprComposite.h"

CompositeWrapper::~CompositeWrapper()
{}

CompositeWrapper::CompositeWrapper(CompositeWrapper&& other)
	: m_pContext(other.m_pContext)
	, m_composite(other.m_composite)
{}

// want copy constructor to do move instead of copy
CompositeWrapper::CompositeWrapper(const CompositeWrapper& other)
	: m_pContext(other.m_pContext)
	, m_composite()
{
	CompositeWrapper& tmp = const_cast<CompositeWrapper&>(other);
	m_composite = tmp.m_composite;
}

CompositeWrapper& CompositeWrapper::operator= (CompositeWrapper&& other)
{
	m_pContext = other.m_pContext;
	m_composite = other.m_composite;
	return *this;
}

CompositeWrapper::CompositeWrapper(rpr_context pContext, rpr_composite_type type)
	: m_pContext(pContext)
	, m_composite(std::make_shared<RprComposite>(pContext, type))
{
}

CompositeWrapper::CompositeWrapper(frw::Context& context, rpr_framebuffer frameBuffer)
	: m_pContext(context.Handle())
	, m_composite(std::make_shared<RprComposite>(context.Handle(), RPR_COMPOSITE_FRAMEBUFFER))
{
	m_composite->SetInputFb("framebuffer.input", frameBuffer);
}

CompositeWrapper::CompositeWrapper(frw::Context& context, const float val)
	: m_pContext(context.Handle())
	, m_composite(std::make_shared<RprComposite>(context.Handle(), RPR_COMPOSITE_CONSTANT))
{
	m_composite->SetInput4f("constant.input", val, val, val, val);
}

CompositeWrapper::CompositeWrapper(frw::Context& context, const float val, const float val4)
	: m_pContext(context.Handle())
	, m_composite(std::make_shared<RprComposite>(context.Handle(), RPR_COMPOSITE_CONSTANT))
{
	m_composite->SetInput4f("constant.input", val, val, val, val4);
}

CompositeWrapper::CompositeWrapper(frw::Context& context, const float val1, const float val2, const float val3, const float val4)
	: m_pContext(context.Handle())
	, m_composite(std::make_shared<RprComposite>(context.Handle(), RPR_COMPOSITE_CONSTANT))
{
	m_composite->SetInput4f("constant.input", val1, val2, val3, val4);
}

CompositeWrapper operator+(const CompositeWrapper& w1, const CompositeWrapper& w2)
{
	CompositeWrapper res(w1.m_pContext, RPR_COMPOSITE_ARITHMETIC);
	RprComposite& summ = *res.m_composite;

	summ.SetInputC("arithmetic.color0", *w1.m_composite);
	summ.SetInputC("arithmetic.color1", *w2.m_composite);
	summ.SetInputOp("arithmetic.op", RPR_MATERIAL_NODE_OP_ADD);

	summ.SaveDependency(w1.m_composite);
	summ.SaveDependency(w2.m_composite);

	return res;
}

CompositeWrapper operator-(const CompositeWrapper& w1, const CompositeWrapper& w2)
{
	CompositeWrapper res(w1.m_pContext, RPR_COMPOSITE_ARITHMETIC);
	RprComposite& subt = *res.m_composite;

	subt.SetInputC("arithmetic.color0", *w1.m_composite);
	subt.SetInputC("arithmetic.color1", *w2.m_composite);
	subt.SetInputOp("arithmetic.op", RPR_MATERIAL_NODE_OP_SUB);

	subt.SaveDependency(w1.m_composite);
	subt.SaveDependency(w2.m_composite);

	return res;
}

CompositeWrapper operator*(const CompositeWrapper& w1, const CompositeWrapper& w2)
{
	CompositeWrapper res(w1.m_pContext, RPR_COMPOSITE_ARITHMETIC);
	RprComposite& mul = *res.m_composite;

	mul.SetInputC("arithmetic.color0", *w1.m_composite);
	mul.SetInputC("arithmetic.color1", *w2.m_composite);
	mul.SetInputOp("arithmetic.op", RPR_MATERIAL_NODE_OP_MUL);

	mul.SaveDependency(w1.m_composite);
	mul.SaveDependency(w2.m_composite);

	return res;
}

CompositeWrapper CompositeWrapper::min(const CompositeWrapper& first, const CompositeWrapper& second)
{
	CompositeWrapper res(first.m_pContext, RPR_COMPOSITE_ARITHMETIC);
	RprComposite& min = *res.m_composite;

	rpr_composite input1 = *first.m_composite;
	rpr_composite input2 = *second.m_composite;

	min.SetInputC("arithmetic.color0", input1);
	min.SetInputC("arithmetic.color1", input2);
	min.SetInputOp("arithmetic.op", RPR_MATERIAL_NODE_OP_MIN);

	min.SaveDependency(first.m_composite);
	min.SaveDependency(second.m_composite);

	return res;
}

void CompositeWrapper::Compute(frw::FrameBuffer& out)
{
	rpr_int frstatus = rprCompositeCompute(*m_composite, out.Handle());
	checkStatus(frstatus);
}

const RprComposite& CompositeWrapper::GetComposite(void)
{
	return *m_composite;
}

