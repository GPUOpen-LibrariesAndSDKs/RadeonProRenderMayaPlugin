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

		strcat_s(buffer, sizeof(buffer), "installerDll.log");
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
		LogSystem("Start installer dll...");
		break;

	case DLL_PROCESS_DETACH:
		LogSystem("Stop installer dll.");
		break;

	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;
	}
	return TRUE;
}

