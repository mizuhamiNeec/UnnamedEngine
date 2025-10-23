#include <dwmapi.h>
#include <dxgi.h>
#include <intrin.h>
#include <lmcons.h>
#include <vector>

#include <engine/Window/WindowsUtils.h>

#include <wrl/client.h>

#pragma comment(lib, "Dwmapi.lib")

/// @brief Windowsのユーザー名を取得します
/// @return ユーザー名文字列
std::string WindowsUtils::GetWindowsUserName() {
	DWORD             bufferSize = UNLEN + 1; // +1 は null 文字分
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

/// @brief Windowsのコンピュータ名を取得します
/// @return コンピュータ名文字列
std::string WindowsUtils::GetWindowsComputerName() {
	DWORD             bufferSize = MAX_COMPUTERNAME_LENGTH + 1; // +1 は null 文字分
	std::vector<char> buffer(bufferSize);
	if (GetComputerNameA(buffer.data(), &bufferSize)) {
		return std::string(buffer.data());
	}
	return std::string("Windowsから取得できませんでした。");
}

/// @brief Windowsのバージョンを取得します
/// @return バージョン文字列
std::string WindowsUtils::GetWindowsVersion() {
	return "Windows 11"; // TODO: !?
}

/// @brief CPUの名前を取得します
/// @return CPU名文字列
std::string WindowsUtils::GetCPUName() {
	int  cpuInfo[4] = {-1};
	char cpuBrand[0x40];
	__cpuid(cpuInfo, 0x80000002);
	memcpy(cpuBrand, cpuInfo, sizeof(cpuInfo));
	__cpuid(cpuInfo, 0x80000003);
	memcpy(cpuBrand + 16, cpuInfo, sizeof(cpuInfo));
	__cpuid(cpuInfo, 0x80000004);
	memcpy(cpuBrand + 32, cpuInfo, sizeof(cpuInfo));
	cpuBrand[48] = '\0';
	return std::string(cpuBrand);
}

/// @brief GPUの名前を取得します
/// @return GPU名文字列
std::string WindowsUtils::GetGPUName() {
	Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
	if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) {
		return "Failed to create DXGI Factory";
	}

	Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
	if (FAILED(factory->EnumAdapters1(0, &adapter))) {
		return "Failed to enumerate adapters";
	}

	DXGI_ADAPTER_DESC1 desc;
	adapter->GetDesc1(&desc);

	// ワイド文字列をマルチバイト文字列に変換
	const int sizeNeeded = WideCharToMultiByte(
		CP_UTF8, 0, desc.Description, -1, nullptr, 0, nullptr, nullptr);
	std::string gpuName(sizeNeeded - 1, 0); // 終端の null 文字を除くために -1
	WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, gpuName.data(),
	                    sizeNeeded, nullptr, nullptr);

	const int vRam = static_cast<int>(desc.DedicatedVideoMemory / (static_cast<
		unsigned long long>(1024) * 1024));
	std::string ret = gpuName + " " + std::to_string(vRam) + " MB";
	return ret;
}

/// @brief 最大RAM容量を取得します
/// @return 最大RAM容量文字列
std::string WindowsUtils::GetRamMax() {
	MEMORYSTATUSEX memoryStatus;
	memoryStatus.dwLength = sizeof(memoryStatus);
	GlobalMemoryStatusEx(&memoryStatus);
	return std::to_string(memoryStatus.ullTotalPhys / 1024 / 1024) + "MB";
}

/// @brief 使用中のRAM容量を取得します
/// @return 使用中のRAM容量文字列
std::string WindowsUtils::GetRamUsage() {
	MEMORYSTATUSEX memoryStatus;
	memoryStatus.dwLength = sizeof(memoryStatus);
	GlobalMemoryStatusEx(&memoryStatus);
	return std::to_string(
			(memoryStatus.ullTotalPhys - memoryStatus.ullAvailPhys) / 1024 /
			1024) +
		"MB";
}

/// @brief HRESULTコードに対応するメッセージを取得します
/// @param hr HRESULTコード
/// @return メッセージ文字列
std::string WindowsUtils::GetHresultMessage(const HRESULT hr) {
	char*        messageBuffer = nullptr;
	const size_t messageLength = FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
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
	} else {
		message = "Unknown error code : " + std::to_string(hr);
	}

	// メモリ解放
	if (messageBuffer) {
		LocalFree(messageBuffer);
	}

	return message;
}

/// @brief レジストリからDWORD値を取得します
/// @param hKeyParent 親キーのハンドル
/// @param key キーのパス
/// @param name 値の名前
/// @param pData 取得したDWORD値の格納先ポインタ
/// @return 成功したらtrueを返す
bool WindowsUtils::RegistryGetDWord(void*       hKeyParent, const char* key,
                                    const char* name, unsigned long*    pData) {
	DWORD len  = sizeof(DWORD);
	HKEY  hKey = nullptr;
	DWORD rc   = RegOpenKeyExA(static_cast<HKEY>(hKeyParent), key, 0, KEY_READ,
	                         &hKey);
	if (rc == ERROR_SUCCESS) {
		rc = RegQueryValueExA(hKey, name, nullptr, nullptr,
		                      reinterpret_cast<LPBYTE>(pData), &len);
	}
	RegCloseKey(hKey);
	return rc == ERROR_SUCCESS;
}

/// @brief アプリのテーマがダークモードかどうかを問い合わせます
/// @return ダークモードならtrueを返す
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

/// @brief アプリのテーマがダークモードかどうかを問い合わせます
/// @return ダークモードならtrueを返す
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
