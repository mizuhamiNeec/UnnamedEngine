#pragma once
#include <string>
#include <windows.h>

inline std::wstring ConvertString(const std::string& string) {
	if (string.empty()) { return {}; }

	const auto sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, string.data(), static_cast<int>(string.size()), nullptr, 0);
	if (sizeNeeded == 0) { return {}; }
	std::wstring result(sizeNeeded, 0);
	MultiByteToWideChar(CP_UTF8, 0, string.data(), static_cast<int>(string.size()), result.data(), sizeNeeded);
	return result;
}

inline std::string ConvertString(const std::wstring& string) {
	if (string.empty()) { return {}; }

	const auto sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, string.data(), static_cast<int>(string.size()), nullptr, 0,
		nullptr, nullptr);
	if (sizeNeeded == 0) { return {}; }
	std::string result(sizeNeeded, 0);
	WideCharToMultiByte(CP_UTF8, 0, string.data(), static_cast<int>(string.size()), result.data(), sizeNeeded, nullptr,
		nullptr);
	return result;
}
