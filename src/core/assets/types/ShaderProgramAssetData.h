#pragma once
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/assets/AssetID.h"
#include "core/filesystem/VirtualPath.h"

namespace Unnamed {
	/// @brief シェーダープログラムのステージごとの情報を表す構造体
	struct ShaderProgramStage {
		VirtualPath                                      sourcePath;
		AssetID shaderSourceAssetId = kInvalidAssetID;
		std::string                                      entry;
		std::string                                      profile;
		std::vector<std::pair<std::string, std::string>> defines;
	};

	/// @brief シェーダープログラムアセットのデータ構造体
	struct ShaderProgramAssetData {
		std::string                       name;
		std::optional<ShaderProgramStage> vs;
		std::optional<ShaderProgramStage> ps;
		std::optional<ShaderProgramStage> cs;
		std::vector<std::string>          includeDirectories;
	};
}
