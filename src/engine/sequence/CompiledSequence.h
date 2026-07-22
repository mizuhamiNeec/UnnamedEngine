#pragma once

#include <memory>
#include <vector>

#include "core/assets/types/SequenceAssetData.h"

namespace Unnamed {
	/// @brief コンパイル済みセクション参照です。
	struct CompiledSequenceSection final {
		const SequenceSectionAssetData* source = nullptr;
	};

	/// @brief コンパイル済みトラック参照です。
	struct CompiledSequenceTrack final {
		const SequenceTrackAssetData*        source   = nullptr;
		std::vector<CompiledSequenceSection> sections = {};
	};

	/// @brief 実行時評価用に前処理したシーケンスです。
	class CompiledSequence final {
	public:
		/// @brief 元アセットからコンパイル済みデータを生成します。
		explicit CompiledSequence(
			std::shared_ptr<const SequenceAssetData> asset
		);

		/// @brief 元シーケンスアセットを取得します。
		[[nodiscard]] const std::shared_ptr<const SequenceAssetData>&
		GetAsset() const;

		/// @brief コンパイル済みトラック配列を取得します。
		[[nodiscard]] const std::vector<CompiledSequenceTrack>&
		GetTracks() const;

		/// @brief 総フレーム長を取得します。
		[[nodiscard]] int64_t GetLengthFrames() const;

	private:
		std::shared_ptr<const SequenceAssetData> mAsset        = nullptr;
		std::vector<CompiledSequenceTrack>       mTracks       = {};
		int64_t                                  mLengthFrames = 0;
	};
}
