#pragma once

#include <bit>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace Unnamed {
	class BinaryReader {
	public:
		/// @brief 指定したパスのファイルをバイナリモードで開く。
		/// @param path ファイルパス
		explicit BinaryReader(const Path& path);

		/// @brief ファイルが正常に開けているか
		/// @return ファイルが開けているか
		[[nodiscard]] bool IsOpen() const;

		/// @brief ストリームが正常な状態か
		/// @return ストリームが正常な状態か
		[[nodiscard]] bool Good() const;

		/// @brief 現在の読み取り位置を取得する。
		/// @return 現在の読み取り位置（バイト単位）
		[[nodiscard]] uint64_t Tell();

		/// @brief 読み取り位置をファイルの先頭からのオフセットで移動する。
		/// @param offset ファイルの先頭からのオフセット（バイト単位）
		/// @return 成功したか
		bool Seek(uint64_t offset);

		/// @brief 読み取り位置を現在の位置からのオフセットで移動する。
		/// @param bytes 現在の位置からのオフセット（バイト単位）
		/// @return 成功したか
		bool Skip(uint64_t bytes);

		/// @brief 現在の読み取り位置からファイル末尾までの残りバイト数を取得する。
		/// @return 残りバイト数。読み取り位置がファイルサイズを超えている場合は`0`
		[[nodiscard]] uint64_t Remaining();

		/// @brief 指定したバッファに生のバイト列を読み込む。
		/// @param outData 読み込んだデータの格納先バッファ
		/// @param sizeBytes 読み込むバイト数
		/// @return 読み込みに成功したか
		bool ReadBytes(void* outData, size_t sizeBytes);

		/// @brief 標準レイアウトな値を読み込む。
		/// @tparam T 読み込む値の型。標準レイアウトである必要がある。
		/// @param outValue 読み込んだ値の格納先
		/// @return 読み込みに成功したか
		template <typename T>
		bool ReadPod(T& outValue) {
			static_assert(std::is_standard_layout_v<T>);
			static_assert(std::endian::native == std::endian::little);
			return ReadBytes(&outValue, sizeof(T));
		}

		/// @brief 文字列を読み込む。文字列は、最初にuint32_tでバイト数が続き、その後にUTF-8エンコードされた文字列データが続く形式でエンコードされている必要がある。
		/// @param outText 読み込んだ文字列の格納先
		/// @return 読み込みに成功したか
		bool ReadString(std::string& outText);

		/// @brief 標準レイアウトな配列を連続バイト列として読み込む。
		/// @tparam T 読み込む要素型。標準レイアウトである必要がある。
		/// @param outValues 要素先頭ポインタ
		/// @param count 要素数
		/// @return 読み込みに成功したか
		template <typename T>
		bool ReadArray(T* outValues, const size_t count) {
			static_assert(std::is_standard_layout_v<T>);
			return ReadBytes(outValues, sizeof(T) * count);
		}

		/// @brief 標準レイアウトな値のベクターを読み込む。ベクターは、最初にuint32_tで要素数が続き、その後に要素が連続バイト列としてエンコードされている必要がある。
		/// @tparam T 読み込む要素型。標準レイアウトである必要がある。
		/// @param outValues 読み込んだ値の格納先ベクター
		/// @return 読み込みに成功したか
		template <typename T>
		bool ReadVector(std::vector<T>& outValues) {
			static_assert(std::is_standard_layout_v<T>);
			uint32_t count = 0;
			if (!ReadPod(count)) {
				return false;
			}
			if (sizeof(T) > 0 &&
			    Remaining() / sizeof(T) < static_cast<size_t>(count)) {
				return false;
			}
			outValues.resize(count);
			return ReadArray(outValues.data(), outValues.size());
		}

	private:
		std::ifstream mStream;
		uint64_t      mFileSize = 0;
	};
}
