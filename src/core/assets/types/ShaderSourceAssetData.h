#pragma once
#include <string>
#include <vector>

#include "core/filesystem/Path.h"

namespace Unnamed {
	/// @brief シェーダーソースアセットのデータ構造体
	struct ShaderSourceAssetData {
		Path                     path;
		std::vector<std::string> includePaths;
	};
}
