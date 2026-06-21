#pragma once
#include <string>
#include <vector>

namespace Unnamed {
	class Path;
}

///@brief 文字列ユーティリティクラス
///@details 文字列の変換、操作、解析のための静的ヘルパー関数を提供します
namespace Unnamed::StrUtil {
	/// @brief ワイド文字列をマルチバイト文字列に変換する
	/// @param string ワイド文字列
	/// @return マルチバイト文字列
	std::string ToString(const std::wstring& string);

	/// @brief ワイド文字列をマルチバイト文字列に変換する
	/// @param string ワイド文字列ポインタ
	/// @return マルチバイト文字列
	std::string ToString(const wchar_t* string);

	/// @brief マルチバイト文字列をワイド文字列に変換する
	/// @param string マルチバイト文字列
	/// @return ワイド文字列
	std::wstring ToWString(const std::string& string);

	/// @brief 文字列を小文字に変換する
	/// @param input 入力文字列
	/// @return 小文字に変換された文字列
	std::string ToLowerCase(const std::string& input);

	/// @brief 文字列配列をデリミタで結合する
	/// @param args 文字列配列
	/// @param delimiter デリミタ文字列
	/// @return 結合された文字列
	std::string Join(
		const std::vector<std::string>& args, const char* delimiter
	);

	/// @brief 文字列をstd::string_viewからstd::stringに変換する
	/// @param str 入力文字列ビュー
	/// @return std::stringに変換された文字列
	std::string ViewToString(const std::string_view& str);

	/// @brief 軸のインデックスを説明文字列に変換する
	/// @param i 軸インデックス
	/// @return 軸の説明文字列
	std::string DescribeAxis(const int& i);

	/// @brief Unicodeコードポイントをutf-8文字列に変換する
	/// @param codePoint Unicodeコードポイント
	/// @return UTF-8文字列
	std::string ConvertToUtf8(uint32_t codePoint);

	/// @brief バージョン文字列を解析する
	/// @param version バージョン文字列（例: "1.2.3"）
	/// @return バージョン番号の配列
	std::vector<int> ParseVersion(const std::string& version);

	/// @brief 文字列から両端の二重引用符を削除する
	/// @param str 入力文字列
	/// @return 二重引用符が削除された文字列
	std::string RemoveDoubleQuotes(const std::string_view& str);

	/// @brief 文字列が浮動小数点数かを判定する
	/// @param str 判定する文字列
	/// @return 浮動小数点数の場合true
	bool IsFloat(const std::string& str);

	/// @brief コマンド文字列をセミコロンで分割する
	/// @param command 入力文字列
	/// @return コマンドの配列
	std::vector<std::string> SplitCommands(const std::string_view& command);

	/// @brief 文字列をトークンに分割する
	/// @param command 入力文字列
	/// @return トークンの配列
	std::vector<std::string> Tokenize(const std::string_view& command);

	/// @brief 文字列の前後の空白を削除する
	/// @param string 対象文字列
	/// @return トリムされた文字列
	std::string TrimSpaces(const std::string& string);

	/// @brief ファイルを読み込み、内容を文字列として返す
	/// @param path ファイルパス
	/// @param outString ファイル内容の文字列
	/// @return 成功したらtrueを返す
	bool ReadFileToString(const Path& path, std::string& outString);

	/// @brief ブール文字列かを判定する
	/// @param str 判定する文字列
	/// @return ブール文字列の場合true
	bool CheckBoolString(std::string str);

	struct LinkSpan {
		std::size_t begin;
		std::size_t end; // [begin, end]
		bool        isRelativePath = false;
	};

	/// @brief 行からリンクを解析する
	/// @param line 入力行文字列
	/// @return リンクのスパン配列
	std::vector<LinkSpan> ParseLinksFromLine(std::string_view line);
}
