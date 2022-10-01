/**********************************************************************
Copyright 2020 Advanced Micro Devices, Inc
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
********************************************************************/
#pragma once

#include <ctime>
#include <string>

#include "RenderProgressBars.h"

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
	void update(int progress, bool isSyncing = false);

	/** Close the progress bar window. */
	void close();

	/** Return true if the user has canceled the render. */
	bool isCancelled();

	void ForceUIUpdate();
	void SetWindowsTitleText(const std::string& title, bool forceUpdate = false);
	void SetTextAboveProgress(const std::string& title, bool forceUpdate = false);

	void SetPreparingSceneText(bool forceUpdate = false);
	void SetRenderingText(bool forceUpdate = false);

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
