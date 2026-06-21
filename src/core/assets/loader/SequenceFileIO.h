#pragma once

#include <cstdint>

#include "core/assets/types/SequenceAssetData.h"
#include "core/assets/types/SequenceAuthoringData.h"

namespace Unnamed {
	/// @brief Sequenceファイル読み込み結果です。
	struct SequenceFileLoadResult final {
		SequenceAuthoringData authoring     = {};
		SequenceAssetData     runtime       = {};
		bool                  migrated      = false;
		int32_t               sourceVersion = 1;
		uint64_t              semanticHash  = 0;
	};

	/// @brief Sequence編集データの保存/読み込みを行うIOユーティリティです。
	class SequenceFileIO final {
	public:
		/// @brief `.sequence.json` を読み込み、編集データとランタイムデータを構築します。
		static bool LoadFromFile(
			const Path&             path,
			SequenceFileLoadResult& outResult
		);

		/// @brief 編集データを `.sequence.json` として保存します。
		/// @param outWritten 内容が変化して実際に書き戻した場合はtrue
		static bool SaveToFile(
			const Path&                  path,
			const SequenceAuthoringData& authoring,
			bool*                        outWritten = nullptr
		);

		/// @brief 編集データからランタイム評価用データを構築します。
		static SequenceAssetData BuildRuntimeData(
			const SequenceAuthoringData& authoring
		);
	};
}
