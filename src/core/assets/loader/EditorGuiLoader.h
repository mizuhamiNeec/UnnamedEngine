#pragma once
#include "interface/IAssetLoader.h"

namespace Unnamed {
	/// @brief EditorGuiLoaderは、Lua editor GUI sourceを読み込み、実行用EditorGuiDataへ変換します
	class EditorGuiLoader : public IAssetLoader {
	public:
		bool CanLoad(const Path& path, ASSET_TYPE* outType) const override;
		LoadResult Load(const Path& path) override;
	};
}
