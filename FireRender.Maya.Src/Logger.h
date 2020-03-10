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
#include <cstdio>
#include <map>
#include <vector>
#include <iostream>
#include <assert.h>

class Logger
{
public:
	enum LevelEnum
	{
		LevelDebug = 0,
		LevelInfo,
		LevelWarn,
		LevelError,
	};

private:
	typedef void(*Callback)(const char * sz);

	std::map<Callback, LevelEnum> callbacks;

	// single instance
	static Logger& Instance()
	{
		static Logger instance;
		return instance;
	}


public:

	static void AddCallback(Callback cb, LevelEnum level)
	{
		Instance().callbacks[cb] = level;
	}

	template <typename... Args>
	static void Printf(LevelEnum level, const char *format, const Args&... args)
	{
		if (!Instance().callbacks.empty())
		{
			std::vector<char> buf;
			buf.resize(0x10000);
			int written = snprintf(buf.data(), buf.size(), format, args...);
			assert(written >= 0 && written < buf.size());
#ifdef LINUX
			// Added for Linux debugging:
			std::clog << buf.data();
#endif
			for (auto cb : Instance().callbacks)
			{
				if (cb.second <= level)
					cb.first(buf.data());
			}
		}
	}
};

template <typename... Args>
inline void DebugPrint(const char *format, const Args&... args)
{
#ifdef _DEBUG
	Logger::Printf(Logger::LevelDebug, format, args...);
#endif
}

template <typename... Args>
inline void LogPrint(const char *format, const Args&... args)
{
	Logger::Printf(Logger::LevelInfo, format, args...);
}

template <typename... Args>
inline void ErrorPrint(const char *format, const Args&... args)
{
	Logger::Printf(Logger::LevelError, format, args...);
}

