#include "UiTextureReference.h"

#include "UiDeserializeContext.h"

#include "core/assets/AssetManager.h"
#include "core/io/json/JsonReader.h"

#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed::Gui {
	bool LoadUiTextureReference(
		const std::string_view      pathValue,
		const UiDeserializeContext& context,
		UiTextureReference&         output
	) {
		output = {};

		const std::optional<VirtualPath> virtualPath =
			VirtualPath::ParseContentReference(pathValue);
		if (!virtualPath.has_value()) {
			Error("UI", "Invalid UI texture virtual path: {}", pathValue);
			return false;
		}

		const AssetID assetId = context.assetManager.LoadTexture(*virtualPath);
		if (assetId == kInvalidAssetID) {
			Error("UI", "Failed to load UI texture: {}", virtualPath->String());
			return false;
		}

		output.virtualPath = *virtualPath;
		output.assetId     = assetId;
		return true;
	}

	bool DeserializeUiTextureReference(
		const JsonReader&           reader,
		const std::string_view      fieldName,
		const UiDeserializeContext& context,
		UiTextureReference&         output
	) {
		output = {};
		if (!reader.Has(fieldName)) {
			return true;
		}

		const JsonReader pathNode = reader[fieldName];
		if (!pathNode.IsString()) {
			Error("UI", "UI texture field '{}' must be a string.", fieldName);
			return !IsStrictAssetValidation(context.assetValidationPolicy);
		}

		if (LoadUiTextureReference(pathNode.GetString(), context, output)) {
			return true;
		}

		return !IsStrictAssetValidation(context.assetValidationPolicy);
	}
}
