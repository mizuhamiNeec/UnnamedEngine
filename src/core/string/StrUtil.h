#pragma once
#include <string>
#include <string_view>
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

	/// @brief 文字列ビューを小文字に変換する
	/// @param input 入力文字列ビュー
	/// @return 小文字に変換された文字列
	std::string ToLowerCase(std::string_view input);

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

	/// @brief 文字列ビューの前後の空白を削除する
	/// @param string 対象文字列ビュー
	/// @return トリムされた文字列
	std::string TrimSpaces(std::string_view string);

	/// @brief 文字列ビューの前後からCロケールのASCII空白文字を削除する
	/// @param string 対象文字列ビュー
	/// @return space、tab、改行、復帰、form-feed、vertical-tabを除いた文字列
	std::string TrimAsciiWhitespace(std::string_view string);

	/// @brief 2つの文字列を大文字小文字を無視して比較する
	/// @param lhs 左辺
	/// @param rhs 右辺
	/// @return 大文字小文字を無視して一致すればtrue
	bool EqualsIgnoreCase(std::string_view lhs, std::string_view rhs);

	/// @brief 文字列が指定の接頭辞で始まるかを大文字小文字を無視して判定する
	/// @param string 対象文字列
	/// @param prefix 接頭辞
	/// @return 接頭辞で始まればtrue
	bool StartsWithIgnoreCase(
		std::string_view string, std::string_view prefix
	);

	/// @brief 文字列が指定の接尾辞で終わるかを大文字小文字を無視して判定する
	/// @param string 対象文字列
	/// @param suffix 接尾辞
	/// @return 接尾辞で終わればtrue
	bool EndsWithIgnoreCase(
		std::string_view string, std::string_view suffix
	);

	/// @brief 文字列内でクエリを大文字小文字を無視して検索する
	/// @param text 検索対象文字列
	/// @param query 検索クエリ
	/// @return 最初の一致位置。一致しない場合はstd::string::npos
	std::size_t FindIgnoreCase(
		std::string_view text, std::string_view query
	);

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
