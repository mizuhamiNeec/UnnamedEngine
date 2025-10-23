#pragma once
#include "interface/IAssetLoader.h"

namespace Unnamed {
	/// @brief DirectXTexを使用したテクスチャローダークラス
	class DirectXTexTextureLoader : public IAssetLoader {
	public:
		bool CanLoad(
			std::string_view path,
			UASSET_TYPE*     outType
		) const override;
		LoadResult Load(const std::string& path) override;
	};
}
