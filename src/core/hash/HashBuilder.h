#pragma once

#include <cstdint>
#include <functional>
#include <type_traits>

namespace Unnamed {
	/// @brief ランタイム用の汎用ハッシュ生成クラス
	/// @details std::hashやポインタの値を組み合わせる。
	/// 生成値は他環境で一致するとは限らない
	class HashBuilder final {
	public:
		/// @brief デフォルトシード（0）でハッシュ生成器を初期化する。
		constexpr HashBuilder() noexcept = default;

		/// @brief シード値を指定してハッシュ生成器を初期化する。
		/// @param seed 初期シード値
		explicit constexpr HashBuilder(const uint64_t seed) noexcept
			: mSeed(seed) {
		}

		/// @brief `std::hash` で値をハッシュ化して現在のシードに混ぜる。
		/// @tparam T ハッシュ化対象の型
		/// @param value ハッシュ化対象の値
		template <typename T>
		void AddValue(const T& value) {
			AddHashed(std::hash<T>{}(value));
		}

		/// @brief 列挙値を基底型に変換して現在のシードに混ぜる。
		/// @tparam T 列挙型
		/// @param value ハッシュ化対象の列挙値
		template <typename T>
		void AddEnum(const T value) {
			static_assert(std::is_enum_v<T>);

			using UnderlyingType = std::underlying_type_t<T>;
			AddValue(static_cast<UnderlyingType>(value));
		}

		/// @brief ポインタ値を現在のシードに混ぜる。
		/// @tparam T ポインタの指す型
		/// @param value ハッシュ化対象のポインタ
		template <typename T>
		void AddPointer(const T* value) noexcept {
			AddHashed(reinterpret_cast<uint64_t>(value));
		}

		/// @brief 既にハッシュ化済みの値を現在のシードに混ぜる。
		/// @param value 既にハッシュ化済みの値
		void AddHashed(const uint64_t value) noexcept {
			mSeed = Combine(mSeed, value);
		}

		/// @brief 現在のハッシュ値を返す。
		/// @return 現在のハッシュ値
		[[nodiscard]]
		constexpr uint64_t Value() const noexcept {
			return mSeed;
		}

		/// @brief 2つの `uint64_t` ハッシュ値を組み合わせる。
		/// @param seed ベースとなるハッシュ値
		/// @param value 混ぜるハッシュ値
		/// @return 組み合わせ結果
		[[nodiscard]] static constexpr uint64_t Combine(
			const uint64_t seed, const uint64_t value
		) noexcept {
			return seed ^ value + kMixConstant + (seed << 6) + (seed >> 2);
		}

	private:
		static constexpr uint64_t kMixConstant = 0x9e3779b97f4a7c15ull;

		uint64_t mSeed = 0;
	};
}
