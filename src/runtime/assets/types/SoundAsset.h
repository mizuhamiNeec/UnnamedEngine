#pragma once
#include <string>
#include <vector>

namespace Unnamed {
	struct SoundAssetData {
		std::vector<uint8_t> bytes;
		uint32_t             sampleRate = 0;
		uint16_t             channels   = 0;
		std::string          sourcePath;
	};
}
