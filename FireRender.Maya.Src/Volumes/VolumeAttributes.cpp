#include "VolumeAttributes.h"
#include "FireRenderUtils.h"
#include <maya/MPxNode.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <array>

MObject RPRVolumeAttributes::volumeDimentions;
MObject RPRVolumeAttributes::albedoEnabled;
MObject RPRVolumeAttributes::albedoGradType;
MObject RPRVolumeAttributes::albedoValue;
MObject RPRVolumeAttributes::emissionEnabled;
MObject RPRVolumeAttributes::emissionGradType;
MObject RPRVolumeAttributes::emissionValue;
MObject RPRVolumeAttributes::emissionIntensity;
MObject RPRVolumeAttributes::emissionRamp;
MObject RPRVolumeAttributes::densityEnabled;
MObject RPRVolumeAttributes::densityGradType;
MObject RPRVolumeAttributes::densityValue;
MObject RPRVolumeAttributes::densityMultiplier;

MStatus postConstructor_initialise_ramp_curve(MObject& parentNode, MObject& rampObj, int index, float position, float value, int interpolation)
{
	MStatus status;

	MPlug rampPlug(parentNode, rampObj);

	MPlug elementPlug = rampPlug.elementByLogicalIndex(index, &status);

	MPlug positionPlug = elementPlug.child(0, &status);
	status = positionPlug.setFloat(position);

	MPlug valuePlug = elementPlug.child(1);
	status = valuePlug.setFloat(value);

	MPlug interpPlug = elementPlug.child(2);
	interpPlug.setInt(interpolation);

	return MS::kSuccess;
}

MStatus postConstructor_initialise_color_curve(MObject& parentNode, MObject& rampObj, int index, float position, MColor value, int interpolation)
{
	MStatus status;

	MPlug rampPlug(parentNode, rampObj);

	MPlug elementPlug = rampPlug.elementByLogicalIndex(index, &status);

	MPlug positionPlug = elementPlug.child(0, &status);
	status = positionPlug.setFloat(position);

	MPlug valuePlug = elementPlug.child(1);
	status = valuePlug.child(0).setFloat(value.r);
	status = valuePlug.child(1).setFloat(value.g);
	status = valuePlug.child(2).setFloat(value.b);

	MPlug interpPlug = elementPlug.child(2);
	interpPlug.setInt(interpolation);

	return MS::kSuccess;
}

void RPRVolumeAttributes::Initialize()
{
	MFnNumericAttribute nAttr;
	MFnEnumAttribute eAttr;
	MRampAttribute rAttr;

	// General
	volumeDimentions = nAttr.create("volumeDimentions", "dims", MFnNumericData::k3Short);
	setAttribProps(nAttr, volumeDimentions);
	CHECK_MSTATUS(nAttr.setDefault(4, 4, 4));
	nAttr.setMin(1, 1, 1);
	nAttr.setMax(10000, 10000, 10000);

	// Albedo
	albedoEnabled = nAttr.create("albedoEnabled", "ealb", MFnNumericData::kBoolean, 0);
	setAttribProps(nAttr, albedoEnabled);

	albedoGradType = eAttr.create("albedoGradType", "talb", VolumeGradient::kCenterGradient);
	eAttr.addField("Constant", VolumeGradient::kConstant);
	eAttr.addField("X Gradient", VolumeGradient::kXGradient);
	eAttr.addField("Y Gradient", VolumeGradient::kYGradient);
	eAttr.addField("Z Gradient", VolumeGradient::kZGradient);
	eAttr.addField("Neg X Gradient", VolumeGradient::kNegXGradient);
	eAttr.addField("Neg Y Gradient", VolumeGradient::kNegYGradient);
	eAttr.addField("Neg Z Gradient", VolumeGradient::kNegZGradient);
	eAttr.addField("Center Gradient", VolumeGradient::kCenterGradient);
	setAttribProps(nAttr, albedoGradType);

	MStatus ret;
	albedoValue = rAttr.createColorRamp("albedoValue", "valb", &ret);
	CHECK_MSTATUS(ret);
	ret = MPxNode::addAttribute(albedoValue);
	CHECK_MSTATUS(ret);

	// Emission
	emissionEnabled = nAttr.create("emissionEnabled", "eems", MFnNumericData::kBoolean, 0);
	setAttribProps(nAttr, emissionEnabled);

	emissionGradType = eAttr.create("emissionGradType", "tems", VolumeGradient::kCenterGradient);
	eAttr.addField("Constant", VolumeGradient::kConstant);
	eAttr.addField("X Gradient", VolumeGradient::kXGradient);
	eAttr.addField("Y Gradient", VolumeGradient::kYGradient);
	eAttr.addField("Z Gradient", VolumeGradient::kZGradient);
	eAttr.addField("Neg X Gradient", VolumeGradient::kNegXGradient);
	eAttr.addField("Neg Y Gradient", VolumeGradient::kNegYGradient);
	eAttr.addField("Neg Z Gradient", VolumeGradient::kNegZGradient);
	eAttr.addField("Center Gradient", VolumeGradient::kCenterGradient);
	setAttribProps(nAttr, emissionGradType);

	emissionValue = rAttr.createColorRamp("emissionValue", "vems", &ret);
	CHECK_MSTATUS(ret);
	ret = MPxNode::addAttribute(emissionValue);
	CHECK_MSTATUS(ret);

	emissionIntensity = nAttr.create("emissionIntensity", "iems", MFnNumericData::kFloat, 1.0f);
	setAttribProps(nAttr, emissionIntensity);
	nAttr.setMin(0.0f);
	nAttr.setSoftMax(1000.0f);

	emissionRamp = rAttr.createCurveRamp("emissionRamp", "rems", &ret);
	CHECK_MSTATUS(ret);
	ret = MPxNode::addAttribute(emissionRamp);
	CHECK_MSTATUS(ret);

	// Density
	densityEnabled = nAttr.create("densityEnabled", "edns", MFnNumericData::kBoolean, 0);
	setAttribProps(nAttr, densityEnabled);

	densityGradType = eAttr.create("densityGradType", "tdns", VolumeGradient::kCenterGradient);
	eAttr.addField("Constant", VolumeGradient::kConstant);
	eAttr.addField("X Gradient", VolumeGradient::kXGradient);
	eAttr.addField("Y Gradient", VolumeGradient::kYGradient);
	eAttr.addField("Z Gradient", VolumeGradient::kZGradient);
	eAttr.addField("Neg X Gradient", VolumeGradient::kNegXGradient);
	eAttr.addField("Neg Y Gradient", VolumeGradient::kNegYGradient);
	eAttr.addField("Neg Z Gradient", VolumeGradient::kNegZGradient);
	eAttr.addField("Center Gradient", VolumeGradient::kCenterGradient);
	setAttribProps(nAttr, densityGradType);

	densityValue = rAttr.createCurveRamp("densityValue", "vdns", &ret);
	CHECK_MSTATUS(ret);
	ret = MPxNode::addAttribute(densityValue);
	CHECK_MSTATUS(ret);

	densityMultiplier = nAttr.create("densityMultiplier", "kdns", MFnNumericData::kFloat, 100.0f);
	setAttribProps(nAttr, densityMultiplier);
	nAttr.setMin(0.0f);
	nAttr.setSoftMax(1000.0f);
}

void RPRVolumeAttributes::postConstructor()
{
	setMPSafe(true);
	MStatus status = setExistWithoutInConnections(true);
	CHECK_MSTATUS(status);

	postConstructor_initialise_color_curve(thisMObject(), albedoValue, 0, 0.0f, MColor(0.0f, 0.0f, 1.0f), 1);
	postConstructor_initialise_color_curve(thisMObject(), emissionValue, 0, 0.0f, MColor(0.0f, 0.0f, 0.0f), 1);
	postConstructor_initialise_ramp_curve(thisMObject(), emissionValue, 0, 0.0f, 0.1f, 1);
}

MDataHandle RPRVolumeAttributes::GetVolumeGridDimentions(const MFnDependencyNode& node)
{
	MPlug plug = node.findPlug(RPRVolumeAttributes::volumeDimentions);

	assert(!plug.isNull());
	if (!plug.isNull())
	{
		return plug.asMDataHandle();
	}

	return MDataHandle();
}

bool RPRVolumeAttributes::GetAlbedoEnabled(const MFnDependencyNode& node)
{
	MPlug plug = node.findPlug(RPRVolumeAttributes::albedoEnabled);

	assert(!plug.isNull());
	if (!plug.isNull())
	{
		return plug.asBool();
	}

	return false;
}

VolumeGradient RPRVolumeAttributes::GetAlbedoGradientType(const MFnDependencyNode& node)
{
	MPlug plug = node.findPlug(RPRVolumeAttributes::albedoGradType);

	assert(!plug.isNull());

	if (!plug.isNull())
	{
		return (VolumeGradient)plug.asInt();
	}

	return kConstant;
}

MPlug RPRVolumeAttributes::GetAlbedoRamp(const MFnDependencyNode& node)
{
	MPlug plug = node.findPlug(RPRVolumeAttributes::albedoValue);

	assert(!plug.isNull());

	return plug;
}

bool RPRVolumeAttributes::GetEmissionEnabled(const MFnDependencyNode& node)
{
	MPlug plug = node.findPlug(RPRVolumeAttributes::emissionEnabled);

	assert(!plug.isNull());
	if (!plug.isNull())
	{
		return plug.asBool();
	}

	return false;
}

VolumeGradient RPRVolumeAttributes::GetEmissionGradientType(const MFnDependencyNode& node)
{
	MPlug plug = node.findPlug(RPRVolumeAttributes::emissionGradType);

	assert(!plug.isNull());

	if (!plug.isNull())
	{
		return (VolumeGradient)plug.asInt();
	}

	return kConstant;
}

MPlug RPRVolumeAttributes::GetEmissionValueRamp(const MFnDependencyNode& node)
{
	MPlug plug = node.findPlug(RPRVolumeAttributes::emissionValue);

	assert(!plug.isNull());

	return plug;
}

float RPRVolumeAttributes::GetEmissionIntensity(const MFnDependencyNode& node)
{
	MPlug plug = node.findPlug(RPRVolumeAttributes::emissionIntensity);

	assert(!plug.isNull());
	if (!plug.isNull())
	{
		return plug.asFloat();
	}

	return 1.0f;
}

MPlug RPRVolumeAttributes::GetEmissionIntensityRamp(const MFnDependencyNode& node)
{
	MPlug plug = node.findPlug(RPRVolumeAttributes::emissionRamp);

	assert(!plug.isNull());

	return plug;
}

bool RPRVolumeAttributes::GetDensityEnabled(const MFnDependencyNode& node)
{
	MPlug plug = node.findPlug(RPRVolumeAttributes::densityEnabled);

	assert(!plug.isNull());
	if (!plug.isNull())
	{
		return plug.asBool();
	}

	return false;
}

VolumeGradient RPRVolumeAttributes::GetDensityGradientType(const MFnDependencyNode& node)
{
	MPlug plug = node.findPlug(RPRVolumeAttributes::densityGradType);

	assert(!plug.isNull());

	if (!plug.isNull())
	{
		return (VolumeGradient)plug.asInt();
	}

	return kConstant;
}

MPlug RPRVolumeAttributes::GetDensityRamp(const MFnDependencyNode& node)
{
	MPlug plug = node.findPlug(RPRVolumeAttributes::densityValue);

	assert(!plug.isNull());

	return plug;
}

float RPRVolumeAttributes::GetDensityMultiplier(const MFnDependencyNode& node)
{
	MPlug plug = node.findPlug(RPRVolumeAttributes::densityMultiplier);

	assert(!plug.isNull());
	if (!plug.isNull())
	{
		return plug.asFloat();
	}

	return 1.0f;
}

void RPRVolumeAttributes::FillVolumeData(VolumeData& data, const MObject& node, FireMaya::Scope* scope)
{
	short3& volumeDims = RPRVolumeAttributes::GetVolumeGridDimentions(node).asShort3();
	data.gridSizeX = volumeDims[0];
	data.gridSizeY = volumeDims[1];
	data.gridSizeZ = volumeDims[2];

	float density_multiplier = 1.0f;
	float emission_intensity = 1.0f;

	data.voxels.clear();
	data.voxels.resize(volumeDims[0] * volumeDims[1] * volumeDims[2]);

	std::vector<RampCtrlPoint<MColor>> albedoCtrlPoints;
	std::vector<RampCtrlPoint<MColor>> emisionCtrlPoints;
	std::vector<RampCtrlPoint<float>> emisionIntensityCtrlPoints;
	std::vector<RampCtrlPoint<float>> denstiyCtrlPoints;
	
	bool isAlbedoEnabled = RPRVolumeAttributes::GetAlbedoEnabled(node);
	if (isAlbedoEnabled)
	{
		MPlug albedoRampPlug = RPRVolumeAttributes::GetAlbedoRamp(node);
		GetValues<MColorArray, MColor>(albedoRampPlug, albedoCtrlPoints);
	}

	bool isEmissionEnabled = RPRVolumeAttributes::GetEmissionEnabled(node);
	if (isEmissionEnabled)
	{
		MPlug emissionRampPlug = RPRVolumeAttributes::GetEmissionValueRamp(node);
		GetValues<MColorArray, MColor>(emissionRampPlug, emisionCtrlPoints);
		emission_intensity = RPRVolumeAttributes::GetEmissionIntensity(node);
		MPlug emissionIntensityRampPlug = RPRVolumeAttributes::GetEmissionIntensityRamp(node);
		GetValues<MFloatArray, float>(emissionIntensityRampPlug, emisionIntensityCtrlPoints);
	}

	bool isDensityEnabled = RPRVolumeAttributes::GetDensityEnabled(node);
	if (isDensityEnabled)
	{
		MPlug densityRampPlug = RPRVolumeAttributes::GetDensityRamp(node);
		GetValues<MFloatArray, float>(densityRampPlug, denstiyCtrlPoints);
		density_multiplier = RPRVolumeAttributes::GetDensityMultiplier(node);
	}

	VolumeGradient albedoGradientType = RPRVolumeAttributes::GetAlbedoGradientType(node);
	VolumeGradient emissionGradientType = RPRVolumeAttributes::GetEmissionGradientType(node);
	VolumeGradient densiyGradientType = RPRVolumeAttributes::GetDensityGradientType(node);

	size_t voxel_idx = 0;
	for (size_t z_idx = 0; z_idx < data.gridSizeZ; ++z_idx)
		for (size_t y_idx = 0; y_idx < data.gridSizeY; ++y_idx)
			for (size_t x_idx = 0; x_idx < data.gridSizeX; ++x_idx)
			{
				VoxelParams voxelParams;
				voxelParams.x = x_idx;
				voxelParams.y = y_idx;
				voxelParams.z = z_idx;
				voxelParams.Xres = data.gridSizeX;
				voxelParams.Yres = data.gridSizeY;
				voxelParams.Zres = data.gridSizeZ;

				// fill voxels with data
				if (isAlbedoEnabled)
				{
					MColor voxelAlbedoColor = GetVoxelValue<MColor>(voxelParams, albedoCtrlPoints, albedoGradientType);

					data.voxels[voxel_idx].aR = voxelAlbedoColor.r;
					data.voxels[voxel_idx].aG = voxelAlbedoColor.g;
					data.voxels[voxel_idx].aB = voxelAlbedoColor.b;
				}

				if (isEmissionEnabled)
				{
					float emission_ramp_intensity = GetVoxelValue<float>(voxelParams, emisionIntensityCtrlPoints, emissionGradientType);
					float emission_multiplier = emission_intensity * emission_ramp_intensity;

					MColor voxelEmissionColor = GetVoxelValue<MColor>(voxelParams, emisionCtrlPoints, emissionGradientType);

					data.voxels[voxel_idx].eR = voxelEmissionColor.r * emission_multiplier;
					data.voxels[voxel_idx].eG = voxelEmissionColor.g * emission_multiplier;
					data.voxels[voxel_idx].eB = voxelEmissionColor.b * emission_multiplier;
				}

				if (isDensityEnabled)
				{
					float voxelDensityValue = GetVoxelValue<float>(voxelParams, denstiyCtrlPoints, densiyGradientType);

					data.voxels[voxel_idx].density = voxelDensityValue * density_multiplier;
				}

				voxel_idx++;
			}


}

float GetDistanceBetweenPoints(
	float x, float y, float z,
	std::array<float, 3> &point)
{
	return sqrt((point[0] - x)*(point[0] - x) + (point[1] - y)*(point[1] - y) + (point[2] - z)*(point[2] - z));
}

float GetDistParamNormalized(
	const VoxelParams& voxelParams,
	VolumeGradient gradientType
)
{
	float dist2vx_normalized; // this is parameter that is used for Ramp input

	float fXres = 1.0f * voxelParams.Xres;
	float fYres = 1.0f * voxelParams.Yres;
	float fZres = 1.0f * voxelParams.Zres;
	float fx = 1.0f * voxelParams.x;
	float fy = 1.0f * voxelParams.y;
	float fz = 1.0f * voxelParams.z;

	switch (gradientType)
	{
	case VolumeGradient::kConstant:
	{
		dist2vx_normalized = 1.0f;
		break;
	}

	case VolumeGradient::kXGradient:
	{
		// get relative distance from current voxel to YZ plane center
		float dist2vx = GetDistanceBetweenPoints(0, fYres / 2, fZres / 2, std::array<float, 3> {fx, fy, fz});
		float distMax = GetDistanceBetweenPoints(0, fYres / 2, fZres / 2, std::array<float, 3> {fXres, 0.0f, 0.0f});
		dist2vx_normalized = 1 - (dist2vx / distMax);
		break;
	}

	case VolumeGradient::kYGradient:
	{
		// get relative distance from current voxel to XZ plane center
		float dist2vx = GetDistanceBetweenPoints(fXres / 2, 0, fZres / 2, std::array<float, 3> {fx, fy, fz});
		float distMax = GetDistanceBetweenPoints(fXres / 2, 0, fZres / 2, std::array<float, 3> {0.0f, fYres, 0.0f});
		dist2vx_normalized = 1 - (dist2vx / distMax);
		break;
	}

	case VolumeGradient::kZGradient:
	{
		// get relative distance from current voxel to XY plane center
		float dist2vx = GetDistanceBetweenPoints(fXres / 2, fYres / 2, 0, std::array<float, 3> {fx, fy, fz});
		float distMax = GetDistanceBetweenPoints(fXres / 2, fYres / 2, 0, std::array<float, 3> {0.0f, 0.0f, fZres});
		dist2vx_normalized = 1 - (dist2vx / distMax);
		break;
	}

	case VolumeGradient::kNegXGradient:
	{
		// get relative distance from current voxel to YZ plane center
		float dist2vx = GetDistanceBetweenPoints(0, fYres / 2, fZres / 2, std::array<float, 3> {fx, fy, fz});
		float distMax = GetDistanceBetweenPoints(0, fYres / 2, fZres / 2, std::array<float, 3> {fXres, 0.0f, 0.0f});
		dist2vx_normalized = dist2vx / distMax;
		break;
	}

	case VolumeGradient::kNegYGradient:
	{
		// get relative distance from current voxel to XZ plane center
		float dist2vx = GetDistanceBetweenPoints(fXres / 2, 0, fZres / 2, std::array<float, 3> {fx, fy, fz});
		float distMax = GetDistanceBetweenPoints(fXres / 2, 0, fZres / 2, std::array<float, 3> {0.0f, fYres, 0.0f});
		dist2vx_normalized = dist2vx / distMax;
		break;
	}

	case VolumeGradient::kNegZGradient:
	{
		// get relative distance from current voxel to XY plane center
		float dist2vx = GetDistanceBetweenPoints(fXres / 2, fYres / 2, 0, std::array<float, 3> {fx, fy, fz});
		float distMax = GetDistanceBetweenPoints(fXres / 2, fYres / 2, 0, std::array<float, 3> {0.0f, 0.0f, fZres});
		dist2vx_normalized = dist2vx / distMax;
		break;
	}

	case VolumeGradient::kCenterGradient: // 0.0 is border, 1.0 is center
	{
		// get relative distance from current voxel to center
		float dist2vx = GetDistanceBetweenPoints(fXres / 2, fYres / 2, fZres / 2, std::array<float, 3> {fx, fy, fz});
		/*std::tie(hasIntersections, dist2center) = GetDistanceToCenter(fx, fy, fz, fXres, fYres, fZres);
		if (!hasIntersections)
			return 100*1.0f; */
		float dist2center = GetDistanceBetweenPoints(fXres / 2, fYres / 2, fZres / 2, std::array<float, 3> {0.0f, 0.0f, 0.0f});
		dist2vx_normalized = 1 - (dist2vx / dist2center);
		break;
	}

	default:
		dist2vx_normalized = 0.0f; // atm only center gradient is supported
	}

	return dist2vx_normalized;
}

