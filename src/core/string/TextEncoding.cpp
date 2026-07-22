#include "pch.h"
#include "TextEncoding.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace Unnamed {
	static constexpr std::string_view kChannel = "TxtEnc";

	namespace {
		/// @brief Win32 APIの文字列長の制限をチェックし、必要に応じて例外をスローします。
		/// @param length チェックする文字列の長さ
		/// @return Win32 APIで使用できる文字列長
		/// @throws std::length_error 文字列の長さがWin32 APIの制限を超えている場合
		[[nodiscard]] int ToWin32Length(const size_t length) {
			if (length > static_cast<size_t>(INT_MAX)) {
				// テキストの長さがWin32 APIの制限を超えています。
				throw std::length_error(
					"Text length exceeds the Win32 API limit.");
			}
			return static_cast<int>(length);
		}

#ifdef _WIN32
		/// @brief Win32 APIの変換エラーをスローします。
		/// @param operation 失敗した操作の説明
		/// @throws std::system_error Win32 APIのエラーコードを含む例外
		[[noreturn]] void ThrowWin32ConversionError(const char* operation) {
			const DWORD errorCode = GetLastError();

			throw std::system_error(
				static_cast<int>(errorCode),
				std::system_category(),
				operation
			);
		}
#endif
	}

	std::wstring TextEncoding::Utf8ToWide(const std::string_view utf8) {
		// 何も変換する文字がない場合は空の文字列を返す
		if (utf8.empty()) {
			Warning(
				kChannel,
				"Input UTF-8 string is empty. Returning an empty wide string."
			);
			return {};
		}

#ifdef _WIN32
		const int utf8Length = ToWin32Length(utf8.size());

		const int wideLength = MultiByteToWideChar(
			CP_UTF8,
			MB_ERR_INVALID_CHARS,
			utf8.data(),
			utf8Length,
			nullptr,
			0
		);

		// 変換に失敗した場合はエラーをスローする
		if (wideLength == 0) {
			ThrowWin32ConversionError(
				"MultiByteToWideChar size calculation failed"
			);
		}

		// 必要なバッファサイズを確保してから変換を実行する
		std::wstring wide(static_cast<size_t>(wideLength), L'\0');

		const int convertedLength = MultiByteToWideChar(
			CP_UTF8,
			MB_ERR_INVALID_CHARS,
			utf8.data(),
			utf8Length,
			wide.data(),
			wideLength
		);

		if (convertedLength == 0) {
			ThrowWin32ConversionError(
				"MultiByteToWideChar conversion failed"
			);
		}

		return wide;
#else
		// 今のところ Windowsのみ
		static_assert(
			sizeof(wchar_t) == 0,
			"TextEncoding::Utf8ToWide requires a platform implementation."
		);
#endif
	}

	std::string TextEncoding::WideToUtf8(const std::wstring_view wide) {
		// 何も変換する文字がない場合は空の文字列を返す
		if (wide.empty()) {
			Warning(
				kChannel,
				"Input wide string is empty. Returning an empty UTF-8 string."
			);
			return {};
		}

#ifdef _WIN32
		const int wideLength = ToWin32Length(wide.size());

		const int utf8Length = WideCharToMultiByte(
			CP_UTF8,
			WC_ERR_INVALID_CHARS,
			wide.data(),
			wideLength,
			nullptr,
			0,
			nullptr,
			nullptr
		);

		if (utf8Length == 0) {
			ThrowWin32ConversionError(
				"WideCharToMultiByte size calculation failed"
			);
		}

		std::string utf8(static_cast<size_t>(utf8Length), '\0');

		const int convertedLength = WideCharToMultiByte(
			CP_UTF8,
			WC_ERR_INVALID_CHARS,
			wide.data(),
			wideLength,
			utf8.data(),
			utf8Length,
			nullptr,
			nullptr
		);

		if (convertedLength == 0) {
			ThrowWin32ConversionError(
				"WideCharToMultiByte conversion failed"
			);
		}

		return utf8;
#else
		// 今のところ Windowsのみ
		static_assert(
			sizeof(wchar_t) == 0,
			"TextEncoding::WideToUtf8 requires a platform implementation."
		);
#endif
	}
}
