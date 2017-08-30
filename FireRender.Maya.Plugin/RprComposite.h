#pragma once

#include "RadeonProRender.h"

class RprComposite
{
public:
	RprComposite(rpr_context context, rpr_composite_type type);
	~RprComposite();
	operator rpr_composite ();

	void SetInputC(const char *inputName, rpr_composite input);
	void SetInputFb(const char *inputName, rpr_framebuffer input);
	void SetInput4f(const char *inputName, float r, float g, float b, float a);

private:
	rpr_composite mData = 0;
	rpr_context mContext = 0;
};
