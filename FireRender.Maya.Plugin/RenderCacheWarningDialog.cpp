#include "RenderCacheWarningDialog.h"
#include <maya/MString.h>
#include <maya/MGlobal.h>


RenderCacheWarningDialog::RenderCacheWarningDialog()
{
	shown = false;
}


RenderCacheWarningDialog::~RenderCacheWarningDialog()
{
}

void RenderCacheWarningDialog::show()
{
	// Create the progress bar window.
	MGlobal::executeCommand("window -title \"Warning!\" -widthHeight 250 35 -tlc 0 0 rprCachingShadersWarningWindow;",true);
	MGlobal::executeCommand("window -edit -height 35 rprCachingShadersWarningWindow;", true);
	MGlobal::executeCommand("columnLayout - adjustableColumn true rprCachingShadersWarningWindow;", true);
	MGlobal::executeCommand("text -label \"Rebuilding Radeon ProRender shader cache.\" -fn \"boldLabelFont\" -bgc 1 1 1 -h 20", true);
	MGlobal::executeCommand("text -label \"Please wait, this may take up to one minute...\" -fn \"boldLabelFont\" -bgc 1 1 1 -h 20", true);
	MGlobal::executeCommand("showWindow rprCachingShadersWarningWindow;", true);

	MGlobal::executeCommand("refresh -f;", true);

	shown = true;
}

// -----------------------------------------------------------------------------
void RenderCacheWarningDialog::close()
{
	MGlobal::executeCommand("deleteUI rprCachingShadersWarningWindow;");

	MGlobal::executeCommand("refresh -f;");

	shown = false;
}