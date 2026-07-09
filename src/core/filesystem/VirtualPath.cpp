#include "pch.h"
#include "VirtualPath.h"

namespace Unnamed {
	constexpr std::string_view kInvalidCharacters = ("<>:\"|?*");

	namespace {
		[[nodiscard]]
		bool IsInvalidAsciiCharacter(
			const char character
		) noexcept {
			// ASCII制御文字は無効とする
			if (
				const auto uChar = static_cast<unsigned char>(character);
				uChar < 0x20
			) {
				return true;
			}

			return kInvalidCharacters.find(character) != std::string_view::npos;
		}
	}

	VirtualPath::VirtualPath() = default;

	std::optional<VirtualPath> VirtualPath::Parse(
		const std::string_view utf8Path
	) {
		if (utf8Path.empty()) {
			return std::nullopt;
		}

		std::string normalizedInput;
		normalizedInput.reserve(utf8Path.size());

		for (const char character : utf8Path) {
			// ヌル文字は無効
			if (character == '\0') {
				return std::nullopt;
			}

			// UTF-8の範囲外の文字は無効
			if (IsInvalidAsciiCharacter(character)) {
				return std::nullopt;
			}

			// 絶対パスは無効
			if (character == '\\') {
				normalizedInput.push_back('/');
				continue;
			}

			// 文字を追加
			normalizedInput.push_back(character);
		}

		// 文字が空!?
		if (normalizedInput.empty()) {
			return std::nullopt;
		}

		// 先頭が '/' で始まる場合は無効 (絶対パスとかサーバーパスとか)
		if (normalizedInput.front() == '/') {
			return std::nullopt;
		}

		std::string normalizedPath;
		normalizedPath.reserve(normalizedInput.size());

		size_t currentPosition = 0;

		while (currentPosition < normalizedInput.size()) {
			// 連続した区切り文字は1つにする。(例: "content//models" -> "content/models")
			while (
				currentPosition < normalizedInput.size() &&
				normalizedInput[currentPosition] == '/'
			) {
				++currentPosition;
			}

			if (currentPosition >= normalizedInput.size()) {
				break;
			}

			const size_t separatorPosition = normalizedInput.find(
				'/', currentPosition);

			size_t segmentEnd = separatorPosition;
			if (separatorPosition == std::string::npos) {
				segmentEnd = normalizedInput.size();
			}

			const std::string_view segment(
				normalizedInput.data() + currentPosition,
				segmentEnd - currentPosition
			);

			if (segment == "..") {
				return std::nullopt;
			}

			if (segment != ".") {
				if (!normalizedPath.empty()) {
					normalizedPath.push_back('/');
				}
				normalizedPath.append(segment);
			}

			if (separatorPosition == std::string::npos) {
				break;
			}

			currentPosition = separatorPosition + 1;
		}

		if (normalizedPath.empty()) {
			return std::nullopt;
		}

		return VirtualPath(std::move(normalizedPath));
	}

	std::optional<VirtualPath> VirtualPath::ParseContentReference(
		const std::string_view utf8Path
	) {
		std::string normalizedSeparators(utf8Path);
		std::ranges::replace(normalizedSeparators, '\\', '/');
		if (
			normalizedSeparators.starts_with("./") ||
			normalizedSeparators.starts_with("../") ||
			normalizedSeparators == "content" ||
			normalizedSeparators.starts_with("content/")
		) {
			return std::nullopt;
		}

		return Parse(utf8Path);
	}

	VirtualPath VirtualPath::ParseOrThrow(const std::string_view utf8Path) {
		const std::optional<VirtualPath> result = Parse(utf8Path);

		if (!result.has_value()) {
			throw std::invalid_argument(
				"Invalid virtual path: " + std::string(utf8Path)
			);
		}

		return *result;
	}

	const std::string& VirtualPath::String() const noexcept {
		return mPath;
	}

	bool VirtualPath::IsEmpty() const noexcept {
		return mPath.empty();
	}

	VirtualPath::VirtualPath(std::string normalizedPath)
		: mPath(std::move(normalizedPath)) {
	}
}
