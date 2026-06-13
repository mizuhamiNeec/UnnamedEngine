
#include "UiDocumentAssetLoader.h"

#include <fstream>

#include "core/path/PathUtil.h"
#include "core/string/StrUtil.h"

namespace Unnamed {
	bool UiDocumentAssetLoader::CanLoad(
		std::string_view path, ASSET_TYPE* outType
	) const {
		const bool ok = StrUtil::ToLowerCase(std::string(path)).ends_with(
			".ui.json"
		);
		if (outType) {
			*outType = ok ? ASSET_TYPE::UI_DOCUMENT : ASSET_TYPE::UNKNOWN;
		}
		return ok;
	}

	LoadResult UiDocumentAssetLoader::Load(const std::string& path) {
		LoadResult    result = {};
		std::ifstream ifs(Path::FromUtf8(path));
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

		const std::filesystem::path full = Path::FromUtf8(path);
		UiDocumentAssetData         data = {};
		data.name = root.value("name", Path::ToUtf8String(full.filename()));
		data.rootJson = std::move(root);

		result.payload     = std::move(data);
		result.resolveName = Path::ToUtf8String(full.stem().stem());

		std::error_code ec;
		if (Path::ExistsUtf8(path, ec)) {
			result.stamp.sizeInBytes = Path::FileSizeUtf8(path, ec);
		}
		return result;
	}
}
