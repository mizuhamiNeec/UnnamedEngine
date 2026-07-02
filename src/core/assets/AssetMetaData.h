#pragma once
#include "core/filesystem/Path.h"
#include "AssetType.h"
#include "FileStamp.h"

namespace Unnamed {
	/// @brief アセットのメタデータ
	struct AssetMetaData {
		ASSET_TYPE  type = ASSET_TYPE::UNKNOWN;
		std::string name;           // 表示名
		Path        sourcePath;     // アセットのソースファイルパス
		FileStamp   fileStamp;      // ファイル監視に使用
		uint32_t    strongRefs = 0; // 外部からの参照
		uint32_t    version    = 0; // アセットのバージョン
		bool        loaded     = false;
		bool        loadFailed = false; // 最後のファイルロードが失敗したか
		bool        runtime    = false; // CreateRuntimeAssetで生成されたアセット
		bool        destroyed  = false; // 明示破棄済みのruntimeアセット
	};
}
