#pragma once
#include <variant>
#include <vector>

#include "AssetID.h"
#include "FileStamp.h"

#include "types/EditorGuiData.h"
#include "types/EventPresentationAssetData.h"
#include "types/MaterialAssetData.h"
#include "types/MaterialInstanceAssetData.h"
#include "types/MeshAssetData.h"
#include "types/PostFxChainAssetData.h"
#include "types/SequenceAssetData.h"
#include "types/ShaderProgramAssetData.h"
#include "types/ShaderSourceAssetData.h"
#include "types/SoundAssetData.h"
#include "types/TextureAssetData.h"
#include "types/UiDocumentAssetData.h"

namespace Unnamed {
	//-------------------------------------------------------------------------
	// variant: 複数の型の一つを突っ込めるもの。 スッゲェェエ!
	// monostate: 空の状態
	//-------------------------------------------------------------------------

	/// @brief アセットのペイロードデータ
	using AssetPayload = std::variant<
		std::monostate,
		TextureAssetData,
		ShaderSourceAssetData,
		MeshAssetData,
		ShaderProgramAssetData,
		MaterialAssetData,
		MaterialInstanceAssetData,
		PostFxChainAssetData,
		SequenceAssetData,
		SoundAssetData,
		UiDocumentAssetData,
		EventPresentationAssetData,
		EditorGuiData
	>;

	/// @brief アセットのロード結果を表す構造体
	struct LoadResult {
		AssetPayload         payload;      // アセットのペイロードデータ
		std::vector<AssetID> dependencies; // パースして見つかった依存
		FileStamp            stamp;        // ファイルのタイムスタンプ
		std::string          resolveName;  // 表示名
		std::vector<Path>    sourceWatchPaths; // missing依存の再試行用監視パス
		std::vector<UnresolvedShaderInclude> unresolvedShaderIncludes;
	};
}
