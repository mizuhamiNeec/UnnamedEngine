#pragma once
#include "interface/IAssetLoader.h"

namespace Unnamed {
	class EditorGuiLoader : public IAssetLoader {
	public:
		bool CanLoad(const Path& path, ASSET_TYPE* outType) const override;
		LoadResult Load(const Path& path) override;
	};
}
