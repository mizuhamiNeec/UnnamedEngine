#pragma once
#include <chrono>
#include <cstdint>
#include <string>
#include <variant>

#include <runtime/assets/core/UAssetID.h>
#include <runtime/assets/types/MaterialAsset.h>
#include <runtime/assets/types/MeshAsset.h>
#include <runtime/assets/types/RawFileAsset.h>
#include <runtime/assets/types/ShaderAsset.h>
#include <runtime/assets/types/SoundAsset.h>
#include <runtime/assets/types/TextureAsset.h>

namespace Unnamed {
	struct FileStamp {
		std::chrono::system_clock::time_point lastWrite   = {};
		uint64_t                              sizeInBytes = 0;
	};

	enum class UASSET_TYPE : uint8_t {
		UNKNOWN  = 1 << 0,
		TEXTURE  = 1 << 1,
		SHADER   = 1 << 2,
		MATERIAL = 1 << 3,
		MESH     = 1 << 4,
		SOUND    = 1 << 5,
		RAWFILE  = 1 << 6,
	};

	const char* ToString(UASSET_TYPE e);

	//-------------------------------------------------------------------------
	// variant: 複数の型の一つを突っ込めるもの
	// monostate: 空の状態
	//-------------------------------------------------------------------------

	using AssetPayload = std::variant<
		std::monostate,
		TextureAssetData,
		ShaderAssetData,
		MaterialAssetData,
		MeshAssetData,
		SoundAssetData,
		RawFileAssetData
	>;

	struct AssetMetaData {
		UASSET_TYPE type = UASSET_TYPE::UNKNOWN;
		std::string name;           // 表示名
		std::string sourcePath;     // アセットのソースファイルパス
		FileStamp   fileStamp;      // ファイル監視に使用
		uint32_t    strongRefs = 0; // 外部からの参照
		uint32_t    version    = 0; // アセットのバージョン
		bool        loaded     = false;
	};

	struct LoadResult {
		AssetPayload         payload;
		std::vector<AssetID> dependencies; // パースして見つかった依存
		FileStamp            stamp;        // ファイルのタイムスタンプ
		std::string          resolveName;  // 表示名
	};
}
