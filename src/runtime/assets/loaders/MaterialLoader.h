#pragma once

#include <runtime/assets/loaders/interface/IAssetLoader.h>

namespace Unnamed {
	class UAssetManager;

	class MaterialLoader : public IAssetLoader {
	public:
		explicit MaterialLoader(UAssetManager* assetManager);

		bool CanLoad(
			std::string_view path, UASSET_TYPE* outType
		) const override;

		LoadResult Load(const std::string& path) override;

	private:
		UAssetManager* mAssetManager = nullptr;
	};
}
