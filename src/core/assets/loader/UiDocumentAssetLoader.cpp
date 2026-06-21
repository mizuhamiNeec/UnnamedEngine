#include "UiDocumentAssetLoader.h"
#include "core/filesystem/Path.h"

#include <fstream>

#include "core/string/StrUtil.h"

namespace Unnamed {
	bool UiDocumentAssetLoader::CanLoad(
		const Path& path, ASSET_TYPE* outType
	) const {
		const bool ok = StrUtil::ToLowerCase(path.ToGenericUtf8()).ends_with(
			".ui.json"
		);
		if (outType) {
			*outType = ok ? ASSET_TYPE::UI_DOCUMENT : ASSET_TYPE::UNKNOWN;
		}
		return ok;
	}

	LoadResult UiDocumentAssetLoader::Load(const Path& path) {
		LoadResult    result = {};
		std::ifstream ifs(path.Native());
		if (!ifs) {
			return result;
		}

		nlohmann::json root;
		try {
			ifs >> root;
		} catch (...) {
			return result;
		}

		if (
			!root.is_object() ||
			!root.contains("version") ||
			!root["version"].is_number_integer() ||
			root["version"].get<int>() != 2 ||
			!root.contains("root")
		) {
			return result;
		}

		const Path          full = path.LexicallyNormal();
		UiDocumentAssetData data = {};
		data.name = root.value("name", Path::ToUtf8String(full.FileName()));
		data.rootJson = std::move(root);

		result.payload     = std::move(data);
		result.resolveName = Path::ToUtf8String(full.Stem().Stem());

		std::error_code ec;
		if (std::filesystem::exists(path.Native(), ec)) {
			result.stamp.sizeInBytes = std::filesystem::file_size(
				path.Native(), ec
			);
		}
		return result;
	}
}
