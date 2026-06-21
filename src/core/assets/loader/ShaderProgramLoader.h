#pragma once
#include "interface/IAssetLoader.h"

namespace Unnamed {
	class AssetManager;

	class ShaderProgramLoader : public IAssetLoader {
	public:
		explicit ShaderProgramLoader(AssetManager* assetManager);

		bool CanLoad(const Path& path, ASSET_TYPE* outType) const override;
		LoadResult Load(const Path& path) override;

	private:
		AssetManager* mAssetManager = nullptr;
	};
}
