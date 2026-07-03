#pragma once

#include <cstdint>
#include <string_view>

#include "engine/content/AssetReferenceValidationPolicy.h"

namespace Unnamed {
	class AssetManager;

	/// @brief シーン内アセット参照の検証方針です。
	enum class SCENE_ASSET_VALIDATION_POLICY : uint8_t {
		PERMISSIVE,
		STRICT,
	};

	/// @brief シーン読込時の動作を指定します。
	struct SceneLoadOptions final {
		SCENE_ASSET_VALIDATION_POLICY assetValidationPolicy =
			SCENE_ASSET_VALIDATION_POLICY::PERMISSIVE;
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
			SCENE_ASSET_VALIDATION_POLICY::STRICT;
	}

	/// @brief シーン検証方針を汎用アセット参照検証方針へ変換します。
	[[nodiscard]] constexpr AssetReferenceValidationPolicy
	ToAssetReferenceValidationPolicy(
		const SCENE_ASSET_VALIDATION_POLICY policy
	) noexcept {
		return policy == SCENE_ASSET_VALIDATION_POLICY::STRICT ?
			AssetReferenceValidationPolicy::Strict :
			AssetReferenceValidationPolicy::Permissive;
	}
}
