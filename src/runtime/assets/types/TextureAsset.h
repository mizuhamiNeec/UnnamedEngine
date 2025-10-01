#pragma once
#include <string>
#include <vector>

namespace Unnamed {
	struct TextureMip {
		std::vector<uint8_t> bytes;
		uint32_t             width    = 0;
		uint32_t             height   = 0;
		size_t               rowPitch = 0;
	};

	struct TextureAssetData {
		std::vector<uint8_t>    bytes;
		std::vector<TextureMip> mips; // [0]の解像度が高い
		uint32_t                width  = 0;
		uint32_t                height = 0;
		bool                    isSRGB = false;
		std::string             sourcePath;
	};
}
