#pragma once
#include "interface/IAssetLoader.h"

namespace Unnamed {
	class RawLoader : public IAssetLoader {
	public:
		bool CanLoad(
			std::string_view path,
			UASSET_TYPE*     outType
		) const override;
		LoadResult Load(const std::string& path) override;
	};
}
