#pragma once
#include <string>
#include <vector>

/**
 * @brief 文字列ユーティリティクラス
 * @details 文字列の変換、操作、解析のための静的ヘルパー関数を提供します
 */
class StrUtil {
public:
	/**
	 * @brief ワイド文字列をマルチバイト文字列に変換する
	 * @param string ワイド文字列
	 * @return マルチバイト文字列
	 */
	static std::string ToString(const std::wstring& string);

	/**
	 * @brief ワイド文字列をマルチバイト文字列に変換する
	 * @param string ワイド文字列ポインタ
	 * @return マルチバイト文字列
	 */
	static std::string ToString(const wchar_t* string);

	/**
	 * @brief マルチバイト文字列をワイド文字列に変換する
	 * @param string マルチバイト文字列
	 * @return ワイド文字列
	 */
	static std::wstring ToWString(const std::string& string);

	/**
	 * @brief 文字列を小文字に変換する
	 * @param input 入力文字列
	 * @return 小文字に変換された文字列
	 */
	static std::string ToLowerCase(const std::string& input);

	/**
	 * @brief 2つの文字列が等しいかを判定する
	 * @param str1 文字列1
	 * @param str2 文字列2
	 * @return 等しい場合true
	 */
	static bool Equal(const std::string& str1, const std::string& str2);

	/**
	 * @brief 文字列配列をデリミタで結合する
	 * @param args 文字列配列
	 * @param delimiter デリミタ文字列
	 * @return 結合された文字列
	 */
	static std::string Join(const std::vector<std::string>& args,
	                        const char*                     delimiter);

	/**
	 * @brief 軸のインデックスを説明文字列に変換する
	 * @param i 軸インデックス
	 * @return 軸の説明文字列
	 */
	static std::string DescribeAxis(const int& i);

	/**
	 * @brief Unicodeコードポイントをutf-8文字列に変換する
	 * @param codePoint Unicodeコードポイント
	 * @return UTF-8文字列
	 */
	static std::string ConvertToUtf8(uint32_t codePoint);

	/**
	 * @brief バージョン文字列を解析する
	 * @param version バージョン文字列（例: "1.2.3"）
	 * @return バージョン番号の配列
	 */
	static std::vector<int> ParseVersion(const std::string& version);

	/**
	 * @brief 拡張子を小文字に変換する
	 * @param str 入力文字列
	 * @return 小文字に変換された拡張子
	 */
	static std::string ToLowerExt(const std::string_view& str);

	/**
	 * @brief 文字列が浮動小数点数かを判定する
	 * @param str 判定する文字列
	 * @return 浮動小数点数の場合true
	 */
	static bool IsFloat(const std::string& str);
};
