#pragma once

#include <cstdint>

namespace Unnamed {
	/// @brief アセット内部参照の検証方針です。
	enum class AssetReferenceValidationPolicy : uint8_t {
		Permissive,
		Strict,
	};

	/// @brief 参照先を必須として検証するかを返します。
	[[nodiscard]] constexpr bool IsStrictAssetValidation(
		const AssetReferenceValidationPolicy policy
	) noexcept {
		return policy == AssetReferenceValidationPolicy::Strict;
	}
}
