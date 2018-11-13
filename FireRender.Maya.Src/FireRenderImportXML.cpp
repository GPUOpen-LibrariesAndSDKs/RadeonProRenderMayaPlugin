#include "FireRenderImportExportXML.h"
#include "FireRenderContext.h"
#include "FireRenderUtils.h"
#include "common.h"
#include "FireRenderMaterial.h"
#include <iterator>

#include <maya/MItDependencyNodes.h>
#include <maya/MArgList.h>

#include "FileSystemUtils.h"
#include "FireRenderError.h"

#ifdef _WIN32
#include <tchar.h>
#else
#include <wchar.h>
#endif

#if defined(LINUX) || defined(OSMac_)
#else
extern "C" {
#include "AxfConverterDll.h"
}
#pragma comment (lib, "../ThirdParty/AxfPackage/ReleaseDll/AxfLib/AxFDecoding_r.lib")
#pragma comment (lib, "../ThirdParty/AxfPackage/ReleaseDll/AxfLib/FreeImage.lib")
#pragma comment (lib, "../ThirdParty/AxfPackage/ReleaseDll/AxfLib/libboost_filesystem-vc140-mt-1_62.lib")
#pragma comment (lib, "../ThirdParty/AxfPackage/ReleaseDll/AxfLib/libboost_program_options-vc140-mt-1_62.lib")
#pragma comment (lib, "../ThirdParty/AxfPackage/ReleaseDll/AxfLib/libboost_regex-vc140-mt-1_62.lib")
#pragma comment (lib, "../ThirdParty/AxfPackage/ReleaseDll/AxfLib/libboost_system-vc140-mt-1_62.lib")

#endif

// convert axf to rpr xml using conversion dll
std::tuple<MStatus, MString> ProcessAXF(std::string& filePathStr)
{
#if defined(LINUX) || defined(OSMac_)
    return std::make_tuple(MS::kFailure, MString());
#else
	HINSTANCE hGetProcIDDLL = LoadLibrary(_T("AxfConverter.dll"));
	if (!hGetProcIDDLL)
	{
		return std::make_tuple(MS::kFailure, MString());
	}

	AxfConvertFile convertFile = (AxfConvertFile)GetProcAddress(hGetProcIDDLL, "convertAxFFile");
	char xmlFilePath[255];
	convertFile(filePathStr.c_str(), xmlFilePath);
	FreeLibrary(hGetProcIDDLL);

	bool xmlCreated = strlen(xmlFilePath) != 0;
	if (!xmlCreated)
		return std::make_tuple(MS::kFailure, MString());
    
	return std::make_tuple(MS::kSuccess, MString(xmlFilePath));
#endif
}

std::tuple<MStatus, MString> FireRenderXmlImportCmd::GetFilePath(const MArgDatabase& argData)
{
	// back-off
	if (!argData.isFlagSet(kFilePathFlag))
	{
		MGlobal::displayError("File path is missing, use -file flag");
		return std::make_tuple(MS::kFailure, MString());
	}

	MString filePath;
	argData.getFlagArgument(kFilePathFlag, 0, filePath);

#if defined(LINUX) || defined(OSMac_)
#else
	std::string filePathStr(filePath.asChar());
	std::transform(filePathStr.begin(), filePathStr.end(), filePathStr.begin(), ::tolower);

	bool isAxf = filePathStr.find(".axf") != -1;

	if (isAxf) 
	{
		MStatus result;
		std::tie(result, filePath) = ProcessAXF(filePathStr);
		if (MS::kSuccess != result)
			return std::make_tuple(MS::kFailure, MString());
	}
#endif

	return std::make_tuple(MS::kSuccess, filePath);
}


