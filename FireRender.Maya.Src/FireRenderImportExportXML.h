#pragma once

#include <maya/MPxCommand.h>
#include <maya/MSyntax.h>
#include <maya/MArgDatabase.h>
#include <maya/MObject.h>
#include <tuple>
#include "MaterialLoader.h"
#include <maya/MStatus.h>
#include <maya/MString.h>

#define kMaterialNameFlag "-mn"
#define kMaterialNameFlagLong "-materialName"
#define kMaterialFlag "-m"
#define kMaterialFlagLong "-material"
#define kSelectionFlag "-s"
#define kSelectionFlagLong "-selection"
#define kAllFlag "-sc"
#define kAllFlagLong "-scene"
#define kFilePathFlag "-f"
#define kFilePathFlagLong "-file"
#define kImportImages "-ii"
#define kImportImagesLong "-importImages"

class FireRenderXmlExportCmd : public MPxCommand
{
public:

	FireRenderXmlExportCmd();

	virtual ~FireRenderXmlExportCmd();

	static void* creator();

	static MSyntax  newSyntax();

	MStatus doIt(const MArgList& args);
};

////////////////////////////////////////////////////////
////////////////////////////////////////////////////////

const bool doesAxfConverterDllExists();

class FireRenderAxfDLLExists : public MPxCommand
{
public:
	FireRenderAxfDLLExists();

	virtual ~FireRenderAxfDLLExists();

	static void* creator();

	static MSyntax  newSyntax();

	MStatus doIt(const MArgList& args);
};

#define useNewImport

class FireRenderXmlImportCmd : public MPxCommand
{
	enum ShadingNodeType
	{
		asLight,
		asPostProcess,
		asRendering,
		asShader,
		asTexture,
		asUtility,
	};

public:

	FireRenderXmlImportCmd();

	virtual ~FireRenderXmlImportCmd();

	static void* creator();

	static MSyntax  newSyntax();

	MStatus doIt(const MArgList& args);
	typedef std::map<const std::string, std::string> AttributeMapper;
	std::tuple<const AttributeMapper*, ShadingNodeType, MString> GetAttributeMapper(int matType) const;

	std::tuple<MStatus, MString> GetFilePath(const MArgDatabase& argData);

	int getAttrType(std::string attrType);
	MObject createShadingNode(MString materialName, std::map<const std::string, std::string> &attributeMapper, std::map<std::string, Param> &params, ShadingNodeType shadingNodeType, frw::ShaderType shaderType = frw::ShaderTypeInvalid);
	void parseAttributeParam(MObject shaderNode, std::map<const std::string, std::string> &attributeMapper, const std::string attrName, const Param &attrParam);

	void attachToPlace2dTextureNode(MObject objectToConnectTo, const std::map<std::string, Param>& paramsToParse);
	MObject createPlace2dTextureNode(std::string name);
	void connectPlace2dTextureNodeTo(MObject place2dTextureNode, MObject objectToConnectTo);

	MObject loadFireRenderMaterialShader(frw::ShaderType shaderType, std::map<std::string, Param> &params);
	MObject loadFireRenderAddMaterial(std::map<std::string, Param> &params);
	MObject loadFireRenderBlendMaterial(std::map<std::string, Param> &params);
	MObject loadFireRenderArithmeticMaterial(std::map<std::string, Param> &params);
	MObject loadFireRenderFresnel(std::map<std::string, Param> &params);
	MObject loadFireRenderFresnelSchlick(std::map<std::string, Param> &params);
	MObject loadFireRenderNormal(std::map<std::string, Param> &params);
	MObject loadFireRenderImageTextureNode(std::map<std::string, Param> &params);
	MObject loadFireRenderNoiseNode(std::map<std::string, Param> &params);
	MObject loadFireRenderDotNode(std::map<std::string, Param> &params);
	MObject loadFireRenderGradientNode(std::map<std::string, Param> &params);
	MObject loadFireRenderCheckerNode(std::map<std::string, Param> &params);
	//TODO: FR_MATERIAL_NODE_CONSTANT_TEXTURE
	MObject loadFireRenderLookupNode(std::map<std::string, Param> &params);
	MObject loadFireRenderBlendValueNode(std::map<std::string, Param> &params);
	MObject loadFireRenderStandardMaterial( std::map<std::string, Param> &params );
	MObject loadFireRenderPassthroughNode(std::map<std::string, Param> &params);
	MObject loadFireRenderBumpNode(std::map<std::string, Param> &params);

	int getMatType(std::string attrType);
	MObject parseMaterialNode(MaterialNode &matNode);
	MObject parseShader(int &matType, std::map<std::string, Param> &params);

	/** Import an image file to the project source images directory. */
	MString importImageFile(const MFileObject& file) const;

	/** Called if there was an error importing an image file. */
	MString importImageFileError(const MString& sourcePath, const MString& destDirectory) const;

	/** Get the project source images directory for a material library image file. */
	MString getSourceImagesDirectory(const MString& filePath) const;

private:
	std::map<std::string, MaterialNode> nodeGroup;
	MString m_directoryPath;
	bool m_importImages;
};
