#include "pch.h"
#include "Path.h"

#include "core/string/TextEncoding.h"

namespace Unnamed {
	Path::Path(const char* utf8) : Path(FromUtf8(utf8 == nullptr ? "" : utf8)) {
	}

	Path::Path(const std::string& utf8) : Path(FromUtf8(utf8)) {
	}

	Path::Path(const std::string_view utf8) : Path(FromUtf8(utf8)) {
	}

	Path Path::FromNative(std::filesystem::path nativePath) {
		return Path(std::move(nativePath));
	}

	const std::filesystem::path& Path::Native() const noexcept {
		return mNativePath;
	}

	Path::operator const std::filesystem::path&() const noexcept {
		return mNativePath;
	}

	Path::operator std::string() const {
		return ToUtf8();
	}

	std::string Path::ToUtf8() const {
		if (mNativePath.empty()) {
			return {};
		}

#ifdef _WIN32
		return TextEncoding::WideToUtf8(mNativePath.native());
#else
		return mNativePath.native();
#endif
	}

	std::string Path::ToGenericUtf8() const {
		return ToGenericUtf8(mNativePath);
	}

	std::string Path::ToUtf8String(
		const std::filesystem::path& nativePath
	) {
		return FromNative(nativePath).ToUtf8();
	}

	std::string Path::ToUtf8String(const Path& path) {
		return path.ToUtf8();
	}

	std::string Path::ToGenericUtf8(
		const std::filesystem::path& nativePath
	) {
		if (nativePath.empty()) {
			return {};
		}

#ifdef _WIN32
		return TextEncoding::WideToUtf8(nativePath.generic_wstring());
#else
		return nativePath.generic_string();
#endif
	}

	std::string Path::ToGenericUtf8(const Path& path) {
		return path.ToUtf8();
	}

	bool Path::ExistsUtf8(
		const std::string_view utf8Path, std::error_code& ec
	) {
		return std::filesystem::exists(FromUtf8(utf8Path).Native(), ec);
	}

	uintmax_t Path::FileSizeUtf8(
		const std::string_view utf8Path, std::error_code& ec
	) {
		return std::filesystem::file_size(FromUtf8(utf8Path).Native(), ec);
	}

	std::filesystem::file_time_type Path::LastWriteTimeUtf8(
		const std::string_view utf8Path, std::error_code& ec
	) {
		return last_write_time(
			FromUtf8(utf8Path).Native(), ec);
	}

	Path Path::ResolveRelativePath(
		const std::filesystem::path& baseDir, const std::string_view path
	) {
		if (path.empty()) {
			return {};
		}

		std::filesystem::path nativePath = FromUtf8(path).Native();
		if (nativePath.is_relative()) {
			nativePath = baseDir / nativePath;
		}
		return FromNative(nativePath.lexically_normal());
	}

	void Path::Clear() noexcept {
		mNativePath.clear();
	}

	bool Path::IsEmpty() const noexcept {
		return mNativePath.empty();
	}

	bool Path::IsAbsolute() const noexcept {
		return mNativePath.is_absolute();
	}

	bool Path::IsRelative() const noexcept {
		return mNativePath.is_relative();
	}

	bool Path::IsDirectory() const {
		return is_directory(mNativePath);
	}

	bool Path::IsRegularFile() const {
		return is_regular_file(mNativePath);
	}

	bool Path::Exists() const noexcept {
		return exists(mNativePath);
	}

	Path Path::ParentPath() const {
		return FromNative(mNativePath.parent_path());
	}

	Path Path::FileName() const {
		return FromNative(mNativePath.filename());
	}

	Path Path::Stem() const {
		return FromNative(mNativePath.stem());
	}

	Path Path::Extension() const {
		return FromNative(mNativePath.extension());
	}

	Path Path::LexicallyNormal() const {
		return FromNative(mNativePath.lexically_normal());
	}

	Path Path::operator/(const Path& rhs) const {
		return FromNative(mNativePath / rhs.mNativePath);
	}

	Path& Path::operator/=(const Path& rhs) {
		mNativePath /= rhs.mNativePath;
		return *this;
	}

	Path Path::FromUtf8(const std::string_view utf8) {
		if (utf8.empty()) {
			return {};
		}

#ifdef _WIN32
		return Path(
			std::filesystem::path(TextEncoding::Utf8ToWide(utf8))
		);
#else
		return Path(
			std::filesystem::path(std::string(utf8))
		);
#endif
	}

	Path::Path(std::filesystem::path nativePath) : mNativePath(
		std::move(nativePath)) {
	}
}
