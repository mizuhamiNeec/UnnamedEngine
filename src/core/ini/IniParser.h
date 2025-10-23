#pragma once
#include <string>
#include <unordered_map>

/**
 * @brief INIファイルパーサークラス
 * @details INI形式の設定ファイルを読み込み、パースします
 */
class IniParser {
public:
	/**
	 * @brief INIファイルをパースする
	 * @param filePath INIファイルのパス
	 * @return セクション名とキー・値のペアのマップ
	 */
	static std::unordered_map<std::string, std::unordered_map<std::string, std::string>> ParseIniFile(const std::string& filePath);

private:
	/**
	 * @brief 文字列の前後の空白を削除する
	 * @param str 対象文字列
	 * @return トリムされた文字列
	 */
	static std::string Trim(const std::string& str);
};

