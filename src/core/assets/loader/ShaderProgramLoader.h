#pragma once
#include "interface/IAssetLoader.h"

namespace Unnamed {
	class AssetManager;

	/// @brief ShaderProgramLoaderは、shader stage、entry point、defineを検証してcompile要求へ変換します
	class ShaderProgramLoader : public IAssetLoader {
	public:
		explicit ShaderProgramLoader(AssetManager* assetManager);

		bool CanLoad(const Path& path, ASSET_TYPE* outType) const override;
		LoadResult Load(const Path& path) override;
		/// @brief 解決元mountを引き継いでShaderProgramをロードします。
		/// @param path 解決済みShaderProgram物理パス。
		/// @param context AssetManagerから渡されるロード元情報。
		/// @return ShaderProgramのロード結果。
		LoadResult Load(
			const Path& path, const AssetLoadContext& context
		) override;

	private:
		AssetManager* mAssetManager = nullptr;
	};
}
