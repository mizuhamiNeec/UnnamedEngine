#include "PathUtil.h"

#include "core/string/StrUtil.h"

namespace Path {
	std::filesystem::path FromUtf8(const std::string_view utf8Path) {
		if (utf8Path.empty()) {
			return {};
		}

#ifdef _WIN32
		// Windows は UTF-16 ネイティブ API 前提なので、UTF-8 を明示的に UTF-16 に変換して保持する。
		return std::filesystem::path(Unnamed::StrUtil::ToWString(std::string(utf8Path)));
#else
		return std::filesystem::path(std::string(utf8Path));
#endif
	}

	std::string ToUtf8String(const std::filesystem::path& path) {
#ifdef _WIN32
		return Unnamed::StrUtil::ToString(path.wstring());
#else
		return path.string();
#endif
	}

	std::string ToGenericUtf8(const std::filesystem::path& path) {
#ifdef _WIN32
		return Unnamed::StrUtil::ToString(path.generic_wstring());
#else
		return path.generic_string();
#endif
	}

	bool ExistsUtf8(const std::string_view utf8Path, std::error_code& ec) {
		return std::filesystem::exists(FromUtf8(utf8Path), ec);
	}

	uintmax_t FileSizeUtf8(const std::string_view utf8Path, std::error_code& ec) {
		return std::filesystem::file_size(FromUtf8(utf8Path), ec);
	}

	std::filesystem::file_time_type LastWriteTimeUtf8(
		const std::string_view utf8Path,
		std::error_code&       ec
	) {
		return std::filesystem::last_write_time(FromUtf8(utf8Path), ec);
	}

	std::string ResolveRelativePath(
		const std::filesystem::path& baseDir, std::string path
	) {
		if (path.empty()) {
			return path;
		}

		std::filesystem::path p = FromUtf8(path);
		if (p.is_relative()) {
			p = baseDir / p;
		}
		return Unnamed::StrUtil::NormalizePath(
			ToGenericUtf8(p.lexically_normal())
		);
	}
}
