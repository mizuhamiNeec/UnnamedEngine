#pragma once
#include <string>
#include <unordered_map>
#include <vector>

#include "core/assets/AssetID.h"
#include "core/filesystem/VirtualPath.h"
#include "core/math/Vec4.h"

namespace Unnamed {
	/// @brief ポストエフェクトパスのデータ構造体
	struct PostFxPassAssetData {
		std::string name;
		bool        enabled = true;

		AssetID shaderProgramId = kInvalidAssetID;
		VirtualPath shaderProgramPath;

		std::unordered_map<std::string, float> scalarParams;
		std::unordered_map<std::string, Vec4>  colorParams;
	};

	/// @brief ポストエフェクトチェーンアセットのデータ構造体
	struct PostFxChainAssetData {
		std::string                      name;
		std::vector<PostFxPassAssetData> passes;
	};
}
