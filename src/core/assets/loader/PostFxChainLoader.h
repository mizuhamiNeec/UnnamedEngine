#pragma once
#include "interface/IAssetLoader.h"

namespace Unnamed {
	class AssetManager;

	class PostFxChainLoader : public IAssetLoader {
	public:
		explicit PostFxChainLoader(AssetManager* assetManager);

		bool CanLoad(const Path& path, ASSET_TYPE* outType) const override;
		LoadResult Load(const Path& path) override;
		/// @brief 親PostFxChainのmountを引き継いでロードします。
		/// @param path 解決済みPostFxChain物理パス。
		/// @param context AssetManagerから渡されるロード元情報。
		/// @return PostFxChainのロード結果。
		LoadResult Load(
			const Path& path, const AssetLoadContext& context
		) override;

	private:
		AssetManager* mAssetManager = nullptr;
	};
}
