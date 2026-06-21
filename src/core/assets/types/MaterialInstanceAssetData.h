#pragma once
#include <string>
#include <unordered_map>

#include "core/assets/AssetID.h"
#include "core/filesystem/Path.h"
#include "core/math/Vec4.h"

namespace Unnamed {
	/// @brief マテリアルインスタンスアセットのデータ構造体
	struct MaterialInstanceAssetData {
		std::string name;

		AssetID     materialId = kInvalidAssetID;
		Path        materialPath;

		std::unordered_map<std::string, Path>        textureOverrides;
		std::unordered_map<std::string, float>       scalarOverrides;
		std::unordered_map<std::string, Vec4>        vectorOverrides;
	};
}
