#pragma once
#include <runtime/assets/loaders/interface/IAssetLoader.h>
#include <runtime/assets/core/UAssetManager.h>

namespace Unnamed {
	class ShaderLoader : public IAssetLoader {
	public:
		explicit ShaderLoader(UAssetManager* assetManager)
			: mAssetManager(assetManager) {
		}

		bool CanLoad(
			std::string_view path,
			UASSET_TYPE*     outType
		) const override;
		LoadResult Load(const std::string& path) override;

	private:
		static std::string GuessStageKeyFromFilename(const std::string& path);
		static std::vector<std::string> ExtractIncludes(const std::string& src);

	private:
		UAssetManager* mAssetManager;
	};
}
