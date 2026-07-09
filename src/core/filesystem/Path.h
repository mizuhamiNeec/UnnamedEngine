#pragma once
#include <filesystem>
#include <format>
#include <string>
#include <string_view>
#include <system_error>

namespace Unnamed {
	/// @brief OS上の物理ファイルパスを表す。
	/// @details Windowsでは内部的にUTF-16のstd::filesystem::pathとして保持する。
	class Path final {
	public:
		Path() = default;
		/// @brief UTF-8文字列からパスを生成します。
		/// @param utf8 UTF-8エンコードされたパス文字列
		explicit Path(const char* utf8);

		/// @brief UTF-8文字列からパスを生成します。
		/// @param utf8 UTF-8エンコードされたパス文字列
		explicit Path(const std::string& utf8);

		/// @brief UTF-8文字列からパスを生成します。
		/// @param utf8 UTF-8エンコードされたパス文字列
		explicit Path(std::string_view utf8);

		/// @brief OSネイティブのfilesystem::pathから生成します。
		/// @param nativePath OSネイティブのパス
		/// @return 生成されたPathオブジェクト
		[[nodiscard]] static Path FromNative(std::filesystem::path nativePath);

		/// @brief OSネイティブのfilesystem::pathを取得します。
		/// @details 主にファイル操作に使用されます。
		/// @return OSネイティブのfilesystem::path
		[[nodiscard]] const std::filesystem::path& Native() const noexcept;

		/// @brief 標準ライブラリのファイルAPIへ渡すためのネイティブパス参照へ変換します。
		[[nodiscard]] explicit operator const std::filesystem::path
		&() const noexcept;

		/// @brief generic UTF-8文字列へ明示変換します。
		[[nodiscard]] explicit operator std::string() const;

		/// @brief パスをUTF-8文字列として取得します。
		/// @return UTF-8エンコードされたパス文字列
		[[nodiscard]] std::string ToUtf8() const;

		/// @brief パスを汎用区切りのUTF-8文字列として取得します。
		/// @return UTF-8エンコードされたgeneric path文字列
		[[nodiscard]] std::string ToGenericUtf8() const;

		/// @brief OSネイティブのfilesystem::pathをUTF-8文字列へ変換します。
		/// @param nativePath OSネイティブのパス
		/// @return UTF-8エンコードされたパス文字列
		[[nodiscard]] static std::string ToUtf8String(
			const std::filesystem::path& nativePath
		);

		/// @brief PathをUTF-8文字列へ変換します。
		/// @param path 変換対象パス
		/// @return UTF-8エンコードされたパス文字列
		[[nodiscard]] static std::string ToUtf8String(const Path& path);

		/// @brief OSネイティブのfilesystem::pathを汎用区切りのUTF-8文字列へ変換します。
		/// @param nativePath OSネイティブのパス
		/// @return UTF-8エンコードされたgeneric path文字列
		[[nodiscard]] static std::string ToGenericUtf8(
			const std::filesystem::path& nativePath
		);

		/// @brief Pathを汎用区切りのUTF-8文字列へ変換します。
		/// @param path 変換対象パス
		/// @return UTF-8エンコードされたgeneric path文字列
		[[nodiscard]] static std::string ToGenericUtf8(const Path& path);

		/// @brief UTF-8パスでstd::filesystem::existsを評価します。
		[[nodiscard]] static bool ExistsUtf8(
			std::string_view utf8Path, std::error_code& ec
		);

		/// @brief UTF-8パスでstd::filesystem::file_sizeを取得します。
		[[nodiscard]] static uintmax_t FileSizeUtf8(
			std::string_view utf8Path, std::error_code& ec
		);

		/// @brief UTF-8パスでstd::filesystem::last_write_timeを取得します。
		[[nodiscard]] static std::filesystem::file_time_type LastWriteTimeUtf8(
			std::string_view utf8Path, std::error_code& ec
		);

		/// @brief ベースディレクトリからの相対パスを解決します。
		[[nodiscard]] static Path ResolveRelativePath(
			const std::filesystem::path& baseDir, std::string_view path
		);

		/// @brief パスを空にします。
		void Clear() noexcept;

		/// @brief パスが空かを判定します。
		/// @return パスが空である場合はtrue、そうでない場合はfalse
		[[nodiscard]] bool IsEmpty() const noexcept;

		/// @brief パスが絶対パスであるかを判定します。
		/// @return パスが絶対パスである場合はtrue、そうでない場合はfalse
		[[nodiscard]] bool IsAbsolute() const noexcept;

		/// @brief パスが相対パスであるかを判定します。
		/// @return パスが相対パスである場合はtrue、そうでない場合はfalse
		[[nodiscard]] bool IsRelative() const noexcept;

		/// @brief パスがディレクトリであるかを判定します。
		/// @return パスがディレクトリである場合はtrue、そうでない場合はfalse
		[[nodiscard]] bool IsDirectory() const;

		/// @brief パスが通常のファイルであるかを判定します。
		/// @return パスが通常のファイルである場合はtrue、そうでない場合はfalse
		[[nodiscard]] bool IsRegularFile() const;

		/// @brief パスが存在するかを判定します。
		/// @return パスが存在する場合はtrue、そうでない場合はfalse
		[[nodiscard]] bool Exists() const noexcept;

		/// @brief パスの親ディレクトリを取得します。
		/// @return パスの親ディレクトリを表すPathオブジェクト
		[[nodiscard]] Path ParentPath() const;

		/// @brief パスのファイル名を取得します。
		/// @return パスのファイル名を表すPathオブジェクト
		[[nodiscard]] Path FileName() const;

		/// @brief パスのstemを取得します。
		/// @return 拡張子を除いたファイル名を表すPathオブジェクト
		[[nodiscard]] Path Stem() const;

		/// @brief パスの拡張子を取得します。
		/// @return パスの拡張子を表すPathオブジェクト
		[[nodiscard]] Path Extension() const;

		/// @brief パスを正規化します
		/// @return 正規化されたPathオブジェクト
		[[nodiscard]] Path LexicallyNormal() const;

		/// @brief パスを結合します。
		/// @param rhs 結合するパス
		/// @return 結合されたPathオブジェクト
		[[nodiscard]] Path operator/(const Path& rhs) const;

		/// @brief パスを結合します。
		/// @param rhs 結合するパス
		/// @return 結合されたPathオブジェクト
		[[nodiscard]] Path& operator/=(const Path& rhs);

		/// @brief パスが等しいかを比較する。
		/// @param lhs 比較する左辺のPathオブジェクト
		/// @param rhs 比較する右辺のPathオブジェクト
		/// @return パスが等しい場合はtrue、そうでない場合はfalse
		[[nodiscard]] friend bool operator==(
			const Path& lhs, const Path& rhs
		) noexcept = default;

	private:
		static Path FromUtf8(std::string_view utf8);

		explicit Path(std::filesystem::path nativePath);

		std::filesystem::path mNativePath;
	};
}

template <>
struct std::formatter<Unnamed::Path, char> : std::formatter<std::string, char> {
	auto format(const Unnamed::Path& path, std::format_context& ctx) const {
		return std::formatter<std::string, char>::format(path.ToUtf8(), ctx);
	}
};
