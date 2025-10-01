#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>

#include "UAssetID.h"

namespace Unnamed {
	enum class ParamType : uint8_t {
		FLOAT,
		FLOAT4
	};

	struct MaterialParam {
		ParamType            type = ParamType::FLOAT4;
		std::array<float, 4> v    = {0, 0, 0, 0};
		int                  slot = -1;
	};

	struct MaterialAssetData {
		std::unordered_map<std::string, float>                floatParams;
		std::unordered_map<std::string, std::array<float, 4>> float4Params;
		std::unordered_map<std::string, AssetID>              textureSlots;

		std::unordered_map<std::string, MaterialParam> params;

		AssetID shader = kInvalidAssetID;

		AssetID shaderVS = kInvalidAssetID;
		AssetID shaderPS = kInvalidAssetID;
		AssetID shaderGS = kInvalidAssetID;
		AssetID shaderCS = kInvalidAssetID;

		std::string entryVS = "VSMain";
		std::string entryPS = "PSMain";
		std::string entryGS = "GSMain";
		std::string entryCS = "CSMain";

		AssetID programBody;
		AssetID programMeta;

		std::vector<std::string> defines;

		std::string sourcePath;
	};
}
