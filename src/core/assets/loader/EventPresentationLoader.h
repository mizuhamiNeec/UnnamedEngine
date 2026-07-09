#pragma once

#include "interface/IAssetLoader.h"

namespace Unnamed {
	/// @brief Event Presentation v2 アセットを読み込むローダーです。
	class EventPresentationLoader final : public IAssetLoader {
	public:
		/// @brief 指定パスが Event Presentation v2 として読み込み可能か判定します。
		/// @param path 判定対象パス
		/// @param outType 判定結果のアセット種別出力先
		/// @return 読み込み可能なら true
		bool CanLoad(const Path& path, ASSET_TYPE* outType) const override;

		/// @brief Event Presentation v2 アセットをロードします。
		/// @param path 読み込み対象パス
		/// @return ロード結果
		LoadResult Load(const Path& path) override;
	};
}
