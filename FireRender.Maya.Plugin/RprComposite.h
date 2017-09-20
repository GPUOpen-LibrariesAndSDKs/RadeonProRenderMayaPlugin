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

private:
	rpr_composite mData = 0;
	rpr_context mContext = 0;
};
