#pragma once
#include <string_view>

#include "core/assets/AssetType.h"
#include "core/assets/LoadResult.h"

namespace Unnamed {
	/// @brief AssetManagerからLoaderへ渡すロード元情報です。
	struct AssetLoadContext final {
		/// @brief マウント経由で解決された場合のmount IDです。
		std::string_view resolvedMountId;
	};

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

		/// @brief ロード元情報を伴ってアセットを読み込みます。
		/// @param path 解決済み物理ファイルパス。
		/// @param context ロード元情報。
		/// @return アセットのロード結果。
		virtual LoadResult Load(
			const Path& path, const AssetLoadContext&
		) {
			return Load(path);
		}
	};
}
