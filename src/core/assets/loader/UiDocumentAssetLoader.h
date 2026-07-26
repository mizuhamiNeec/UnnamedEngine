#pragma once

#include "core/assets/loader/interface/IAssetLoader.h"

namespace Unnamed {
	/// @brief UiDocumentAssetLoaderは、UI JSONのnode階層、style、asset参照をUiDocumentDataへ変換します
	class UiDocumentAssetLoader final : public IAssetLoader {
	public:
		bool CanLoad(
			const Path& path, ASSET_TYPE* outType
		) const override;

		LoadResult Load(const Path& path) override;
	};
}
