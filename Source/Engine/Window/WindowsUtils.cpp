#include "WindowsUtils.h"

#include <dwmapi.h>
#include <lmcons.h>
#include <vector>

#pragma comment(lib, "Dwmapi.lib")

std::string WindowsUtils::GetWindowsUserName() {
	DWORD bufferSize = UNLEN + 1; // +1 は　null文字分
	std::vector<char> buffer(bufferSize);
	if (GetUserNameA(buffer.data(), &bufferSize)) {
		return std::string(buffer.data());
	}
	return std::string("名無しの権兵衛");
}

//-----------------------------------------------------------------------------
// Purpose: 指定されたレジストリのキーを開き、名前に関連付けられたDWORD値を返します
//-----------------------------------------------------------------------------
bool WindowsUtils::RegistryGetDWord(const HKEY hKeyParent, char const* key, char const* name, DWORD* pData) {
	DWORD len = sizeof(DWORD);
	HKEY hKey = nullptr;
	DWORD rc = RegOpenKeyExA(hKeyParent, key, 0, KEY_READ, &hKey);
	if (rc == ERROR_SUCCESS) {
		rc = RegQueryValueExA(hKey, name, nullptr, nullptr, reinterpret_cast<LPBYTE>(pData), &len);
	}
	RegCloseKey(hKey);
	return rc == ERROR_SUCCESS;
}

//-----------------------------------------------------------------------------
// Purpose: アプリケーションのテーマがダークモードかどうかを問い合わせます
//-----------------------------------------------------------------------------
bool WindowsUtils::IsAppDarkTheme() {
	DWORD lightMode = 1;
	RegistryGetDWord(
		HKEY_CURRENT_USER,
		R"(Software\Microsoft\Windows\CurrentVersion\Themes\Personalize)",
		"AppsUseLightTheme",
		&lightMode
	);
	return lightMode == 0;
}

//-----------------------------------------------------------------------------
// Purpose: システムのテーマがダークモードかどうかを問い合わせます
//-----------------------------------------------------------------------------
bool WindowsUtils::IsSystemDarkTheme() {
	DWORD lightMode = 1;
	RegistryGetDWord(
		HKEY_CURRENT_USER,
		R"(Software\Microsoft\Windows\CurrentVersion\Themes\Personalize)",
		"SystemUsesLightTheme",
		&lightMode
	);
	return lightMode == 0;
}
