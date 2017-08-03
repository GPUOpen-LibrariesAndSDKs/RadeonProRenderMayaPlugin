#pragma once

#include <ctime>

/**
 * Manage the display of the render progress window
 * and the main progress bar. Handle the user pressing
 * escape to cancel the render.
 */
class RenderProgressBars
{
public:

	// Life Cycle
	// -----------------------------------------------------------------------------

	RenderProgressBars(bool unlimited);

	virtual ~RenderProgressBars();


	// Public Methods
	// -----------------------------------------------------------------------------

	/** Update the progress. */
	void update(int progress);

	/** Close the progress bar window. */
	void close();

	/** Return true if the user has canceled the render. */
	bool isCancelled();


private:

	// Members
	// -----------------------------------------------------------------------------

	/** True if completion criteria is set to unlimited. */
	bool m_unlimited;

	/** Progress bar position */
	int m_progress;

	/** Last check for canceled */
	clock_t m_lastCanceledCheck;

	/** Canceled flag */
	bool m_canceled;

	/** Visible flag */
	bool m_visible;
};
