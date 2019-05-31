#include "RenderProgressBars.h"
#include <maya/MString.h>
#include <maya/MGlobal.h>
#include "FireRenderThread.h"
#include <assert.h>

// Life Cycle
// -----------------------------------------------------------------------------
RenderProgressBars::RenderProgressBars(bool unlimited) :
	m_unlimited(unlimited),
	m_progress(-1),
	m_lastCanceledCheck(-1),
	m_canceled(false),
	m_visible(false)
{
	MAIN_THREAD_ONLY;

	// Create the progress bar window.
	MGlobal::executeCommand("window -title \"Rendering...\" -widthHeight 250 250 -tlc 0 0 rprProductionRenderingInfoWindow;");
	MGlobal::executeCommand("window -edit -height 35 rprProductionRenderingInfoWindow;");
	MGlobal::executeCommand("columnLayout - adjustableColumn true rprProductionRenderingInfoWindow;");
	MGlobal::executeCommand("text -label \"Rendering...[Press ESC to Cancel]\" rprProductionRenderingInfoWindow;");

	// Show an 'Unlimited render time' label
	// instead of a progress bar if completion
	// criteria is set to unlimited.
	if (unlimited)
		MGlobal::executeCommand("text -label \"Unlimited render time\" -align \"center\";");

	// Otherwise, create the progress bar.
	else
	{
		int phpbExists = 0;
		MGlobal::executeCommand("progressBar -q -exists rprPlaceHolderProgressBar;", phpbExists);
		if (!phpbExists)
			MGlobal::executeCommand("progressBar rprPlaceHolderProgressBar;");
		else
			MGlobal::executeCommand("progressBar -edit -vis true rprPlaceHolderProgressBar;");

		MGlobal::executeCommand("progressBar -edit -pr 1 -ann \"Rendering...\" -maxValue 100 rprPlaceHolderProgressBar;");
	}

	// Show the progress bar window.
	MGlobal::executeCommand("showWindow rprProductionRenderingInfoWindow;");

	m_visible = true;

	// This needs to be done due to a bug (if esc hit
	// multiple times before cancel is registered in plugin).
	MGlobal::executeCommand("progressBar -edit -beginProgress -isInterruptable true -status \"Rendering...\" -maxValue 100 $gMainProgressBar;");
	int i = 0;
	while (i < 100)
	{
		if (isCancelled())
		{
			MGlobal::executeCommand("progressBar -edit -endProgress $gMainProgressBar;");
			MGlobal::executeCommand("progressBar -edit -beginProgress -isInterruptable true -status \"Rendering...\" -maxValue 100 $gMainProgressBar;");
			MGlobal::executeCommand("refresh -f;");
			i = 0;
		}
		else
		{
			i++;
			update(i);
		}
	}

	// Initialize the main progress bar.
	MGlobal::executeCommand("progressBar -edit -endProgress $gMainProgressBar;");
	MGlobal::executeCommand("progressBar -edit -beginProgress -isInterruptable true -status \"Rendering...\" -maxValue 100 $gMainProgressBar;");
	MGlobal::executeCommand("progressBar -edit -pr 1 $gMainProgressBar;");

	// Perform an initial update.
	update(0);

	// Force a Maya UI refresh.
	MGlobal::executeCommand("refresh -f;");

	m_lastCanceledCheck = clock();
}

// -----------------------------------------------------------------------------
RenderProgressBars::~RenderProgressBars()
{
	MAIN_THREAD_ONLY;

	close();
}


// Public Methods
// -----------------------------------------------------------------------------
void RenderProgressBars::update(int progress)
{
	MAIN_THREAD_ONLY;

	if (m_progress != progress)
	{
		if (m_visible)
		{
			MString commandExists = "progressBar -ex rprPlaceHolderProgressBar;";
			int exists;
			MGlobal::executeCommand(commandExists, exists);

			if (exists == 0)
			{
				m_visible = false;
				m_canceled = true;	// Cancel when closed
			}
		}

		// Update progress on the window and main progress bars.
		MString command = "progressBar -edit -pr ";
		command += progress;

		MString commandMain = command + " $gMainProgressBar;";
		MGlobal::executeCommand(commandMain);

		if (!m_unlimited && m_visible)
		{
			MString commandPlaceHolder = command + " rprPlaceHolderProgressBar;";
			MGlobal::executeCommand(commandPlaceHolder);
		}

		m_progress = progress;
	}
}

// -----------------------------------------------------------------------------
void RenderProgressBars::close()
{
	MAIN_THREAD_ONLY;

	MGlobal::executeCommand("progressBar -edit -endProgress $gMainProgressBar;");

	if (!m_unlimited && m_visible)
		MGlobal::executeCommand("progressBar -edit -vis false rprPlaceHolderProgressBar;");

	if(m_visible)
		MGlobal::executeCommand("deleteUI rprProductionRenderingInfoWindow;");

	m_visible = false;

	MGlobal::executeCommand("refresh -f;");

	m_progress = -1;
}

// -----------------------------------------------------------------------------
bool RenderProgressBars::isCancelled()
{
	MAIN_THREAD_ONLY;

	// For MacOS one second interval is too big for proper checking of cancellation status. Let's do it 1/4 of second
#if defined(OSMac_)
	clock_t interval = CLOCKS_PER_SEC / 4;
#else
	clock_t interval = CLOCKS_PER_SEC;
#endif

	clock_t t = clock();
	if (m_lastCanceledCheck + interval <= t)
	{
		int isCancelled = 0;

		MGlobal::executeCommand("progressBar -q -ic $gMainProgressBar;", isCancelled);

		m_lastCanceledCheck = t;

		m_canceled = isCancelled != 0;
	}

	return m_canceled;
}
