#include "RadeonProRender.h"

#include <windows.h>
#include <string>
#include <assert.h>
#include "logger.h"

// Note: RadeonProRender64.dll is delay-loaded now, see FireRenderMaya.props. We're calling LoadLibrary before doing first call to any DLL's function.
// RPR_DYNAMIC_LOAD was defined in FireRenderMaya.props. When it was defined, special wrapper was used - .WrappedFunctions*.h. These files had to be
// regenerated every time RPR was updated. If not regenerated, plugin used wrong API version for RPR calls, and these calls were failed - caused
// plugin to crash. Another problem appeared in RPR 1.254: one of functions has default parameter, which is not supported by function typedef, i.e.
// it is not possible to generate correct wrapper. So we're using delay loaded DLL instead.

EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#define HINST_THISCOMPONENT ((HINSTANCE)&__ImageBase)

HMODULE hLoaderModule = NULL;


#ifdef RPR_DYNAMIC_LOAD

#undef RPR_DECL
#define RPR_DECL(x)		P##x x = NULL;
#include ".WrappedFunctions.h"
#include ".WrappedFunctions_GL.h"


bool BindFunctions()
{
	#undef RPR_DECL

	#define RPR_DECL(x)	\
		x = (P##x)GetProcAddress(hLoaderModule, #x); \
		assert(x);

	#include ".WrappedFunctions.h"
	#include ".WrappedFunctions_GL.h"
	return true;
}

#endif // RPR_DYNAMIC_LOAD

bool RPRInit()
{
	std::string dllFolder;

	const HINSTANCE hCrurrentModule = (HINSTANCE)&__ImageBase;

	char buffer[MAX_PATH];
	if (GetModuleFileName(hCrurrentModule, buffer, MAX_PATH))
	{
		std::string path = buffer;
		size_t pos = path.rfind("plug-ins");
		if (pos != std::string::npos)
		{
			dllFolder.assign(path, 0, pos);
			dllFolder.append("bin\\");
		}
	}

	std::string sAddPath = "PATH=" + dllFolder + ";";
	char * oldPath = getenv("PATH");
	if (oldPath)
	{
		DebugPrint("PATH = %s", oldPath);
		std::string sFullEnvPath = sAddPath + oldPath;
		if (_putenv(sFullEnvPath.c_str()))
			DebugPrint("Can't set PATH");
	}

	hLoaderModule = LoadLibrary("RadeonProRender64.dll");
	if (!hLoaderModule)
	{
		DebugPrint("Can't load loader dll");
		assert(hLoaderModule);
	}

	if (GetModuleFileName(hLoaderModule, buffer, MAX_PATH))
	{
		DebugPrint("Loader dll path - %s", buffer);
	}
	else
	{
		DebugPrint("Can't get loader dll path");
	}

#ifdef RPR_DYNAMIC_LOAD
	return BindFunctions();
#else
	return (hLoaderModule != NULL);
#endif
}


void RPRRelease()
{
	assert(hLoaderModule);
	FreeLibrary(hLoaderModule);
	hLoaderModule = NULL;
}
