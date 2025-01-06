#pragma once
#include <algorithm>
#include <string>
#include <vector>
#include <windows.h>

class StrUtils {
public:
	static std::wstring ToWString(const std::string& string) {
		if (string.empty()) {
			return {};
		}

		const auto sizeNeeded = MultiByteToWideChar(
			CP_UTF8, 0, string.data(), static_cast<int>(string.size()), nullptr,
			0
		);
		if (sizeNeeded == 0) {
			return {};
		}
		std::wstring result(sizeNeeded, 0);
		MultiByteToWideChar(CP_UTF8, 0, string.data(), static_cast<int>(string.size()), result.data(), sizeNeeded);
		return result;
	}

	static std::string ToString(const std::wstring& string) {
		if (string.empty()) {
			return {};
		}

		const auto sizeNeeded = WideCharToMultiByte(
			CP_UTF8, 0, string.data(), static_cast<int>(string.size()), nullptr,
			0,
			nullptr, nullptr
		);
		if (sizeNeeded == 0) {
			return {};
		}
		std::string result(sizeNeeded, 0);
		WideCharToMultiByte(
			CP_UTF8, 0, string.data(), static_cast<int>(string.size()), result.data(), sizeNeeded,
			nullptr,
			nullptr
		);
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
		int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), nullptr, 0);
		auto wstr = new wchar_t[sizeNeeded + 1]; // +1 は終端用
		MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), wstr, sizeNeeded);
		wstr[sizeNeeded] = L'\0'; // 終端を追加
		return wstr;
	}

	static std::string ToLowerCase(const std::string& input) {
		std::string result = input;
		std::transform(
			result.begin(),
			result.end(),
			result.begin(),
			[](unsigned char c) {
				return static_cast<char>(std::tolower(c));
			}
		);
		return result;
	}

	static bool Equal(const std::string& str1, const std::string& str2) {
		if (str1.size() != str2.size()) {
			return false;
		}
		return std::equal(
			str1.begin(),
			str1.end(),
			str2.begin(),
			[](unsigned char c1, unsigned char c2) {
				return std::tolower(c1) == std::tolower(c2);
			}
		);
	}

	static std::string Join(const std::vector<std::string>& args, const char* delimiter) {
		if (args.empty()) {
			return "";
		}

		std::string result;
		for (const auto& arg : args) {
			result += arg + delimiter;
		}
		if (!result.empty()) {
			result.pop_back();
		}
		return result;
	}

	static std::string DescribeAxis(const int& i) {
		switch (i) {
		case 0:
			return "X";
		case 1:
			return "Y";
		case 2:
			return "Z";
		default:
			return "Unknown";
		}
	}
};
