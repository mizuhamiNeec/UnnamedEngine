#pragma once

#include <optional>
#include <string_view>

#include "core/assets/AssetID.h"
#include "core/filesystem/VirtualPath.h"

namespace Unnamed {
	class JsonReader;
}

namespace Unnamed::Gui {
	struct UiDeserializeContext;

	/// @brief UIが保持する解決済みテクスチャ参照です。
	struct UiTextureReference final {
		std::optional<VirtualPath> virtualPath;
		AssetID                   assetId = kInvalidAssetID;
	};

	/// @brief JSONフィールドからUIテクスチャ参照を検証してロードします。
	/// @return strict失敗時はfalse、成功またはpermissive継続時はtrue。
	[[nodiscard]] bool DeserializeUiTextureReference(
		const JsonReader&           reader,
		std::string_view            fieldName,
		const UiDeserializeContext& context,
		UiTextureReference&         output
	);

	/// @brief 論理パスを検証してUIテクスチャ参照をロードします。
	/// @return ロード成功時はtrue。失敗時はoutputを未設定にします。
	[[nodiscard]] bool LoadUiTextureReference(
		std::string_view            pathValue,
		const UiDeserializeContext& context,
		UiTextureReference&         output
	);
}
