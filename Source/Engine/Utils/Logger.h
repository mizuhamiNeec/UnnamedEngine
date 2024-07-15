#pragma once
#include <string>
#include <Windows.h>

inline void Log(const std::string& message) { OutputDebugStringA(message.c_str()); }

class Logger {
	static void Log(const std::string& message);
};
