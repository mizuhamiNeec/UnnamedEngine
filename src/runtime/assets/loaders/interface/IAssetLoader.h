#pragma once
#include <string_view>

#include <runtime/assets/core/UAsset.h>

namespace Unnamed {
	class IAssetLoader {
	public:
		virtual      ~IAssetLoader() = default;
		virtual bool CanLoad(
			std::string_view path, UASSET_TYPE* outType
		) const = 0;
		virtual LoadResult Load(const std::string& path) = 0;
	};
}
