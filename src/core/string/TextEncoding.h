#pragma once

namespace Unnamed {
	class TextEncoding final {
	public:
		TextEncoding() = delete;

		/// @brief UTF-8文字列をUTF-16文字列へ変換します。
		/// @param utf8 変換するUTF-8文字列
		/// @return 変換されたUTF-16文字列
		/// @throws std::system_error 文字列が不正、または変換に失敗した場合
		[[nodiscard]] static std::wstring Utf8ToWide(std::string_view utf8);

		/// @brief UTF-16文字列をUTF-8文字列へ変換します。
		/// @param wide 変換するUTF-16文字列
		/// @return 変換されたUTF-8文字列
		/// @throws std::system_error 文字列が不正、または変換に失敗した場合
		[[nodiscard]] static std::string WideToUtf8(std::wstring_view wide);
	};
}
