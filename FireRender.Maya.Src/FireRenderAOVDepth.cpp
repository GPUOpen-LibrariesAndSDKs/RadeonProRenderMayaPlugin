#include "FireRenderAOVDepth.h"
#include <maya/MGlobal.h>
#include <maya/MRenderView.h>
#include "FireRenderGlobals.h"


FireRenderAOVDepth::FireRenderAOVDepth(unsigned int id, const MString& attribute, const MString& name,
	const MString& folder, AOVDescription description) : 
	FireRenderAOV (id, attribute, name, folder, description)
{
	InitValues();
}

FireRenderAOVDepth::FireRenderAOVDepth(const FireRenderAOV& other) : FireRenderAOV(other)
{
	InitValues();
}


FireRenderAOVDepth::~FireRenderAOVDepth()
{
}

void FireRenderAOVDepth::InitValues()
{
	m_bIsAutoNormalise = false;
	m_bIsInvertOutput = false;

	m_fMin = 0.0f;
	m_fMax = 0.0f;
}

inline bool IsPixelDepthCorrectValue(const RV_PIXEL& pixel)
{
	return pixel.r > 0.0f;
}

inline float clamp(float val, float lower, float upper)
{
	return std::min<float>(upper, std::max<float>(val, lower));
}

void FireRenderAOVDepth::SearchMinMax(float& minOut, float& maxOut)
{
	RV_PIXEL* pData = pixels.get();

	float min = std::numeric_limits<float>::max();
	float max = std::numeric_limits<float>::lowest();

	// Search Min and Max values automatically
	int length = m_frameWidth * m_frameHeight;

	for (int index = 0; index < length; ++index)
	{
		if (IsPixelDepthCorrectValue(pData[index]))
		{
			float val = pData[index].r;

			if (min > val)
			{
				min = val;
			}

			if (max < val)
			{
				max = val;
			}
		}
	}

	minOut = min;
	maxOut = max;
}

void FireRenderAOVDepth::PostProcess()
{
	RV_PIXEL* pData = pixels.get();

	float min = m_fMin;
	float max = m_fMax;

	if (m_bIsAutoNormalise)
	{
		SearchMinMax(min, max);
	}

	for (unsigned int y = m_region.bottom; y <= m_region.top; y++)
	{
		for (unsigned int x = m_region.left; x <= m_region.right; x++)
		{
			int index = y * m_frameWidth + x;

			if (IsPixelDepthCorrectValue(pData[index]))
			{
				float val = pData[index].r;

				if (val >= max)
				{
					val = 1.0f;
				}
				else if (val <= min)
				{
					val = 0.0f;
				}
				else
				{
					val = (val - min) / (max - min);
				}		
				
				if (m_bIsInvertOutput)
				{
					val = 1.0f - val;
				}

				pData[index].r = val;
				pData[index].g = val;
				pData[index].b = val;
			}
		}
	}
}

void FireRenderAOVDepth::ReadFromGlobals(const MFnDependencyNode& globals)
{
	MPlug plug = globals.findPlug("aovDepthAutoNormalise");
	if (!plug.isNull())
		m_bIsAutoNormalise = plug.asBool();

	plug = globals.findPlug("aovDepthInvertOutput");
	if (!plug.isNull())
		m_bIsInvertOutput = plug.asBool();

	plug = globals.findPlug("aovDepthNormaliseMin");
	if (!plug.isNull())
		m_fMin = plug.asFloat();

	plug = globals.findPlug("aovDepthNormaliseMax");
	if (!plug.isNull())
		m_fMax = plug.asFloat();
}

