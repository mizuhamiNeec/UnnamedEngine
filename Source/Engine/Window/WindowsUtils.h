#pragma once

#include <Windows.h>

class WindowsUtils {
public:
	static bool RegistryGetDWord(HKEY hKeyParent, char const* key, char const* name, DWORD* pData);
	static bool IsAppDarkTheme();
	static bool IsSystemDarkTheme();
};

