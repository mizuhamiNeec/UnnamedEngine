#pragma once

#include <string>
#include <Windows.h>

class WindowsUtils {
public:
	static void RegisterRawInput(const HWND& hwnd);

	static std::string GetWindowsUserName();
	static std::string GetHresultMessage(HRESULT hr);
	static bool RegistryGetDWord(HKEY hKeyParent, char const* key, char const* name, DWORD* pData);
	static bool IsAppDarkTheme();
	static bool IsSystemDarkTheme();
};
