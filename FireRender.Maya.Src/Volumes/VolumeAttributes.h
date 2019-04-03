#pragma once

#include "FireMaya.h"
#include "FireRenderUtils.h"

#include <maya/MObject.h>
#include <maya/MColor.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MDataHandle.h>
#include <maya/MRampAttribute.h>

#include <vector>

// This is the data fields for Volume representation used by RPR Volume Node.
// These are also all data values that are supported by RPR.
struct VolumeData
{
	size_t gridSizeX;
	size_t gridSizeY;
	size_t gridSizeZ;

	std::vector<float> albedoLookupCtrlPoints;
	std::vector<float> albedoVal;

	std::vector<float> emissionLookupCtrlPoints;
	std::vector<float> emissionVal;

	std::vector<float> denstiyLookupCtrlPoints;
	std::vector<float> densityVal;

	struct VolumeVoxel
	{
		// albedo
		float aR;
		float aG;
		float aB;

		// emission
		float eR;
		float eG;
		float eB;

		// density
		float density;

		VolumeVoxel(void)
			: aR (0.0f)
			, aG (0.0f)
			, aB (1.0f)
			, eR (0.0f)
			, eG (0.0f)
			, eB (0.0f)
			, density (1.0f)
		{}
	};

	std::vector<VolumeVoxel> voxels;

	bool IsValid (void) { return ((voxels.size() > 0) && (voxels.size() == (gridSizeX*gridSizeY*gridSizeZ) ) ); }

	VolumeData()
		: albedoLookupCtrlPoints()
		, albedoVal()
		, emissionLookupCtrlPoints()
		, denstiyLookupCtrlPoints()
		, voxels()
		, gridSizeX(1)
		, gridSizeY(1)
		, gridSizeZ(1)
	{}
};

// This enum is used to set a way how ramps inputs should be interpreted.
// Notice that these are the same enum values that are used by maya volume node.
enum VolumeGradient
{
	kConstant = 4, // value is set to one across the volume
	kXGradient, // ramp the value from zero to one along the X axis
	kYGradient, // ramp the value from zero to one along the Y axis
	kZGradient, // ramp the value from zero to one along the Z axis
	kNegXGradient, // ramp the value from one to zero along the X axis
	kNegYGradient, // ramp the value from one to zero along the Y axis
	kNegZGradient, // ramp the value from one to zero along the Z axis
	kCenterGradient = 11, // ramps the value from one at the center to zero at the edges
};

// This is the class that describes attributes of RPR Volume node that are visible in Maya
class RPRVolumeAttributes : public MPxNode
{
public:
	RPRVolumeAttributes();
	~RPRVolumeAttributes();

	virtual void postConstructor() override;

	static void Initialize();

	static MDataHandle GetVolumeGridDimentions(const MFnDependencyNode& node);

	static bool GetAlbedoEnabled(const MFnDependencyNode& node);
	static VolumeGradient GetAlbedoGradientType(const MFnDependencyNode& node);
	static MPlug GetAlbedoRamp(const MFnDependencyNode& node);

	static bool GetEmissionEnabled(const MFnDependencyNode& node);
	static VolumeGradient GetEmissionGradientType(const MFnDependencyNode& node);
	static MPlug GetEmissionValueRamp(const MFnDependencyNode& node);
	static float GetEmissionIntensity(const MFnDependencyNode& node);
	static MPlug GetEmissionIntensityRamp(const MFnDependencyNode& node);

	static bool GetDensityEnabled(const MFnDependencyNode& node);
	static VolumeGradient GetDensityGradientType(const MFnDependencyNode& node);
	static MPlug GetDensityRamp(const MFnDependencyNode& node);
	static float GetDensityMultiplier(const MFnDependencyNode& node);

	static void FillVolumeData(VolumeData& data, const MObject& node, FireMaya::Scope* scope);

public:
	// General
	static MObject volumeDimentions;

	/*
	We will probably add noise parameters to each of inputs below
	*/

	// Albedo
	static MObject albedoEnabled;
	static MObject albedoGradType; // gradient type
	static MObject albedoValue;    // values ramp

	// Emission
	static MObject emissionEnabled;
	static MObject emissionGradType; // gradient type
	static MObject emissionValue;    // values ramp
	static MObject emissionIntensity; // intensity of emission light source
	static MObject emissionRamp; // ramp used to set different intensity across volume

	// Density
	static MObject densityEnabled;
	static MObject densityGradType; // gradient type
	static MObject densityValue;    // values ramp
	static MObject densityMultiplier; // density is multiplied by this value
};
