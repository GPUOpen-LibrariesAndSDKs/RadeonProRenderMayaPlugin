// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"

#include <thread>
#include <sstream>
#include <iomanip>

void LogCallback(const char *sz)
{
	static char buffer[MAX_PATH] = "";
	if (!*buffer)
	{
		if (!GetTempPathA(sizeof(buffer), buffer))
			strcpy_s(buffer, sizeof(buffer), "c://");

		strcat_s(buffer, sizeof(buffer), "istallerDll.log");
	}

	std::stringstream ss;
	ss << std::setbase(16) << std::setw(4) << std::this_thread::get_id() << ": " << sz << std::endl;

	FILE * hFile = nullptr;
	fopen_s(&hFile, buffer, "at");
	if (hFile)
	{
		fprintf(hFile, ss.str().c_str());
		fclose(hFile);
	}
}



BOOL APIENTRY DllMain( HMODULE hModule,
					   DWORD  ul_reason_for_call,
					   LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		Logger::AddCallback(LogCallback, Logger::LevelInfo);
		LogSystem("Start istaller dll...");
		break;

	case DLL_PROCESS_DETACH:
		LogSystem("Stop istaller dll.");
		break;

	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;
	}
	return TRUE;
}

