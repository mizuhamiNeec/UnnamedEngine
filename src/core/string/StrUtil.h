#pragma once
#include <string>
#include <vector>

class StrUtil {
public:
	static std::string ToString(const std::wstring& string);
	static std::string ToString(const wchar_t* string);

	static std::wstring ToWString(const std::string& string);

	static std::string ToLowerCase(const std::string& input);

	static bool Equal(const std::string& str1, const std::string& str2);

	static std::string Join(const std::vector<std::string>& args,
	                        const char*                     delimiter);

	static std::string DescribeAxis(const int& i);

	static std::string ConvertToUtf8(uint32_t codePoint);

	static std::vector<int> ParseVersion(const std::string& version);

	static std::string ToLowerExt(const std::string_view& str);

	static bool IsFloat(const std::string& str);
};
