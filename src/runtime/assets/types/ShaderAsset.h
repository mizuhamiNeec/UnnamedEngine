#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace Unnamed {
	/// @brief シェーダーアセットデータ構造体
	struct ShaderAssetData {
		std::string                                           hlsl;
		std::unordered_map<std::string, std::vector<uint8_t>> blobs;
		std::string                                           sourcePath;
	};
}
