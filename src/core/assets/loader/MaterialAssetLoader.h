#pragma once
#include "interface/IAssetLoader.h"

namespace Unnamed {
	class AssetManager;

	/// @brief MaterialAssetLoaderは、material定義のshader、parameter、texture参照を検証してruntime materialへ変換します
	class MaterialAssetLoader : public IAssetLoader {
	public:
		explicit MaterialAssetLoader(AssetManager* assetManager);

		bool CanLoad(const Path& path, ASSET_TYPE* outType) const override;
		LoadResult Load(const Path& path) override;

	private:
		AssetManager* mAssetManager;
	};
}
