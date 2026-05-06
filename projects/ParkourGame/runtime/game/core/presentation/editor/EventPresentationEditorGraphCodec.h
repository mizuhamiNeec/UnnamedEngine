#pragma once

#include <string>

#include "core/assets/types/EventPresentationAssetData.h"
#include "game/core/presentation/editor/EventPresentationEditorGraph.h"

namespace Unnamed {
	/// @brief EventPresentation アセットと編集グラフの相互変換を行うユーティリティです。
	class EventPresentationEditorGraphCodec final {
	public:
		/// @brief EventPresentation アセットを編集グラフへ展開します。
		/// @param assetData 変換元アセット
		/// @param outGraph 変換先グラフ
		/// @param outError 失敗理由の出力先
		/// @return 成功時 true
		[[nodiscard]] static bool BuildGraphFromAsset(
			const EventPresentationAssetData& assetData,
			EventPresentationEditorGraph&     outGraph,
			const std::string&                sourcePath = "",
			std::string*                      outError = nullptr
		);

		/// @brief 編集グラフを EventPresentation アセットへ逆変換します。
		/// @param graph 変換元グラフ
		/// @param outAssetData 変換先アセット
		/// @param outError 失敗理由の出力先
		/// @return 成功時 true
		[[nodiscard]] static bool BuildAssetFromGraph(
			const EventPresentationEditorGraph& graph,
			EventPresentationAssetData&          outAssetData,
			std::string*                         outError = nullptr
		);

		/// @brief EventPresentation アセットを JSON ファイルとして保存します。
		/// @param assetData 保存対象アセット
		/// @param path 保存先パス
		/// @param outError 失敗理由の出力先
		/// @return 成功時 true
		[[nodiscard]] static bool SaveAssetJson(
			const EventPresentationAssetData& assetData,
			const EventPresentationEditorGraph* editorGraph,
			const std::string&                path,
			std::string*                      outError = nullptr
		);
	};
}
