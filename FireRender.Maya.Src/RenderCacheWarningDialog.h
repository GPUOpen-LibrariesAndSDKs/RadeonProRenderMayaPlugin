#pragma once

class RenderCacheWarningDialog
{
public:
	RenderCacheWarningDialog();
	~RenderCacheWarningDialog();

	/** Show the Warning dialog. */
	void show();

	/** Close the Warning dialog. */
	void close();

	bool shown;
};

