#pragma once
#include <filesystem>
#include <string>
#include <string_view>

namespace Path {
	/// @brief UTF-8 文字列パスをプラットフォームのネイティブ `std::filesystem::path` に変換します。
	/// @param utf8Path UTF-8 エンコードされたパス
	/// @return ネイティブエンコードのファイルシステムパス
	[[nodiscard]] std::filesystem::path FromUtf8(std::string_view utf8Path);

	/// @brief `std::filesystem::path` を UTF-8 文字列へ変換します。
	/// @param path 変換対象パス
	/// @return UTF-8 エンコード文字列
	[[nodiscard]] std::string ToUtf8String(const std::filesystem::path& path);

	/// @brief `std::filesystem::path` を汎用区切り（`/`）の UTF-8 文字列へ変換します。
	/// @param path 変換対象パス
	/// @return UTF-8 エンコードされた generic path 文字列
	[[nodiscard]] std::string ToGenericUtf8(const std::filesystem::path& path);

	/// @brief UTF-8 パスで `std::filesystem::exists` を評価します。
	/// @param utf8Path UTF-8 エンコードされたパス
	/// @param ec エラーコード
	/// @return 存在する場合 true
	[[nodiscard]] bool ExistsUtf8(
		std::string_view utf8Path, std::error_code& ec
	);

	/// @brief UTF-8 パスで `std::filesystem::file_size` を取得します。
	/// @param utf8Path UTF-8 エンコードされたパス
	/// @param ec エラーコード
	/// @return ファイルサイズ（バイト）
	[[nodiscard]] uintmax_t FileSizeUtf8(
		std::string_view utf8Path, std::error_code& ec
	);

	/// @brief UTF-8 パスで `std::filesystem::last_write_time` を取得します。
	/// @param utf8Path UTF-8 エンコードされたパス
	/// @param ec エラーコード
	/// @return 最終更新時刻
	[[nodiscard]] std::filesystem::file_time_type LastWriteTimeUtf8(
		std::string_view utf8Path, std::error_code& ec
	);

	/// @brief ベースディレクトリからの相対パスを解決する。パスがすでに絶対パスの場合は、そのまま正規化して返す。
	/// @param baseDir ベースディレクトリ
	/// @param path 解決するパス
	/// @return 解決されたパス
	[[nodiscard]] std::string ResolveRelativePath(
		const std::filesystem::path& baseDir, std::string path
	);
}
