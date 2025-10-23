#pragma once
#include <string>
#include <unordered_map>


/// @brief INIファイルパーサークラス
/// @details INI形式の設定ファイルを読み込み、パースします
class IniParser {
public:
	static std::unordered_map<std::string, std::unordered_map<
		                          std::string, std::string>> ParseIniFile(
		const std::string& filePath);

private:
	static std::string Trim(const std::string& str);
};
