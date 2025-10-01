#include <pch.h>

#include <filesystem>
#include <sstream>

std::string StrUtil::ToString(const std::wstring& string) {
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
		CP_UTF8, 0, string.data(), static_cast<int>(string.size()),
		result.data(), sizeNeeded,
		nullptr,
		nullptr
	);
	return result;
}

std::string StrUtil::ToString(const wchar_t* string) {
	// WideCharToMultiByteを使用して、ワイド文字列をマルチバイト文字列に変換
	int bufferSize = WideCharToMultiByte(CP_UTF8, 0, string, -1, nullptr, 0,
	                                     nullptr, nullptr);
	std::string ret(bufferSize - 1, 0);
	WideCharToMultiByte(CP_UTF8, 0, string, -1, ret.data(), bufferSize, nullptr,
	                    nullptr);
	return ret;
}

std::wstring StrUtil::ToWString(const std::string& string) {
	if (string.empty()) {
		return {};
	}

	int sizeNeeded = MultiByteToWideChar(
		CP_UTF8, 0, string.data(), static_cast<int>(string.size()), nullptr, 0
	);
	if (sizeNeeded == 0) {
		return {};
	}
	std::wstring result(sizeNeeded, 0);
	int          written = MultiByteToWideChar(
		CP_UTF8, 0, string.data(), static_cast<int>(string.size()),
		result.data(), sizeNeeded
	);
	result.resize(written);
	return result;
}

std::string StrUtil::ToLowerCase(const std::string& input) {
	std::string result = input;
	std::ranges::transform(
		result,
		result.begin(),
		[](const unsigned char c) {
			return static_cast<char>(std::tolower(c));
		}
	);
	return result;
}

bool StrUtil::Equal(const std::string& str1, const std::string& str2) {
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

std::string StrUtil::Join(
	const std::vector<std::string>& args,
	const char*                     delimiter
) {
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

/// @brief 軸を表す整数(0,1,2)を文字列に変換します
/// @param i 軸を表す整数(0,1,2)
/// @return 軸を表す文字列("X","Y","Z")
std::string StrUtil::DescribeAxis(const int& i) {
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

/// @brief UnicodeコードポイントをUTF-8に変換します
/// @param codePoint Unicodeコードポイント
/// @return UTF-8文字列
std::string StrUtil::ConvertToUtf8(const uint32_t codePoint) {
	std::string utf8String;

	if (codePoint <= 0x7F) {
		// 1バイト形式: 0xxxxxxx
		utf8String += static_cast<char>(codePoint);
	} else if (codePoint <= 0x7FF) {
		// 2バイト形式: 110xxxxx 10xxxxxx
		utf8String += static_cast<char>(0xC0 | ((codePoint >> 6) & 0x1F));
		utf8String += static_cast<char>(0x80 | (codePoint & 0x3F));
	} else if (codePoint <= 0xFFFF) {
		// 3バイト形式: 1110xxxx 10xxxxxx 10xxxxxx
		utf8String += static_cast<char>(0xE0 | ((codePoint >> 12) & 0x0F));
		utf8String += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
		utf8String += static_cast<char>(0x80 | (codePoint & 0x3F));
	} else if (codePoint <= 0x10FFFF) {
		// 4バイト形式: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
		utf8String += static_cast<char>(0xF0 | ((codePoint >> 18) & 0x07));
		utf8String += static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F));
		utf8String += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
		utf8String += static_cast<char>(0x80 | (codePoint & 0x3F));
	} else {
	}

	return utf8String;
}

std::vector<int> StrUtil::ParseVersion(const std::string& version) {
	std::vector<int>  result;
	std::stringstream ss(version);
	std::string       item;

	while (std::getline(ss, item, '.')) {
		result.emplace_back(std::stoi(item));
	}

	return result;
}

/// @brief ファイルパスから拡張子(小文字)を取得します
/// @param str ファイルパス
/// @return 拡張子(小文字)
std::string StrUtil::ToLowerExt(const std::string_view& str) {
	std::string e = std::filesystem::path(std::string(str)).extension().
		string();
	for (auto& c : e) {
		c = static_cast<std::string::value_type>(std::tolower(c));
	}
	return e;
}
