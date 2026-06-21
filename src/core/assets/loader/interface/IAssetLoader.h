#pragma once
#include "core/assets/AssetType.h"
#include "core/assets/LoadResult.h"

namespace Unnamed {
	/// @brief アセットローダーインターフェース
	class IAssetLoader {
	public:
		virtual ~IAssetLoader() = default;

		/// @brief 指定されたパスのアセットをこのローダーが読み込めるか?
		virtual bool CanLoad(
			const Path& path, ASSET_TYPE* outType
		) const = 0;

		/// @brief 指定されたパスのアセットを読み込む
		virtual LoadResult Load(const Path& path) = 0;
	};
}
