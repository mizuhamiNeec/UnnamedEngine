#include <pch.h>

#include <fstream>
#include <sstream>

#include "core/filesystem/Path.h"

namespace Unnamed::StrUtil {
	std::string ToString(const std::wstring& string) {
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

	std::string ToString(const wchar_t* string) {
		// WideCharToMultiByteを使用して、ワイド文字列をマルチバイト文字列に変換
		const int bufferSize = WideCharToMultiByte(
			CP_UTF8, 0, string, -1, nullptr, 0,
			nullptr, nullptr
		);
		if (bufferSize == 0) {
			return {};
		}
		std::string ret(bufferSize, 0);
		WideCharToMultiByte(
			CP_UTF8, 0, string, -1, ret.data(), bufferSize,
			nullptr,
			nullptr
		);
		if (!ret.empty()) {
			ret.pop_back();
		}
		return ret;
	}

	std::wstring ToWString(const std::string& string) {
		if (string.empty()) {
			return {};
		}

		const int sizeNeeded = MultiByteToWideChar(
			CP_UTF8, 0, string.data(), static_cast<int>(string.size()), nullptr,
			0
		);
		if (sizeNeeded == 0) {
			return {};
		}
		std::wstring result(sizeNeeded, 0);
		const int    written = MultiByteToWideChar(
			CP_UTF8, 0, string.data(), static_cast<int>(string.size()),
			result.data(), sizeNeeded
		);
		result.resize(written);
		return result;
	}

	std::string ToLowerCase(const std::string& input) {
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

	std::string Join(
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

	std::string ViewToString(const std::string_view& str) {
		return {str.data(), str.size()};
	}

	std::string DescribeAxis(const int& i) {
		switch (i) {
			case 0: return "X";
			case 1: return "Y";
			case 2: return "Z";
			default: return "Unknown";
		}
	}

	std::string ConvertToUtf8(const uint32_t codePoint) {
		std::string utf8String;

		if (codePoint <= 0x7F) {
			// 1バイト形式: 0xxxxxxx
			utf8String += static_cast<char>(codePoint);
		} else if (codePoint <= 0x7FF) {
			// 2バイト形式: 110xxxxx 10xxxxxx
			utf8String += static_cast<char>(0xC0 | codePoint >> 6 & 0x1F);
			utf8String += static_cast<char>(0x80 | codePoint & 0x3F);
		} else if (codePoint <= 0xFFFF) {
			// 3バイト形式: 1110xxxx 10xxxxxx 10xxxxxx
			utf8String += static_cast<char>(0xE0 | codePoint >> 12 & 0x0F);
			utf8String += static_cast<char>(0x80 | codePoint >> 6 & 0x3F);
			utf8String += static_cast<char>(0x80 | codePoint & 0x3F);
		} else if (codePoint <= 0x10FFFF) {
			// 4バイト形式: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
			utf8String += static_cast<char>(0xF0 | codePoint >> 18 & 0x07);
			utf8String += static_cast<char>(0x80 | codePoint >> 12 & 0x3F);
			utf8String += static_cast<char>(0x80 | codePoint >> 6 & 0x3F);
			utf8String += static_cast<char>(0x80 | codePoint & 0x3F);
		} else {
		}

		return utf8String;
	}

	std::vector<int> ParseVersion(const std::string& version) {
		std::vector<int>  result;
		std::stringstream ss(version);
		std::string       item;

		while (std::getline(ss, item, '.')) {
			result.emplace_back(std::stoi(item));
		}

		return result;
	}

	std::string RemoveDoubleQuotes(const std::string_view& str) {
		std::string result;
		result.reserve(str.size());
		for (const char c : str) {
			if (c != '"') {
				result += c;
			}
		}
		return result;
	}

	bool IsFloat(const std::string& str) {
		try {
			[[maybe_unused]] auto d = std::stof(str);
			return true;
		} catch (...) {
			return false;
		}
	}

	std::vector<std::string> SplitCommands(const std::string_view& command) {
		std::vector<std::string> result;
		std::string              current;
		bool                     inQuotes = false;
		for (const char ch : command) {
			if (ch == '"') {
				inQuotes = !inQuotes;
				current  += ch;
			} else if (ch == ';' && !inQuotes) {
				result.emplace_back(current);
				current.clear();
			} else {
				current += ch;
			}
		}

		if (!current.empty()) {
			result.emplace_back(current);
		}

		return result;
	}

	std::vector<std::string> Tokenize(const std::string_view& command) {
		std::istringstream       stream{std::string(command)};
		std::vector<std::string> tokens;
		std::string              token;

		while (stream >> token) {
			tokens.emplace_back(token);
		}

		return tokens;
	}

	std::string TrimSpaces(const std::string& string) {
		const size_t start = string.find_first_not_of(" \t\n\r");
		const size_t end   = string.find_last_not_of(" \t\n\r");
		if (start == std::string::npos || end == std::string::npos) {
			return "";
		}
		return string.substr(start, end - start + 1);
	}

	bool ReadFileToString(const Path& path, std::string& outString) {
		const std::ifstream ifs(path.Native(), std::ios::binary);
		if (!ifs) {
			return false;
		}
		std::stringstream ss;
		ss << ifs.rdbuf();
		outString = ss.str();
		return true;
	}

	bool CheckBoolString(std::string str) {
		str            = ToLowerCase(str);
		const bool ret = str == "1" || str == "true" || str == "yes" || str ==
		                 "on";
		return ret;
	}

	std::vector<LinkSpan> ParseLinksFromLine(const std::string_view line) {
		std::vector<LinkSpan> result;
		const auto            size = line.size();

		auto IsSpace = [](const unsigned char c) {
			return std::isspace(c) != 0;
		};

		auto IsTrailingPunct = [](const char c) {
			switch (c) {
				case '.':
				case ',':
				case ';':
				case ':':
				case ')':
				case ']':
				case '}':
				case '>':
				case '"':
				case '\'': return true;
				default: return false;
			}
		};

		auto StartsWithAt = [&](
			const std::size_t pos, const std::string_view prefix
		) {
			return pos + prefix.size() <= size &&
			       std::string_view(line.data() + pos, prefix.size()) == prefix;
		};

		for (std::size_t i = 0; i < size; ++i) {
			LinkSpan span{};
			bool     matched = false;

			// ブラウザリンク
			if (StartsWithAt(i, "http://") || StartsWithAt(i, "https://") ||
			    StartsWithAt(i, "file://")) {
				span.begin    = i;
				std::size_t j = i;
				while (j < size && !IsSpace(
					       static_cast<unsigned char>(line[j])
				       )) {
					++j;
				}
				// 末尾の句読点などを削る
				while (j > span.begin && IsTrailingPunct(line[j - 1])) {
					--j;
				}
				if (j > span.begin) {
					span.end = j;
					matched  = true;
				}
			}
			// Windows 絶対パス: C:\ or D:/ など
			else if (i + 2 < size &&
			         std::isalpha(static_cast<unsigned char>(line[i])) &&
			         line[i + 1] == ':' &&
			         (line[i + 2] == '\\' || line[i + 2] == '/')) {
				span.begin    = i;
				std::size_t j = i;
				while (j < size && !IsSpace(
					       static_cast<unsigned char>(line[j])
				       )) {
					++j;
				}
				while (j > span.begin && IsTrailingPunct(line[j - 1])) {
					--j;
				}
				if (j > span.begin + 3) {
					span.end = j;
					matched  = true;
				}
			}
			// 相対パス
			else if (StartsWithAt(i, "./") || StartsWithAt(i, ".\\") ||
			         StartsWithAt(i, "../") || StartsWithAt(i, "..\\")
			) {
				span.begin    = i;
				std::size_t j = i;
				while (j < size && !IsSpace(
					       static_cast<unsigned char>(line[j])
				       )) {
					++j;
				}
				while (j > span.begin && IsTrailingPunct(line[j - 1])) {
					--j;
				}
				if (j > span.begin + 2) {
					span.end = j;
					matched  = true;
				}
				span.isRelativePath = true;
			}

			if (matched) {
				result.emplace_back(span);
				// 見つけたリンクの末尾まで飛ばす
				i = span.end;
			}
		}

		return result;
	}
}
