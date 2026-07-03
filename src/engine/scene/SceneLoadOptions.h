#pragma once

#include <cstdint>
#include <string_view>

#include "engine/content/AssetReferenceValidationPolicy.h"

namespace Unnamed {
	class AssetManager;

	/// @brief シーン内アセット参照の検証方針です。
	enum class SceneAssetValidationPolicy : uint8_t {
		Permissive,
		Strict,
	};

	/// @brief シーン読込時の動作を指定します。
	struct SceneLoadOptions final {
		SceneAssetValidationPolicy assetValidationPolicy =
			SceneAssetValidationPolicy::Permissive;
	};

	/// @brief コンポーネントのシーンデシリアライズに必要な文脈です。
	struct SceneDeserializeContext final {
		const SceneLoadOptions& loadOptions;
		AssetManager*           assetManager = nullptr;
		const Path&             scenePath;
		std::string_view        entityName;
		uint64_t                entityId = 0;
		std::string_view        componentType;
	};

	/// @brief strict アセット検証が要求されているかを返します。
	[[nodiscard]] constexpr bool IsStrictAssetValidation(
		const SceneLoadOptions& options
	) noexcept {
		return options.assetValidationPolicy ==
			SceneAssetValidationPolicy::Strict;
	}

	/// @brief シーン検証方針を汎用アセット参照検証方針へ変換します。
	[[nodiscard]] constexpr AssetReferenceValidationPolicy
	ToAssetReferenceValidationPolicy(
		const SceneAssetValidationPolicy policy
	) noexcept {
		return policy == SceneAssetValidationPolicy::Strict ?
			AssetReferenceValidationPolicy::Strict :
			AssetReferenceValidationPolicy::Permissive;
	}
}
