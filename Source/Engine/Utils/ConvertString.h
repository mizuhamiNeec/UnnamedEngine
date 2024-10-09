#pragma once
#include <string>
#include <windows.h>

class ConvertString {
public:
	static std::wstring ToWString(const std::string& string) {
		if (string.empty()) { return {}; }

		const auto sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, string.data(), static_cast<int>(string.size()), nullptr, 0);
		if (sizeNeeded == 0) { return {}; }
		std::wstring result(sizeNeeded, 0);
		MultiByteToWideChar(CP_UTF8, 0, string.data(), static_cast<int>(string.size()), result.data(), sizeNeeded);
		return result;
	}

	static std::string ToString(const std::wstring& string) {
		if (string.empty()) { return {}; }

		const auto sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, string.data(), static_cast<int>(string.size()), nullptr, 0,
			nullptr, nullptr);
		if (sizeNeeded == 0) { return {}; }
		std::string result(sizeNeeded, 0);
		WideCharToMultiByte(CP_UTF8, 0, string.data(), static_cast<int>(string.size()), result.data(), sizeNeeded, nullptr,
			nullptr);
		return result;
	}

	static std::string ToString(const PWSTR string) {
		// WideCharToMultiByteを使用して、ワイド文字列をマルチバイト文字列に変換
		int size_needed = WideCharToMultiByte(CP_UTF8, 0, string, -1, nullptr, 0, nullptr, nullptr);
		std::string ret(size_needed, 0);
		WideCharToMultiByte(CP_UTF8, 0, string, -1, &ret[0], size_needed, nullptr, nullptr);
		return ret;
	}

	static wchar_t* ToString(const std::string& str) {
		// 変換に必要なバッファサイズを取得
		int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
		wchar_t* wstr = new wchar_t[sizeNeeded + 1]; // +1 は終端用
		MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), wstr, sizeNeeded);
		wstr[sizeNeeded] = L'\0'; // 終端を追加
		return wstr;
	}
};