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

