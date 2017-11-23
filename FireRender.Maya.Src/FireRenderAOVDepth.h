#pragma once
#include "FireRenderAOV.h"

class FireRenderAOVDepth : public FireRenderAOV
{
public:
	FireRenderAOVDepth(const FireRenderAOV& other);

	FireRenderAOVDepth(unsigned int id, const MString& attribute, const MString& name,
		const MString& folder, AOVDescription description);

	~FireRenderAOVDepth();
protected:
	void PostProcess() override;
	void ReadFromGlobals(const MFnDependencyNode& globals) override;

private:
	void InitValues();
	void SearchMinMax(float& minOut, float& maxOut);

private:
	bool m_bIsAutoNormalise;
	bool m_bIsInvertOutput;

	float m_fMin;
	float m_fMax;
};

