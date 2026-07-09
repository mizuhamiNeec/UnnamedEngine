#pragma once
#include "interface/IAssetLoader.h"

namespace Unnamed {
	class AssetManager;

	class MaterialInstanceAssetLoader : public IAssetLoader {
	public:
		explicit MaterialInstanceAssetLoader(AssetManager* assetManager);

		bool CanLoad(const Path& path, ASSET_TYPE* outType) const override;
		LoadResult Load(const Path& path) override;

	private:
		AssetManager* mAssetManager = nullptr;
	};
}
