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

#include <RadeonProRender.h>
#include <exception>
#include <chrono>
#include <maya/MString.h>

#include <atomic>
#include <mutex>


class FireRenderException : public std::exception
{
public:
	rpr_int code;
	MString message;
	explicit FireRenderException(int code, const MString& message = "") :
		code(code),
		message(message)
	{}
};

class FireRenderError
{

public:

	// Life Cycle
	// -----------------------------------------------------------------------------

	/** Default constructor - no error set. */
	FireRenderError();

	/** Create an error with an RPR status code, message, and optional dialog. */
	FireRenderError(rpr_int status, const MString& message, bool showDialog = false);

	/** Create an error with an exception and optional dialog. */
	FireRenderError(std::exception_ptr exception, bool showDialog = true);

	/** Create an error directly with a name and critical status. */
	FireRenderError(const MString& name, const MString& message, bool showDialog = false, bool isCritical = false);

	/** Destructor. */
	virtual ~FireRenderError();


	// Public Methods
	// -----------------------------------------------------------------------------

	/** Set from an error code. */
	void set(rpr_int status, const MString& message, bool showDialog = true);

	/** Set from an exception. */
	void set(std::exception_ptr exception, bool showDialog = true);

	/** Set directly with a name and critical status. */
	void set(const MString& name, const MString& message, bool showDialog = true, bool isCritical = false);

	/** Return true if an error has been set. */
	bool check() const;

	/** True if the error code represents a critical error. */
	static bool isErrorCodeCritical(rpr_int errorCode);


private:

	// Structs
	// -----------------------------------------------------------------------------

	/** Information about the error. */
	struct ErrorInfo
	{
		MString name = "";
		MString message = "";
		bool critical = false;

		ErrorInfo(const MString& name, const MString& message, bool critical) :
			name(name),
			message(message),
			critical(critical)
		{}
	};


	// Constants
	// -----------------------------------------------------------------------------

	/**
	 * The duration in seconds between showing error dialogs.
	 * This prevents multiple dialogs being shown to the user
	 * if they were generated around the same time.
	 */
	const double c_dialogInterval = 1.0;


	// Members
	// -----------------------------------------------------------------------------

	/** True if currently in an error state. */
	std::atomic<bool> m_error;


	// Static Members
	// -----------------------------------------------------------------------------

	/**
	 * Sharing the lock between all instances of the class
	 * ensures that multiple dialogs won't be shown to the user.
	 */
	static std::mutex s_lock;

	/**
	 * The time of the most recent error dialog across all error
	 *  handlers, used to determine whether to display a dialog.
	 */
	static std::chrono::time_point<std::chrono::steady_clock> s_lastDialogTime;

	/** True if the last dialog was for a critical error. */
	static bool s_lastDialogCritical;


	// Private Methods
	// -----------------------------------------------------------------------------

	/** Set the error state. */
	void setError(const ErrorInfo& error, bool showDialog);

	/** Log the error. */
	void logError(const ErrorInfo& error) const;

	/** Show an error dialog containing the error. */
	void showErrorDialog(const ErrorInfo& error) const;

	/** Get an error message, optionally formatted for dialog display. */
	MString getErrorMessage(const ErrorInfo& error, bool dialogFormat = false) const;

	/** Get an error message formatted for logging. */
	MString getLogMessage(const ErrorInfo& error) const;

	/** Get an error message formatted for dialog. */
	MString getDialogMessage(const ErrorInfo& error) const;

	/** Get the error info for an exception. */
	ErrorInfo getExceptionInfo(std::exception_ptr exception) const;

	/** Get the error info for the code exception. */
	ErrorInfo getErrorInfo(rpr_int errorCode, const MString& message) const;

	/** Get a string representation of an error code. */
	MString getErrorName(rpr_int errorCode) const;
};

/** Check an RPR return status and display an error if it's not a success. */
bool checkStatus(rpr_int status, const MString message = "", bool showDialog = false);

/** Check an RPR return status and throw an error if it's not a success. */
void checkStatusThrow(rpr_int status, const MString message = "");
