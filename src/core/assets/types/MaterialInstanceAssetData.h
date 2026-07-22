#pragma once
#include <string>
#include <unordered_map>

#include "core/assets/AssetID.h"
#include "core/filesystem/VirtualPath.h"
#include "core/math/Vec4.h"

namespace Unnamed {
	/// @brief Material Instanceのテクスチャオーバーライドです。
	struct MatTextureOverride final {
		VirtualPath assetPath;
		AssetID     assetId = kInvalidAssetID;
	};

	/// @brief マテリアルインスタンスアセットのデータ構造体
	struct MaterialInstanceAssetData {
		std::string name;

		AssetID     materialId = kInvalidAssetID;
		VirtualPath materialPath;

		std::unordered_map<std::string, MatTextureOverride> textureOverrides;
		std::unordered_map<std::string, float>              scalarOverrides;
		std::unordered_map<std::string, Vec4>               vectorOverrides;
	};
}
