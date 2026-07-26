#pragma once

#include <string>

#include <json.hpp>

namespace Unnamed {
	/// @brief UiDocumentAssetDataは、UiDocumentAsset assetのdecode結果をruntime生成処理へ渡す中間表現として保持します
	struct UiDocumentAssetData {
		std::string    name;
		nlohmann::json rootJson;
	};
}
