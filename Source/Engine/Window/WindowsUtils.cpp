#include "WindowsUtils.h"

#include <dwmapi.h>
#include <lmcons.h>
#include <vector>

#pragma comment(lib, "Dwmapi.lib")

std::string WindowsUtils::GetWindowsUserName() {
	DWORD bufferSize = UNLEN + 1; // +1 は null 文字分
	std::vector<char> buffer(bufferSize);
	if (GetUserNameA(buffer.data(), &bufferSize)) {
		std::string userName(buffer.data());
		// 空白をアンダースコアに置き換える
		for (char& ch : userName) {
			if (ch == ' ') {
				ch = '_';
			}
		}
		return userName;
	}
	return std::string("Windowsから取得できませんでした。");
}

//-----------------------------------------------------------------------------
// Purpose: HRESULTからのメッセージを取得します
//-----------------------------------------------------------------------------
std::string WindowsUtils::GetHresultMessage(const HRESULT hr) {
	char* messageBuffer = nullptr;
	const size_t messageLength = FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr,
		hr,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		reinterpret_cast<LPSTR>(&messageBuffer),
		0,
		nullptr
	);

	std::string message;
	if (messageLength > 0) {
		message = std::string(messageBuffer, messageLength);
	}
	else {
		message = "Unknown error code : " + std::to_string(hr);
	}

	// メモリ解放
	if (messageBuffer) {
		LocalFree(messageBuffer);
	}

	return message;
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
