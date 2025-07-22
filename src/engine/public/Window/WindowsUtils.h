#pragma once

#include <string>

class WindowsUtils {
public:
	static std::string GetWindowsUserName();
	static std::string GetWindowsComputerName();
	static std::string GetWindowsVersion();
	static std::string GetCPUName();
	static std::string GetGPUName();
	static std::string GetRamMax();
	static std::string GetRamUsage();
	static std::string GetHresultMessage(long hr);
	static bool RegistryGetDWord(void* hKeyParent, const char* key, const char* name, unsigned long* pData);
	static bool IsAppDarkTheme();
	static bool IsSystemDarkTheme();
};
