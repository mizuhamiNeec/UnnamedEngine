#pragma once

#include <runtime/assets/core/UAsset.h>
#include <runtime/assets/loaders/interface/IAssetLoader.h>

namespace Unnamed {
	class BasicBinaryLoader : public IAssetLoader {
	public:
		bool CanLoad(
			std::string_view path,
			UASSET_TYPE*     outType
		) const override;
		LoadResult Load(const std::string& path) override;
	};
}
