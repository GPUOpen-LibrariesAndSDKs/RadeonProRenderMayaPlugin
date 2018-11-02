#include "FireRenderExportCmd.h"
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
#include <maya/MTime.h>
#include <maya/MAnimControl.h>
#include <fstream>

#ifdef __linux__
	#include <../RprLoadStore.h>
#else
	#include <../inc/RprLoadStore.h>
#endif

FireRenderExportCmd::FireRenderExportCmd()
{
}

FireRenderExportCmd::~FireRenderExportCmd()
{
}

void * FireRenderExportCmd::creator()
{
	return new FireRenderExportCmd;
}

MSyntax FireRenderExportCmd::newSyntax()
{
	MStatus status;
	MSyntax syntax;

	CHECK_MSTATUS(syntax.addFlag(kMaterialFlag, kMaterialFlagLong, MSyntax::kString));
	CHECK_MSTATUS(syntax.addFlag(kFilePathFlag, kFilePathFlagLong, MSyntax::kString));
	CHECK_MSTATUS(syntax.addFlag(kSelectionFlag, kSelectionFlagLong, MSyntax::kNoArg));
	CHECK_MSTATUS(syntax.addFlag(kAllFlag, kAllFlagLong, MSyntax::kNoArg));
	CHECK_MSTATUS(syntax.addFlag(kFramesFlag, kFramesFlagLong, MSyntax::kBoolean, MSyntax::kLong, MSyntax::kLong));

	return syntax;
}

MStatus FireRenderExportCmd::doIt(const MArgList & args)
{
	MStatus status;

	MArgDatabase argData(syntax(), args);

	// for the moment, the LoadStore library of RPR only supports the export/import of all scene
	if (  !argData.isFlagSet(kAllFlag)  )
	{
		MGlobal::displayError("This feature is not supported.");
		return MS::kFailure;
	}

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

		MSelectionList sList;
		sList.add(materialName);

		MObject node;
		sList.getDependNode(0, node);
		if (node.isNull())
		{
			MGlobal::displayError("Invalid shader");
			return MS::kFailure;
		}

		FireRenderContext context;
		context.setCallbackCreationDisabled(true);

		rpr_int res = context.initializeContext();
		if (res != RPR_SUCCESS)
		{
			MString msg;
			FireRenderError(res, msg, true);
			return MS::kFailure;
		}

		context.setCallbackCreationDisabled(true);

		auto shader = context.GetShader(node);
		if (!shader)
		{
			MGlobal::displayError("Invalid shader");
			return MS::kFailure;
		}

		MGlobal::displayError("This feature is not supported.\n");
		return MS::kFailure;
	}

	bool isSequenceExportEnabled = false;
	int firstFrame = 1;
	int lastFrame = 1;
	if (argData.isFlagSet(kFramesFlag))
	{
		argData.getFlagArgument(kFramesFlag, 0, isSequenceExportEnabled);
		argData.getFlagArgument(kFramesFlag, 1, firstFrame);
		argData.getFlagArgument(kFramesFlag, 2, lastFrame);
	}

	if (argData.isFlagSet(kAllFlag))
	{
		// initialize
		FireRenderContext context;
		context.buildScene();
		context.setResolution(36, 24, true);

		MDagPathArray cameras = getRenderableCameras();
		if ( cameras.length() >= 1 )
		{
			context.setCamera(cameras[0]);
		}

		// setup frame ranges
		if (!isSequenceExportEnabled)
		{
			firstFrame = lastFrame = 1;
		}

		// process file path
		MString name;
		MString ext;

		int pos = filePath.rindexW(L".");
		if (pos == -1)
		{
			name = filePath;
			ext = ".rpr";
		}
		else
		{
			name = filePath.substringW(0, pos - 1);
			ext = filePath.substringW(pos, filePath.length() - 1);

			if (ext != ".rpr")
			{
				name += ext;
				ext = ".rpr";
			}
		}

		// process each frame
		for (int frame = firstFrame; frame <= lastFrame; ++frame)
		{
			// Move the animation to the next frame.
			MTime time;
			time.setValue(static_cast<double>(frame));
			MStatus isTimeSet = MAnimControl::setCurrentTime(time);
			CHECK_MSTATUS(isTimeSet);

			// Refresh the context so it matches the
			// current animation state and start the render.
			context.Freshen();

			// update file path
			MString newFilePath;
			if (isSequenceExportEnabled)
			{
				newFilePath = name;
				newFilePath += "_";
				newFilePath += frame;
				newFilePath += ext;
			}
			else
			{
				newFilePath = filePath;
			}

			// launch export
			rpr_int statuExport = rprsExport(
				newFilePath.asChar(),
				context.context(),
				context.scene(),
				0, 0,
				0, 0,
				0, 0);

			if (statuExport != RPR_SUCCESS)
			{
				MGlobal::displayError("Unable to export fire render scene\n");
				return MS::kFailure;
			}
		}

		return MS::kSuccess;
	}

	if (argData.isFlagSet(kSelectionFlag))
	{
		//initialize
		FireRenderContext context;
		context.setCallbackCreationDisabled(true);
		context.buildScene();


		MDagPathArray cameras = getRenderableCameras();
		if ( cameras.length() >= 1 )
		{
			context.setCamera(cameras[0]);
		}

		MSelectionList sList;
		std::vector<rpr_shape> shapes;
		std::vector<rpr_light> lights;
		rpr_light envLight = nullptr;

		MGlobal::getActiveSelectionList(sList);
		for (unsigned int i = 0; i < sList.length(); i++)
		{
			MDagPath path;

			sList.getDagPath(i, path);
			path.extendToShape();

			MObject node = path.node();
			if (node.isNull())
				continue;

			MFnDependencyNode nodeFn(node);

			FireRenderObject* frObject = context.getRenderObject(getNodeUUid(node));
			if (!frObject)
				continue;

			if (auto light = dynamic_cast<FireRenderLight*>(frObject))
			{
				if (light->data().isAreaLight && light->data().areaLight)
				{
					shapes.push_back(light->data().areaLight.Handle());
				}
				else
				{
					if (light->data().light)
					{
						lights.push_back(light->data().light.Handle());
					}
				}
			}

			else if (auto mesh = dynamic_cast<FireRenderMesh*>(frObject))
			{
				for (auto element : mesh->Elements())
				{
					shapes.push_back(element.shape.Handle());
				}
			}
			else if (auto frEnvLight = static_cast<FireRenderEnvLight*>(frObject))
			{
				envLight = frEnvLight->data().Handle();
			}

		}

		MGlobal::displayError("This feature is not supported.\n");
		return MS::kFailure;
	}

	return MS::kFailure;
}
