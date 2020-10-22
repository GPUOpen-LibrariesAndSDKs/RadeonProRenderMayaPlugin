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
#include "FireRenderError.h"
#include "Logger.h"
#include "FireRenderUtils.h"
#include "AutoLock.h"
#include <maya/MGlobal.h>
#include <regex>


// Namespaces
// -----------------------------------------------------------------------------
using namespace std;
using namespace chrono;


// Static Initialization
// -----------------------------------------------------------------------------
time_point<steady_clock> FireRenderError::s_lastDialogTime;
std::mutex FireRenderError::s_lock;
bool FireRenderError::s_lastDialogCritical = false;


// Life Cycle
// -----------------------------------------------------------------------------
FireRenderError::FireRenderError()
{
	m_error = false;
}

// -----------------------------------------------------------------------------
FireRenderError::FireRenderError(rpr_int status, const MString& message, bool showDialog) :
	FireRenderError()
{
	set(status, message, showDialog);
}

// -----------------------------------------------------------------------------
FireRenderError::FireRenderError(std::exception_ptr exception, bool showDialog) :
	FireRenderError()
{
	set(exception, showDialog);
}

// -----------------------------------------------------------------------------
FireRenderError::FireRenderError(const MString& name, const MString& message, bool showDialog, bool isCritical)
{
	set(name, message, showDialog, isCritical);
}

// -----------------------------------------------------------------------------
FireRenderError::~FireRenderError()
{
}


// Public Methods
// -----------------------------------------------------------------------------
void FireRenderError::set(rpr_int status, const MString& message, bool showDialog)
{
	// Generate the error info from
	// the RPR status and set the error.
	ErrorInfo error = getErrorInfo(status, message);
	setError(error, showDialog);
}

// -----------------------------------------------------------------------------
void FireRenderError::set(exception_ptr exception, bool showDialog)
{
	// Generate the error info from
	// the exception and set the error.
	ErrorInfo error = getExceptionInfo(exception);
	setError(error, showDialog);
}

// -----------------------------------------------------------------------------
void FireRenderError::set(const MString& name, const MString& message, bool showDialog, bool isCritical)
{
	// Initialize the error info from parameters.
	ErrorInfo error(name, message, isCritical);
	setError(error, showDialog);
}

// -----------------------------------------------------------------------------
bool FireRenderError::check() const
{
	return m_error;
}


// Private Methods
// -----------------------------------------------------------------------------
void FireRenderError::setError(const ErrorInfo& error, bool showDialog)
{
	// Set to error state.
	m_error = true;

	// Acquire the lock.
	auto lock = RPR::AutoLock<std::mutex>(s_lock);

	// Log the error.
	logError(error);

	// Show the user an error dialog if required.
	// Always show a dialog for critical errors.
	if (showDialog || error.critical)
		showErrorDialog(error);
}

// -----------------------------------------------------------------------------
void FireRenderError::logError(const ErrorInfo& error) const
{
	// Add the error message to the script and debug logs.
	std::string logMessage = getErrorMessage(error).asChar();

	// Sometimes Maya has %20 in textures path instead of spaces
	// We should avoid this because snprintf call will be performed later 
	// and "%" symbol is special symbol for this function.
	logMessage = std::regex_replace(logMessage, std::regex("%20"), " ");

	DebugPrint(logMessage.c_str());
	MGlobal::displayError(MString(logMessage.c_str()));
}

// -----------------------------------------------------------------------------
void FireRenderError::showErrorDialog(const ErrorInfo& error) const
{
	// Check if any error dialog has been
	// shown within the dialog interval.
	time_point<steady_clock> now = steady_clock::now();
	duration<double> delta = now - s_lastDialogTime;
	if (delta.count() < c_dialogInterval)
	{
		// Override the time interval if the new error
		// is critical and the previous error was not.
		if (!(error.critical && !s_lastDialogCritical))
			return;
	}

	// Update the time when the last dialog
	// was shown and the last critical status.
	s_lastDialogTime = now;
	s_lastDialogCritical = error.critical;

	// Create the command to show the error dialog.
	MString command =
		"confirmDialog -title \"Radeon ProRender Error\" -button \"OK\" -message \"" +
		getErrorMessage(error, true) + "\"";

	// Show the error dialog.
	MGlobal::executeCommandOnIdle(command);
}

// -----------------------------------------------------------------------------
MString FireRenderError::getErrorMessage(const ErrorInfo& error, bool dialogFormat) const
{
	return dialogFormat ?
		getDialogMessage(error) :
		getLogMessage(error);
}

// -----------------------------------------------------------------------------
MString FireRenderError::getLogMessage(const ErrorInfo& error) const
{
	// Construct the message.
	MString message = "Radeon ProRender";
	message += error.critical ? " (critical): " : ": ";
	message += error.name;

	// Add the accompanying error message, if any.
	if (error.message.length() > 0)
	{
		message += " - ";
		message += error.message;
	}

	return message;
}

// -----------------------------------------------------------------------------
MString FireRenderError::getDialogMessage(const ErrorInfo& error) const
{
	// Construct the message.
	MString message = "Radeon ProRender has encountered a";
	message += error.critical ? " critical error:" : "n error:";
	message += "\\n\\n";
	message += error.name;

	// Add the accompanying error message, if any.
	if (error.message.length() > 0)
	{
		message += "\\n\\n";
		message += error.message;
	}

	// Add a recommendation to restart for critical messages.
	if (error.critical)
		message += "\\n\\nIt is recommended that you save a copy of your work and restart Maya.";

	return message;
}

// -----------------------------------------------------------------------------
FireRenderError::ErrorInfo FireRenderError::getExceptionInfo(exception_ptr exception) const
{
	// Re-throw the exception.
	try
	{
		std::rethrow_exception(exception);
	}

	// Handle an RPR exception.
	catch (const FireRenderException& ex)
	{
		return getErrorInfo(ex.code, ex.message);
	}

	// Handle all other exceptions.
	catch (...)
	{
		return ErrorInfo("Unknown error", "", true);
	}
}

// -----------------------------------------------------------------------------
FireRenderError::ErrorInfo FireRenderError::getErrorInfo(rpr_int errorCode, const MString& message) const
{
	return ErrorInfo(
		getErrorName(errorCode), message,
		isErrorCodeCritical(errorCode));
}

// -----------------------------------------------------------------------------
MString FireRenderError::getErrorName(rpr_int errorCode) const
{
	switch (errorCode)
	{
		case RPR_ERROR_COMPUTE_API_NOT_SUPPORTED: return "Compute API not supported";
		case RPR_ERROR_OUT_OF_SYSTEM_MEMORY: return "Out of system memory";
		case RPR_ERROR_OUT_OF_VIDEO_MEMORY: return "Out of video memory";
		case RPR_ERROR_INVALID_LIGHTPATH_EXPR: return "Invalid light path expression";
		case RPR_ERROR_INVALID_IMAGE: return "Invalid image";
		case RPR_ERROR_INVALID_AA_METHOD: return "Invalid AA method";
		case RPR_ERROR_UNSUPPORTED_IMAGE_FORMAT: return "Unsupported image format";
		case RPR_ERROR_INVALID_GL_TEXTURE: return "Invalid GL texture";
		case RPR_ERROR_INVALID_CL_IMAGE: return "Invalid CL image";
		case RPR_ERROR_INVALID_OBJECT: return "Invalid object";
		case RPR_ERROR_INVALID_PARAMETER: return "Invalid parameter";
		case RPR_ERROR_INVALID_TAG: return "Invalid tag";
		case RPR_ERROR_INVALID_LIGHT: return "Invalid light";
		case RPR_ERROR_INVALID_CONTEXT: return "Invalid context";
		case RPR_ERROR_UNIMPLEMENTED: return "Unimplemented";
		case RPR_ERROR_INVALID_API_VERSION: return "Invalid API version";
		case RPR_ERROR_INTERNAL_ERROR: return "Internal error";
		case RPR_ERROR_IO_ERROR: return "IO error";
		case RPR_ERROR_UNSUPPORTED_SHADER_PARAMETER_TYPE: return "Unsupported shader parameter type";
		case RPR_ERROR_MATERIAL_STACK_OVERFLOW: return "Material stack overflow";
		default: return "Unknown error";
	}
}

/** True if the error code represents a critical error. */
bool FireRenderError::isErrorCodeCritical(rpr_int errorCode)
{
	switch (errorCode)
	{
		case RPR_ERROR_COMPUTE_API_NOT_SUPPORTED: return false;
		case RPR_ERROR_OUT_OF_SYSTEM_MEMORY: return true;
		case RPR_ERROR_OUT_OF_VIDEO_MEMORY: return true;
		case RPR_ERROR_INVALID_LIGHTPATH_EXPR: return false;
		case RPR_ERROR_INVALID_IMAGE: return false;
		case RPR_ERROR_INVALID_AA_METHOD: return false;
		case RPR_ERROR_UNSUPPORTED_IMAGE_FORMAT: return false;
		case RPR_ERROR_UNSUPPORTED: return false;
		case RPR_ERROR_INVALID_GL_TEXTURE: return false;
		case RPR_ERROR_INVALID_CL_IMAGE: return false;
		case RPR_ERROR_INVALID_OBJECT: return false;
		case RPR_ERROR_INVALID_PARAMETER: return false;
		case RPR_ERROR_INVALID_TAG: return false;
		case RPR_ERROR_INVALID_LIGHT: return false;
		case RPR_ERROR_INVALID_CONTEXT: return true;
		case RPR_ERROR_UNIMPLEMENTED: return false;
		case RPR_ERROR_INVALID_API_VERSION: return true;
		case RPR_ERROR_INTERNAL_ERROR: return true;
		case RPR_ERROR_IO_ERROR: return false;
		case RPR_ERROR_UNSUPPORTED_SHADER_PARAMETER_TYPE: return false;
		case RPR_ERROR_MATERIAL_STACK_OVERFLOW: return false;
		default: return true;
	}
}

// -----------------------------------------------------------------------------
bool checkStatus(rpr_int status, const MString message, bool showDialog)
{
	// Return true if the status is success.
	if (status == RPR_SUCCESS)
		return true;

	// Elevate the error to an exception if it is critical.
	if (FireRenderError::isErrorCodeCritical(status))
		throw FireRenderException(status, message);

	// Display an error.
	FireRenderError(status, message, showDialog);

	return false;
}

// -----------------------------------------------------------------------------
void checkStatusThrow(rpr_int status, const MString message)
{
	// Throw an exception if the status is not success.
	if (status != RPR_SUCCESS)
		throw FireRenderException(status, message);
}

