#pragma once

#include "engine/content/AssetReferenceValidationPolicy.h"

namespace Unnamed {
	class AssetManager;
}

namespace Unnamed::Gui {
	/// @brief UI Documentのデシリアライズに必要なサービスと方針です。
	struct UiDeserializeContext final {
		AssetManager& assetManager;
		AssetReferenceValidationPolicy assetValidationPolicy =
			AssetReferenceValidationPolicy::Permissive;
	};
}
