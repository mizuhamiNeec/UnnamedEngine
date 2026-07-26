#pragma once

#include <bit>
#include <cstdint>
#include <fstream>
#include <string_view>
#include <vector>

#include "core/filesystem/Path.h"

namespace Unnamed {
	/// @brief BinaryWriterは、バイナリ入出力の型付き値を決められたバイナリ形式へ書き出します
	class BinaryWriter {
	public:
		explicit BinaryWriter(const Path& path);

		/// @brief ファイルが正常に開けているか
		/// @return ファイルが正常に開けているならtrue、開けなかった場合はfalse
		[[nodiscard]] bool IsOpen() const;

		/// @brief ストリームが正常な状態か
		/// @return ストリームが正常な状態ならtrue、エラーが発生している場合はfalse
		[[nodiscard]] bool Good() const;

		/// @brief 現在の書き込み位置を取得する
		/// @return 書き込み位置のオフセット（バイト単位）
		[[nodiscard]] uint64_t Tell();

		/// @brief 書き込み位置を移動する
		/// @param offset 移動先のオフセット（バイト単位）
		/// @return 書き込み位置の移動に成功したか
		bool Seek(uint64_t offset);

		/// @brief バイト列を書き込む
		/// @param data 書き込むデータの先頭ポインタ
		/// @param sizeBytes 書き込むデータのサイズ（バイト単位）
		/// @return 書き込みに成功したか
		bool WriteBytes(const void* data, size_t sizeBytes);

		/// @brief 標準レイアウトな値をバイト列として書き込む。
		/// @tparam T 書き込む値の型。標準レイアウトである必要がある。
		/// @param value 書き込む値
		/// @return 書き込みに成功したか
		template <typename T>
		bool WritePod(const T& value) {
			static_assert(std::is_standard_layout_v<T>);
			static_assert(std::endian::native == std::endian::little);
			return WriteBytes(&value, sizeof(T));
		}

		/// @brief 文字列を書き込む。文字列の長さはuint32_tで表現できる必要がある。
		/// @param text 書き込む文字列
		/// @return 書き込みに成功したか
		bool WriteString(std::string_view text);

		/// @brief 標準レイアウトな配列を連続バイト列として書き込む。
		/// @tparam T 書き込む要素型。標準レイアウトである必要がある。
		/// @param values 要素先頭ポインタ
		/// @param count 要素数
		/// @return 書き込みに成功したか
		template <typename T>
		bool WriteArray(const T* values, const size_t count) {
			static_assert(std::is_standard_layout_v<T>);
			return WriteBytes(values, sizeof(T) * count);
		}

		/// @brief 標準レイアウトなベクターを連続バイト列として書き込む。ベクターのサイズはuint32_tで表現できる必要がある。
		/// @tparam T 書き込む要素型。標準レイアウトである必要がある。
		/// @param values 書き込むベクター
		/// @return 書き込みに成功したか
		template <typename T>
		bool WriteVector(const std::vector<T>& values) {
			static_assert(std::is_standard_layout_v<T>);
			if (values.size() > static_cast<size_t>(std::numeric_limits<
				    uint32_t>::max())) {
				return false;
			}
			const uint32_t count = static_cast<uint32_t>(values.size());
			return WritePod(count) &&
			       WriteArray(values.data(), values.size());
		}

		/// @brief ストリームをフラッシュする
		/// @return フラッシュ後のストリームの状態が正常ならtrue、エラーが発生している場合はfalse
		bool Flush();

	private:
		std::ofstream mStream;
	};
}
