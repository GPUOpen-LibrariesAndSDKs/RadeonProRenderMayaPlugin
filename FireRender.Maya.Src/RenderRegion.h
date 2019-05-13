#pragma once

// Render region
// This class define a render region used inside a render view
class RenderRegion
{
public:

	// Constructor
	RenderRegion() :
		left(0), right(0), top(0), bottom(0) {}

	// Constructor
	RenderRegion(unsigned int l, unsigned int r, unsigned int t, unsigned int b) :
		left(l), right(r), top(t), bottom(b) {}

	// create region with given size and placed at 0,0
	RenderRegion(unsigned int w, unsigned int h) :
		left(0), right(w - 1), top(h - 1), bottom(0) {}

	// Copy constructor
	RenderRegion(const RenderRegion& other) :
		left(other.left), right(other.right), top(other.top), bottom(other.bottom) {}

	// Copy constructor
	RenderRegion& operator=(const RenderRegion& other)
	{
		left = other.left;
		right = other.right;
		top = other.top;
		bottom = other.bottom;
		return *this;
	}

	// Get the region width.
	unsigned int getWidth() const
	{
		return right - left + 1;
	}

	// Get the region height.
	unsigned int getHeight() const
	{
		return top - bottom + 1;
	}

	// Get the region area.
	unsigned int getArea() const
	{
		return getWidth() * getHeight();
	}

	// Return true if the region has non-zero size in both dimensions.
	bool isZeroArea() const
	{
		return getWidth() <= 0 || getHeight() <= 0;
	}


public:

	// Left coordinate
	unsigned int left;

	// Right coordinate
	unsigned int right;

	// Top coordinate
	unsigned int top;

	// Bottom coordinate
	unsigned int bottom;
};
