#pragma once

#include <Windows.h>
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
	static std::string GetHresultMessage(HRESULT hr);
	static bool RegistryGetDWord(HKEY hKeyParent, const char* key, const char* name, DWORD* pData);
	static bool IsAppDarkTheme();
	static bool IsSystemDarkTheme();
};
