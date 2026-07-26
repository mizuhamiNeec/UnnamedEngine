#pragma once

namespace Unnamed {
	inline constexpr uint64_t kFnv1a64OffsetBasis = 14695981039346656037ull;
	inline constexpr uint64_t kFnv1a64Prime       = 1099511628211ull;

	[[nodiscard]] constexpr uint64_t Fnv1a64(
		const std::string_view value
	) noexcept {
		uint64_t hash = kFnv1a64OffsetBasis;

		for (const char character : value) {
			hash ^= static_cast<uint64_t>(character);
			hash *= kFnv1a64Prime;
		}

		return hash;
	}

	[[nodiscard]]
	constexpr uint64_t Combine64(
		const uint64_t seed,
		const uint64_t value
	) noexcept {
		constexpr uint64_t mixConstant = 0x9e3779b97f4a7c15ull;

		// まじぇまじぇ
		return seed ^ value + mixConstant + (seed << 6) + (seed >> 2);
	}

	/// @brief 順序付き入力から実行間で安定したハッシュ値を構築します
	class StableHashBuilder final {
	public:
		explicit constexpr
		StableHashBuilder(const uint64_t seed = 0) noexcept : mSeed(seed) {
		}

		/// @brief 既にハッシュ化済みの値を現在のシードに混ぜる。
		/// @param value 既にハッシュ化済みの値
		constexpr void AddHash(const uint64_t value) noexcept {
			mSeed = Combine64(mSeed, value);
		}

		/// @brief 文字列をFNV-1aでハッシュ化して現在のシードに混ぜる。
		/// @param value ハッシュ化対象の文字列ビュー
		constexpr void AddString(const std::string_view value) noexcept {
			AddHash(Fnv1a64(value));
		}

		/// @brief 64ビット符号なし整数を現在のシードに混ぜる。
		/// @param value ハッシュ化対象の64ビット符号なし整数
		constexpr void AddUInt64(const uint64_t value) noexcept {
			AddHash(value);
		}

		/// @brief 64ビット符号付き整数を現在のシードに混ぜる。
		/// @param value ハッシュ化対象の64ビット符号付き整数
		constexpr void AddInt64(const int64_t value) noexcept {
			AddHash(static_cast<uint64_t>(value));
		}

		/// @brief ブール値を現在のシードに混ぜる。trueは1、falseは0として扱う。
		/// @param value ハッシュ化対象のブール値
		constexpr void AddBool(const bool value) noexcept {
			AddHash(value ? 1ull : 0ull);
		}

		/// @brief 列挙値を基底型に変換して現在のシードに混ぜる。
		/// @tparam T 列挙型
		/// @param value ハッシュ化対象の列挙値
		template <typename T>
		constexpr void AddEnum(const T value) noexcept {
			static_assert(std::is_enum_v<T>);

			using UnderlyingType = std::underlying_type_t<T>;
			using UnsignedType   = std::make_unsigned_t<UnderlyingType>;

			AddHash(static_cast<uint64_t>(static_cast<UnsignedType>(value)));
		}

		/// @brief 現在のハッシュ値を返す。
		/// @return 現在のハッシュ値
		[[nodiscard]] constexpr uint64_t Value() const noexcept {
			return mSeed;
		}

	private:
		uint64_t mSeed = 0;
	};
}
