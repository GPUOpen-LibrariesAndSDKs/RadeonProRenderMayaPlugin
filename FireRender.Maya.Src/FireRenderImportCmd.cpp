#include "FireRenderImportCmd.h"
#include "FireRenderContext.h"
#include "FireRenderUtils.h"
#include <maya/MArgDatabase.h>
#include <maya/MItDag.h>
#include <maya/MDagPath.h>
#include <maya/MGlobal.h>
#include <maya/MSelectionList.h>
#include <maya/MObjectArray.h>
#include <maya/MArgList.h>
#include <maya/MStatus.h>
#include <maya/MDagPathArray.h>
#include <maya/MPlug.h>
#include <fstream>

#ifdef MAYA2015
#undef min
#undef max
#endif

#ifdef OSMac_
	#include <OpenImageIO/imageio.h>
#else
	#include <imageio.h>
#endif

class FireRenderShaderLoader
{
public:
	FireRenderShaderLoader();

	~FireRenderShaderLoader();

	MObject load(std::ifstream *myfile, const std::string& name);

	MObject loadFireRenderMaterialShader(std::ifstream *myfile, int shaderType);

	MObject loadFireRenderStandardMaterial(std::ifstream *myfile);

	MObject loadShader(std::ifstream *myfile);

private:

	std::string m_nodeName;

	std::vector<std::string> m_images;
};

FireRenderShaderLoader::FireRenderShaderLoader()
{

}

FireRenderShaderLoader::~FireRenderShaderLoader()
{

}

MObject FireRenderShaderLoader::load(std::ifstream *myfile, const std::string& name)
{
	m_nodeName = name;
	MObject shader = loadShader(myfile);
	if (!shader.isNull())
	{
		MFnDependencyNode nodeFn(shader);
		nodeFn.setName(name.c_str());

		MString sgNode = MGlobal::executeCommandStringResult("sets -renderable true -noSurfaceShader true -empty -name "+ nodeFn.name() + "SG");
		if (sgNode != "")
		{
			MGlobal::executeCommand("connectAttr -f " + nodeFn.name() + ".outColor " + sgNode + ".surfaceShader");
		}
	}
	return shader;
}

MObject FireRenderShaderLoader::loadFireRenderMaterialShader(std::ifstream *myfile, int shaderType)
{
	MGlobal::displayError("Feature not Supported");
	return MObject();
}


MObject FireRenderShaderLoader::loadFireRenderStandardMaterial(std::ifstream *myfile)
{

	MGlobal::displayError("Feature Not Supported");
	return MObject();
}

MObject FireRenderShaderLoader::loadShader(std::ifstream *myfile)
{
	MGlobal::displayError("Feature not supported");
	return MObject();
}


FireRenderImportCmd::FireRenderImportCmd()
{
}

FireRenderImportCmd::~FireRenderImportCmd()
{
}

void * FireRenderImportCmd::creator()
{
	return new FireRenderImportCmd;
}

MSyntax FireRenderImportCmd::newSyntax()
{
	MStatus status;
	MSyntax syntax;

	CHECK_MSTATUS(syntax.addFlag(kMaterialFlag, kMaterialFlagLong, MSyntax::kString));
	CHECK_MSTATUS(syntax.addFlag(kFilePathFlag, kFilePathFlagLong, MSyntax::kString));

	return syntax;
}

MStatus FireRenderImportCmd::doIt(const MArgList & args)
{
	MStatus status;

	MArgDatabase argData(syntax(), args);

	MString filePath;
	if (argData.isFlagSet(kFilePathFlag))
	{
		argData.getFlagArgument(kFilePathFlag, 0, filePath);
	}
	else
	{
		MGlobal::displayError("File path is missing, use -file flag");
		return MS::kFailure;
	}

	MString materialName;
	if (argData.isFlagSet(kMaterialFlag))
	{
		argData.getFlagArgument(kMaterialFlag, 0, materialName);

		std::ifstream fileStore(filePath.asChar(), std::ifstream::binary | std::ofstream::in);
		if (fileStore.is_open())
		{
			FireRenderShaderLoader loader;
			MObject shader = loader.load(&fileStore, materialName.asChar());
			fileStore.close();
			if (shader.isNull())
			{
				MGlobal::displayError("Unable to import fire render material\n");
				return MS::kFailure;
			}

			MFnDependencyNode nodeFn(shader);
			setResult(nodeFn.name());
		}

		return MS::kSuccess;
	}

	return MS::kFailure;
}
